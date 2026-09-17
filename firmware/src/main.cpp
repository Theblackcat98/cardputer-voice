// cardputer-voice — Cardputer thin client: status dashboard + push-to-talk voice.
//
// Screens:
//   STATUS — idle dashboard, polls GET /status every STATUS_POLL_MS.
//   VOICE  — hold SPACE to record, release to send; reply plays back.
//
// Controls: ENTER -> voice screen, Q -> status screen, SPACE (hold) -> talk.

#include <M5Cardputer.h>
#include <WiFi.h>

#include "config.h"
#include "net.h"
#include "ui.h"
#include "voice.h"

enum class Screen { STATUS, VOICE };

static Screen screen = Screen::STATUS;
static unsigned long lastPoll = 0;
static StatusData status{};

// --- helpers ---------------------------------------------------------------

static bool keyDown(char c) {
  // isKeyPressed(char) is the stable M5Cardputer keyboard API; it reflects
  // the held state as long as update() is called every loop.
  return M5Cardputer.Keyboard.isKeyPressed(c);
}

static void connectWifi() {
  ui::message("WiFi", "Connecting...");
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  unsigned long t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 15000) {
    delay(250);
  }
  if (WiFi.status() == WL_CONNECTED) {
    ui::message("WiFi OK", WiFi.localIP().toString().c_str());
  } else {
    ui::message("WiFi FAILED", "check config.h");
  }
  delay(1200);
}

// Push-to-talk predicate: true while SPACE is physically held.
static bool pttHeld() {
  M5Cardputer.update();
  return keyDown(' ');
}

// --- arduino entry points ---------------------------------------------------

void setup() {
  auto cfg = M5.config();
  M5Cardputer.begin(cfg, true);  // true = enable keyboard
  M5Cardputer.Display.setRotation(1);

  // Audio peripherals. Verify begin()/record()/playRaw() against the
  // installed M5Unified version if these fail to compile.
  M5Cardputer.Speaker.begin();
  M5Cardputer.Speaker.setVolume(180);
  M5Cardputer.Mic.begin();

  connectWifi();
  ui::drawStatus(status);  // shows "no data" until first poll
  lastPoll = millis() - STATUS_POLL_MS;  // poll immediately
}

void loop() {
  M5Cardputer.update();

  if (screen == Screen::STATUS) {
    if (keyDown(KEY_ENTER)) {
      screen = Screen::VOICE;
      ui::drawVoiceIdle();
      delay(250);  // debounce
      return;
    }
    if (millis() - lastPoll >= STATUS_POLL_MS) {
      lastPoll = millis();
      if (fetchStatus(SERVER_HOST, SERVER_PORT, status)) {
        ui::drawStatus(status);
      } else {
        ui::drawStatusError();
      }
    }
  } else {  // Screen::VOICE
    if (keyDown('q') || keyDown('Q')) {
      screen = Screen::STATUS;
      ui::drawStatus(status);
      delay(250);
      return;
    }
    // SPACE idle-poll: start an exchange on press edge.
    static bool wasDown = false;
    bool down = keyDown(' ');
    if (down && !wasDown) {
      // pttHeld() keeps update() pumping so the key state stays fresh.
      bool ok = voiceExchange(SERVER_HOST, SERVER_PORT, pttHeld,
                              ui::drawVoiceState);
      ui::drawVoiceIdle();
      if (!ok) {
        ui::drawVoiceState("error - see serial");
        delay(1500);
        ui::drawVoiceIdle();
      }
    }
    wasDown = down;
  }

  delay(20);
}
