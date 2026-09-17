#include "net.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>

bool fetchStatus(const char* host, uint16_t port, StatusData& out) {
  HTTPClient http;
  char url[160];
  snprintf(url, sizeof url, "http://%s:%u/status", host, port);
  http.setTimeout(4000);
  if (!http.begin(url)) return false;

  int code = http.GET();
  if (code != 200) {
    http.end();
    return false;
  }
  String body = http.getString();
  http.end();

  JsonDocument doc;
  if (deserializeJson(doc, body)) return false;

  out.ok = true;
  out.gpuTempC = doc["gpu"]["temp_c"] | -1.0f;
  out.gpuUtilPct = doc["gpu"]["util_pct"] | 0.0f;
  out.cpuPct = doc["cpu_pct"] | 0.0f;
  out.memPct = doc["mem_pct"] | 0.0f;
  out.llamaOk = doc["llamacpp"]["ok"] | false;
  const char* model = doc["llamacpp"]["model"] | "";
  out.llamaModel = String(model);
  return true;
}
