"""Cardputer voice companion server.

Endpoints:
  POST /voice      WAV body (16 kHz, mono, 16-bit PCM) -> JSON
                   {transcript, reply, audio_url}
  GET  /audio/{id}  synthesized reply speech (WAV 16 kHz mono)
  GET  /status      host telemetry JSON for the Cardputer dashboard
"""
from __future__ import annotations

import re
import secrets
import threading
import time
import wave
from pathlib import Path

import yaml
from fastapi import Request
from fastapi.responses import FileResponse, JSONResponse
from fastapi import FastAPI

from . import llm, stats, stt, tts

BASE = Path(__file__).resolve().parent.parent
CFG_PATH = BASE / "config.yaml"
AUDIO_DIR = BASE / "audio_tmp"
AUDIO_RE = re.compile(r"[0-9a-f]{16}\.wav")

if not CFG_PATH.exists():
    raise SystemExit(
        "config.yaml not found — copy config.yaml.example to config.yaml and edit it."
    )
cfg = yaml.safe_load(CFG_PATH.read_text())
AUDIO_DIR.mkdir(exist_ok=True)

app = FastAPI(title="cardputer-voice-server")


def _write_wav(path: Path, pcm: bytes, rate: int = 16000) -> None:
    with wave.open(str(path), "wb") as f:
        f.setnchannels(1)
        f.setsampwidth(2)
        f.setframerate(rate)
        f.writeframes(pcm)


def _cleaner(ttl_s: int) -> None:
    while True:
        time.sleep(60)
        now = time.time()
        for p in AUDIO_DIR.glob("*.wav"):
            try:
                if now - p.stat().st_mtime > ttl_s:
                    p.unlink()
            except OSError:
                pass


threading.Thread(
    target=_cleaner,
    args=(int(cfg["server"].get("audio_ttl_minutes", 10)) * 60,),
    daemon=True,
).start()


@app.post("/voice")
async def voice(request: Request):
    body = await request.body()
    if len(body) < 48:
        return JSONResponse({"error": "empty audio"}, status_code=400)
    # Firmware sends a WAV with placeholder sizes; strip the 44-byte header
    # if present, otherwise treat the body as raw PCM.
    pcm = body[44:] if body[:4] == b"RIFF" else body
    if len(pcm) < 3200:  # less than ~0.1 s of audio
        return JSONResponse({"error": "audio too short"}, status_code=400)

    in_path = AUDIO_DIR / f"in-{secrets.token_hex(8)}.wav"
    _write_wav(in_path, pcm)
    try:
        transcript = stt.transcribe(cfg, str(in_path)).strip()
    finally:
        in_path.unlink(missing_ok=True)

    if not transcript:
        transcript, reply = "", "I didn't catch that."
    else:
        try:
            reply = llm.chat(cfg, transcript).strip()
        except Exception as exc:  # llama.cpp down etc. — still answer something
            reply = f"Sorry, I couldn't reach the language model: {exc}"
        if not reply:
            reply = "Sorry, I couldn't come up with a reply."

    out_name = f"{secrets.token_hex(8)}.wav"
    tts.synthesize(cfg, reply, str(AUDIO_DIR / out_name))
    return {"transcript": transcript, "reply": reply,
            "audio_url": f"/audio/{out_name}"}


@app.get("/audio/{name}")
async def audio(name: str):
    if not AUDIO_RE.fullmatch(name):
        return JSONResponse({"error": "not found"}, status_code=404)
    path = AUDIO_DIR / name
    if not path.is_file():
        return JSONResponse({"error": "expired"}, status_code=404)
    return FileResponse(path, media_type="audio/wav")


@app.get("/status")
async def status():
    return stats.snapshot(cfg)


def run() -> None:
    import uvicorn

    s = cfg["server"]
    uvicorn.run(app, host=s.get("host", "0.0.0.0"),
                port=int(s.get("port", 8000)))
