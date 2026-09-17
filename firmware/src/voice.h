// Push-to-talk voice exchange with the companion server.
//
// Records from the mic while continueFn() returns true (capped at
// RECORD_MAX_SECS), streams the audio as a chunked WAV to POST /voice,
// parses the JSON reply {transcript, reply, audio_url}, shows the texts,
// then streams GET <audio_url> straight into the speaker.
//
// No full recording is ever held in RAM: ~4 KB chunks both ways.
// onState() receives short human-readable phase updates for the screen.
#pragma once

#include <Arduino.h>

bool voiceExchange(const char* host, uint16_t port,
                   bool (*continueFn)(), void (*onState)(const char*));
