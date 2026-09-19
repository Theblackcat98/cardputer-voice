"""Canned backends for mock mode.

Set `mock: true` in config.yaml to run the whole pipeline without
faster-whisper, llama.cpp, or piper installed: STT returns a fixed
transcript, the LLM echoes it back, and TTS writes a short beep so
`GET /audio/<id>.wav` still returns a playable 16 kHz mono WAV.
"""
from __future__ import annotations

import math
import struct
import wave


def _opts(cfg) -> dict:
    m = cfg.get("mock", {})
    return m if isinstance(m, dict) else {}


def transcribe(cfg, wav_path: str) -> str:
    return _opts(cfg).get(
        "transcript", "hello from the mock speech recognizer"
    )


def chat(cfg, user_text: str) -> str:
    default = f'Mock reply to: "{user_text}"'
    return _opts(cfg).get("reply", default)


def synthesize(cfg, text: str, out_wav: str) -> None:
    rate = 16000
    dur = 0.8  # seconds
    n = int(rate * dur)
    # 440 Hz sine with fade in/out so it doesn't click
    frames = bytearray()
    for i in range(n):
        env = math.sin(math.pi * i / n)
        sample = int(12000 * env * math.sin(2 * math.pi * 440 * i / rate))
        frames += struct.pack("<h", sample)
    with wave.open(out_wav, "wb") as f:
        f.setnchannels(1)
        f.setsampwidth(2)
        f.setframerate(rate)
        f.writeframes(bytes(frames))
