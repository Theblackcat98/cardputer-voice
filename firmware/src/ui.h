// Tiny screen UI for the 240x135 display.
#pragma once

#include "net.h"

namespace ui {

// Generic centered message screen (boot, wifi, errors).
void message(const char* title, const char* body);

// STATUS screen.
void drawStatus(const StatusData& s);  // s.ok == false -> "waiting for data"
void drawStatusError();                // poll failed (keeps last good frame dimmed)

// VOICE screen.
void drawVoiceIdle();                        // "hold SPACE to talk"
void drawVoiceState(const char* state);       // recording / thinking / speaking...
void drawVoiceResult(const char* transcript, const char* reply);

}  // namespace ui
