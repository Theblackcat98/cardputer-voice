// Minimal HTTP client helpers for the companion server API.
#pragma once

#include <Arduino.h>

struct StatusData {
  bool ok = false;
  float gpuTempC = -1;    // -1 when sensor unavailable
  float gpuUtilPct = 0;
  float cpuPct = 0;
  float memPct = 0;
  bool llamaOk = false;
  String llamaModel;
};

// GET http://host:port/status -> out. Returns false on any failure.
bool fetchStatus(const char* host, uint16_t port, StatusData& out);
