#include "voice.h"

#include <M5Cardputer.h>
#include <WiFi.h>
#include <ArduinoJson.h>

#include "config.h"
#include "ui.h"

namespace {

constexpr size_t CHUNK_SAMPLES = 2048;
constexpr size_t CHUNK_BYTES = CHUNK_SAMPLES * sizeof(int16_t);

// 44-byte PCM WAV header, 16 kHz mono 16-bit. Sizes are placeholders — the
// server ignores them and treats everything after byte 44 as raw PCM.
void wavHeader(uint8_t h[44]) {
  memset(h, 0, 44);
  memcpy(h, "RIFF", 4);
  memcpy(h + 8, "WAVEfmt ", 8);
  h[16] = 16;  // fmt chunk size
  h[20] = 1;   // PCM
  h[22] = 1;   // mono
  h[24] = (SAMPLE_RATE_HZ & 0xFF);
  h[25] = ((SAMPLE_RATE_HZ >> 8) & 0xFF);
  uint32_t br = (uint32_t)SAMPLE_RATE_HZ * 2;  // byte rate
  h[28] = br & 0xFF;
  h[29] = (br >> 8) & 0xFF;
  h[30] = (br >> 16) & 0xFF;
  h[31] = (br >> 24) & 0xFF;
  h[32] = 2;   // block align
  h[34] = 16;  // bits per sample
  memcpy(h + 36, "data", 4);
}

void sendChunk(WiFiClient& c, const uint8_t* data, size_t len) {
  char hex[10];
  snprintf(hex, sizeof hex, "%X\r\n", (unsigned)len);
  c.write((const uint8_t*)hex, strlen(hex));
  c.write(data, len);
  c.write((const uint8_t*)"\r\n", 2);
}

void sendLastChunk(WiFiClient& c) { c.write((const uint8_t*)"0\r\n\r\n", 5); }

String readLine(WiFiClient& c) {
  String s = c.readStringUntil('\n');
  s.trim();
  return s;
}

// Consume HTTP response headers; return Content-Length or -1.
long headerContentLength(WiFiClient& c) {
  long len = -1;
  while (true) {
    String line = readLine(c);
    if (line.length() == 0) break;
    int colon = line.indexOf(':');
    if (colon > 0) {
      String name = line.substring(0, colon);
      name.trim();
      name.toLowerCase();
      if (name == "content-length") len = line.substring(colon + 1).toInt();
    }
  }
  return len;
}

bool readExact(WiFiClient& c, uint8_t* buf, size_t n,
               unsigned long timeoutMs = 15000) {
  size_t got = 0;
  unsigned long t0 = millis();
  while (got < n) {
    if (millis() - t0 > timeoutMs) return false;
    int avail = c.available();
    if (avail > 0) {
      size_t want = n - got;
      if ((size_t)avail < want) want = avail;
      int r = c.read(buf + got, want);
      if (r > 0) {
        got += r;
        t0 = millis();
      }
    } else {
      delay(2);
    }
  }
  return true;
}

}  // namespace

bool voiceExchange(const char* host, uint16_t port,
                   bool (*continueFn)(), void (*onState)(const char*)) {
  // ---- 1. record + chunked POST /voice ------------------------------------
  onState("recording...");
  WiFiClient c;
  c.setTimeout(5000);
  if (!c.connect(host, port)) {
    Serial.println("voice: connect failed");
    return false;
  }

  c.print(String("POST /voice HTTP/1.1\r\nHost: ") + host + "\r\n" +
          "Content-Type: audio/wav\r\n"
          "Transfer-Encoding: chunked\r\n"
          "Connection: close\r\n\r\n");

  uint8_t hdr[44];
  wavHeader(hdr);
  sendChunk(c, hdr, sizeof hdr);

  int16_t pcm[CHUNK_SAMPLES];
  unsigned long t0 = millis();
  unsigned long lastUi = 0;
  size_t chunks = 0;
  while (continueFn() &&
         millis() - t0 < (unsigned long)RECORD_MAX_SECS * 1000UL) {
    // Verify against the installed M5Unified: record(int16_t*, size_t, uint32_t)
    if (!M5Cardputer.Mic.record(pcm, CHUNK_SAMPLES, SAMPLE_RATE_HZ)) break;
    sendChunk(c, (const uint8_t*)pcm, CHUNK_BYTES);
    if (++chunks % 4 == 0 && millis() - lastUi > 400) {
      lastUi = millis();
      char s[32];
      snprintf(s, sizeof s, "recording %.0fs", (millis() - t0) / 1000.0);
      onState(s);
    }
  }
  sendLastChunk(c);

  // ---- 2. JSON reply: {transcript, reply, audio_url} ------------------------
  String statusLine = readLine(c);
  if (!statusLine.startsWith("HTTP/") || statusLine.indexOf(" 200 ") < 0) {
    Serial.printf("voice: bad status: %s\n", statusLine.c_str());
    c.stop();
    return false;
  }
  long len = headerContentLength(c);
  if (len <= 0 || len > 8192) {
    c.stop();
    return false;
  }
  uint8_t* body = (uint8_t*)malloc(len + 1);
  if (!body) {
    c.stop();
    return false;
  }
  bool ok = readExact(c, body, len);
  c.stop();
  if (!ok) {
    free(body);
    return false;
  }
  body[len] = '\0';

  JsonDocument doc;
  bool parseOk = !deserializeJson(doc, (const char*)body);
  String transcript = doc["transcript"] | "";
  String reply = doc["reply"] | "";
  String audioUrl = doc["audio_url"] | "";
  free(body);
  if (!parseOk || audioUrl.length() == 0) return false;

  ui::drawVoiceResult(transcript.c_str(), reply.c_str());

  // ---- 3. stream reply WAV into the speaker --------------------------------
  WiFiClient a;
  a.setTimeout(5000);
  if (!a.connect(host, port)) return true;  // text shown; audio is optional
  a.print(String("GET ") + audioUrl + " HTTP/1.1\r\nHost: " + host +
          "\r\nConnection: close\r\n\r\n");
  String sl = readLine(a);
  if (!sl.startsWith("HTTP/") || sl.indexOf(" 200 ") < 0) {
    a.stop();
    return true;
  }
  long alen = headerContentLength(a);
  if (alen <= 44) {
    a.stop();
    return true;
  }
  uint8_t wh[44];  // skip WAV header
  if (!readExact(a, wh, 44)) {
    a.stop();
    return true;
  }
  size_t remaining = (size_t)alen - 44;
  uint8_t abuf[4096];
  while (remaining > 0) {
    size_t want = remaining > sizeof abuf ? sizeof abuf : remaining;
    if (!readExact(a, abuf, want)) break;
    // Verify against the installed M5Unified:
    //   playRaw(const int16_t*, size_t, uint32_t, bool)
    M5Cardputer.Speaker.playRaw((const int16_t*)abuf, want / 2,
                                SAMPLE_RATE_HZ, false);
    remaining -= want;
  }
  a.stop();
  return true;
}
