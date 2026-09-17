"""Piper TTS. Voice loads lazily on first use.

Download a voice (.onnx + .onnx.json) from
https://github.com/rhasspy/piper/releases and point tts.voice at it.
"""
from __future__ import annotations

import wave

from piper import PiperVoice

_voice: PiperVoice | None = None


def _get(cfg) -> PiperVoice:
    global _voice
    if _voice is None:
        t = cfg["tts"]
        _voice = PiperVoice.load(t["voice"], config_path=t.get("voice_config"))
    return _voice


def synthesize(cfg, text: str, out_wav: str) -> None:
    voice = _get(cfg)
    with wave.open(out_wav, "wb") as f:
        f.setnchannels(1)
        f.setsampwidth(2)
        f.setframerate(voice.config.sample_rate)
        voice.synthesize(text, f)
