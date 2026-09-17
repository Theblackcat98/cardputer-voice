#include "ui.h"

#include <M5Cardputer.h>

namespace ui {
namespace {

constexpr uint16_t BG = TFT_BLACK;
constexpr uint16_t FG = TFT_WHITE;
constexpr uint16_t DIM = TFT_DARKGREY;
constexpr uint16_t ACCENT = TFT_CYAN;
constexpr uint16_t GOOD = TFT_GREEN;
constexpr uint16_t BAD = TFT_RED;

void header(const char* title) {
  auto& d = M5Cardputer.Display;
  d.fillScreen(BG);
  d.setTextSize(2);
  d.setTextColor(ACCENT, BG);
  d.setCursor(4, 4);
  d.println(title);
  d.drawFastHLine(0, 26, 240, DIM);
}

void body2(const char* l1, const char* l2 = nullptr, const char* l3 = nullptr,
           const char* l4 = nullptr) {
  auto& d = M5Cardputer.Display;
  d.setTextSize(2);
  d.setTextColor(FG, BG);
  int y = 34;
  for (const char* l : {l1, l2, l3, l4}) {
    if (!l) break;
    d.setCursor(4, y);
    d.println(l);
    y += 22;
  }
}

void footer(const char* hint) {
  auto& d = M5Cardputer.Display;
  d.setTextSize(1);
  d.setTextColor(DIM, BG);
  d.setCursor(4, 122);
  d.println(hint);
}

// Horizontal bar: x,y top-left, w px wide, pct 0-100.
void bar(int x, int y, int w, float pct, uint16_t color) {
  auto& d = M5Cardputer.Display;
  if (pct < 0) pct = 0;
  if (pct > 100) pct = 100;
  d.drawRect(x, y, w, 10, DIM);
  d.fillRect(x + 1, y + 1, (int)((w - 2) * pct / 100.0f), 8, color);
}

String fmtTemp(float t) {
  if (t < 0) return "--";
  char b[16];
  snprintf(b, sizeof b, "%.0fC", t);
  return String(b);
}

}  // namespace

void message(const char* title, const char* body) {
  header(title);
  body2(body);
}

void drawStatus(const StatusData& s) {
  auto& d = M5Cardputer.Display;
  header("SERVER");
  if (!s.ok) {
    body2("waiting for", "server...");
    footer("ENTER voice");
    return;
  }
  d.setTextSize(2);
  d.setTextColor(FG, BG);
  d.setCursor(4, 34);
  d.printf("GPU %s", fmtTemp(s.gpuTempC).c_str());
  bar(110, 36, 120, s.gpuUtilPct, TFT_ORANGE);

  d.setCursor(4, 58);
  d.printf("CPU %3.0f%%", s.cpuPct);
  d.setCursor(4, 80);
  d.printf("MEM %3.0f%%", s.memPct);

  d.setCursor(4, 102);
  d.setTextColor(s.llamaOk ? GOOD : BAD, BG);
  d.printf("llama.cpp %s", s.llamaOk ? "OK" : "DOWN");
  d.setTextColor(DIM, BG);

  footer("ENTER voice");
}

void drawStatusError() {
  auto& d = M5Cardputer.Display;
  d.setTextSize(1);
  d.setTextColor(BAD, BG);
  d.setCursor(180, 4);
  d.print("poll fail");
}

void drawVoiceIdle() {
  header("VOICE");
  body2("hold SPACE", "to talk", "", "Q: back");
  footer("mic -> server -> speaker");
}

void drawVoiceState(const char* state) {
  header("VOICE");
  body2(state);
  footer("Q: back");
}

// Word-wrap printer for the result screen. Returns the y after the block.
int wrapPrint(int x, int y, const char* label, const char* text) {
  auto& d = M5Cardputer.Display;
  constexpr int COLS = 38;  // text size 1 -> 6px per char on 240px
  char line[COLS + 1];
  size_t li = 0;
  auto emitLine = [&] {
    line[li] = '\0';
    if (y < 116) {
      d.setCursor(x, y);
      d.println(line);
      y += 10;
    }
    li = 0;
  };
  auto pushWord = [&](const char* w, size_t wl) {
    size_t wi = 0;
    while (wi < wl) {
      size_t need = (li == 0) ? 0 : 1;  // preceding space
      size_t room = (COLS > li + need) ? COLS - li - need : 0;
      if (room == 0) {
        emitLine();
        continue;
      }
      size_t take = wl - wi;
      if (take > room) take = room;
      if (li > 0) line[li++] = ' ';
      memcpy(line + li, w + wi, take);
      li += take;
      wi += take;
      if (li >= COLS) emitLine();
    }
  };
  auto pushText = [&](const char* t) {
    const char* p = t;
    while (*p) {
      while (*p == ' ' || *p == '\t' || *p == '\r') ++p;
      if (*p == '\n') {
        emitLine();
        ++p;
        continue;
      }
      if (!*p) break;
      const char* w = p;
      while (*p && *p != ' ' && *p != '\t' && *p != '\r' && *p != '\n') ++p;
      pushWord(w, p - w);
    }
  };
  pushWord(label, strlen(label));
  pushText(text);
  emitLine();
  return y;
}

void drawVoiceResult(const char* transcript, const char* reply) {
  header("VOICE");
  auto& d = M5Cardputer.Display;
  d.setTextSize(1);
  d.setTextColor(FG, BG);
  int y = wrapPrint(4, 32, "you:", transcript);
  wrapPrint(4, y + 4, "ai:", reply);
  footer("Q: back");
}

}  // namespace ui
