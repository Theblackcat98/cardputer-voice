# cardputer-voice

A voice assistant + server status dashboard for the M5Stack Cardputer.

The Cardputer is a thin client: push-to-talk voice capture, a small status
screen, and audio playback. All heavy work (speech recognition, LLM
inference, text-to-speech, host telemetry) runs on a companion Python
service on your PC/server, where the GPU actually lives.

```
                        WiFi (LAN)
  +------------------+   POST /voice (chunked WAV 16kHz mono)   +-------------------+
  |   Cardputer      |  --------------------------------------> |                   |
  |                  |                                          |  Companion server |
  |  [STATUS] idle   |  <-------------------------------------- |                   |
  |   GPU temp/util  |   JSON {transcript, reply, audio_url}    |  faster-whisper   |
  |   llama.cpp up?  |                                          |  llama.cpp server |
  |                  |   GET /audio/<id>.wav  (reply speech)    |  Piper TTS        |
  |  [VOICE] hold    |  --------------------------------------> |  rocm-smi/sysfs   |
  |   SPACE to talk  |                                          |  psutil           |
  +------------------+                                          +-------------------+
```

## Repo layout

```
cardputer-voice/
  firmware/                 # PlatformIO project for the Cardputer (ESP32-S3)
    platformio.ini
    src/
      main.cpp              # setup + screen state machine
      config.h.example      # WiFi + server address (copy to config.h, gitignored)
      ui.h / ui.cpp         # status + voice screen rendering
      net.h / net.cpp       # GET /status polling
      voice.h / voice.cpp   # push-to-talk: record -> POST /voice -> play reply
  server/                   # Companion service (Python, runs on your PC)
    pyproject.toml
    config.yaml.example     # whisper/llama.cpp/piper settings (copy to config.yaml)
    app/
      main.py               # FastAPI: POST /voice, GET /audio/{id}, GET /status
      stt.py                # faster-whisper wrapper
      llm.py                # llama.cpp OpenAI-compatible chat client
      tts.py                # Piper TTS wrapper
      stats.py              # GPU temp/util (AMD sysfs), CPU/RAM, llama.cpp health
```

## Quickstart — server

Requires Python 3.10+, and a [llama.cpp server](https://github.com/ggml-org/llama.cpp)
already running with `--host 0.0.0.0` (or at least reachable from the Cardputer's LAN).

```bash
cd server
python -m venv .venv && source .venv/bin/activate
pip install -e .                     # base deps only — enough for mock mode
# pip install -e ".[voice]"          # uncomment for real whisper/piper backends
cp config.yaml.example config.yaml   # edit: whisper model, llamacpp url, piper voice
# Download a Piper voice, e.g.:
#   https://github.com/rhasspy/piper/releases  -> en_US-lessac-medium.onnx (+ .json)
uvicorn app.main:app --host 0.0.0.0 --port 8000
```

**Mock mode:** set `mock: true` in `config.yaml` to run with canned responses —
no whisper, llama.cpp, or piper needed. `/voice` returns a fixed transcript,
echoes it as the reply, and `/audio/<id>.wav` is a short beep. Handy for
testing the Cardputer end-to-end before installing any models.

Check it: `curl http://<pc-ip>:8000/status`

## Quickstart — firmware

```bash
cd firmware
cp src/config.h.example src/config.h   # edit: WIFI_SSID, WIFI_PASSWORD, SERVER_HOST, SERVER_PORT
pio run                                # build
pio run --target upload                # flash (Cardputer in download mode: hold G0, power on)
pio device monitor                     # serial log, 115200
```

`platformio.ini` targets `m5stack-stamps3` (the StampS3 module inside the
original Cardputer). The `M5Cardputer` library auto-detects the Adv revision
at runtime, so the same build works on both. Arduino-IDE/CLI users can use
board `m5stack:esp32:m5stack_cardputer` instead.

## Using it

- Boot lands on the **STATUS** screen: GPU temp/util, CPU/RAM, llama.cpp
  reachability. Refreshes every 5 s.
- Press **ENTER** for the **VOICE** screen. Hold **SPACE** to talk, release to
  send. The transcript and reply print on screen while the spoken reply plays.
- Press **Q** to go back to STATUS.

## API contract

- `POST /voice` — body: WAV (16 kHz, mono, 16-bit PCM; firmware streams it
  with `Transfer-Encoding: chunked`, sizes in the header are placeholders).
  Returns JSON: `{"transcript": str, "reply": str, "audio_url": "/audio/<id>.wav"}`.
- `GET /audio/<id>.wav` — reply speech, WAV 16 kHz mono 16-bit. Files expire
  after 10 minutes.
- `GET /status` — JSON:
  `{"gpu": {"temp_c": 54.0, "util_pct": 12.0}, "cpu_pct": 23.1, "mem_pct": 41.7,
    "llamacpp": {"ok": true, "model": "..."}, "ts": 1726450000}`.
  Missing sensors report `null`, never fail the request.

## Design notes / honest limitations

- The Cardputer has no PSRAM and ~320 KB usable SRAM, so the firmware never
  holds a full recording: audio streams to the server in ~4 KB chunks and the
  reply streams back the same way. Max utterance is 12 s (configurable).
- Expect a few seconds of round-trip latency (STT + LLM generation + TTS).
  Fine for a voice assistant, not for snappy conversation.
- The MEMS mic is basic: quiet rooms work well, noisy rooms less so.
- WiFi + audio streaming drains the small battery in an hour or two of active
  use. This is a desk toy, not an all-day device.
- `faster-whisper` defaults to CPU (`int8`, `small` model) — plenty fast for
  short utterances. Set `device: cuda` in config if you have a CUDA build;
  ROCm wheels for ctranslate2 are spotty, so CPU is the safe default on AMD.
- GPU telemetry reads AMD sysfs (`/sys/class/drm/.../hwmon`) directly — no
  `rocm-smi` dependency, works for any AMD dGPU on Linux. NVIDIA/Windows
  support is a TODO (see stats.py).

## Roadmap

- [ ] Verify `M5Cardputer.Mic.record` / `Speaker.playRaw` signatures against the
      installed M5Unified version (marked in code) and do a first `pio run`.
- [ ] On-device VU meter while recording.
- [ ] Conversation memory: pass recent turns to llama.cpp (server-side, keyed
      by client).
- [ ] Interrupt playback by pressing SPACE (barge-in).
- [ ] NVIDIA (`nvidia-smi`) and Windows telemetry backends.
- [ ] Config portal (captive WiFi setup) instead of hardcoded `config.h`.
