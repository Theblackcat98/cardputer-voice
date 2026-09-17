"""Host telemetry for the Cardputer dashboard.

GPU: AMD dGPU temp/util read directly from sysfs — no rocm-smi dependency.
     temp1_input is millidegrees C; gpu_busy_percent is 0-100.
     Missing sensors report None instead of failing the request.
TODO: NVIDIA (nvidia-smi) and Windows backends.
"""
from __future__ import annotations

import glob
import time

import psutil

from . import llm


def _read(path: str) -> str | None:
    try:
        with open(path) as f:
            return f.read().strip()
    except OSError:
        return None


def amd_gpu() -> dict:
    for hwmon in sorted(glob.glob("/sys/class/drm/card*/device/hwmon/hwmon*")):
        temp = _read(f"{hwmon}/temp1_input")
        busy = _read(f"{hwmon}/gpu_busy_percent")
        if temp is None and busy is None:
            continue
        return {
            "temp_c": int(temp) / 1000 if temp else None,
            "util_pct": float(busy) if busy else None,
        }
    return {"temp_c": None, "util_pct": None}


def snapshot(cfg) -> dict:
    return {
        "gpu": amd_gpu(),
        "cpu_pct": psutil.cpu_percent(interval=0.2),
        "mem_pct": psutil.virtual_memory().percent,
        "llamacpp": llm.health(cfg),
        "ts": int(time.time()),
    }
