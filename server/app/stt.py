"""faster-whisper speech-to-text. Model loads lazily on first use."""
from __future__ import annotations

from faster_whisper import WhisperModel

_model: WhisperModel | None = None


def _get(cfg) -> WhisperModel:
    global _model
    if _model is None:
        w = cfg["whisper"]
        _model = WhisperModel(
            w.get("model", "small"),
            device=w.get("device", "cpu"),
            compute_type=w.get("compute_type", "int8"),
        )
    return _model


def transcribe(cfg, wav_path: str) -> str:
    model = _get(cfg)
    # beam_size=1 (greedy) + VAD: fast and robust for short push-to-talk clips.
    segments, _info = model.transcribe(
        wav_path, language="en", beam_size=1, vad_filter=True
    )
    return "".join(s.text for s in segments).strip()
