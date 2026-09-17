"""llama.cpp OpenAI-compatible chat client + health check."""
from __future__ import annotations

import httpx


def _base(cfg) -> str:
    return cfg["llamacpp"]["base_url"].rstrip("/")


def chat(cfg, user_text: str) -> str:
    lc = cfg["llamacpp"]
    payload = {
        "model": lc.get("model", ""),
        "messages": [
            {"role": "system", "content": lc.get("system_prompt", "")},
            {"role": "user", "content": user_text},
        ],
        "max_tokens": lc.get("max_tokens", 200),
        "temperature": lc.get("temperature", 0.7),
        "stream": False,
    }
    r = httpx.post(f"{_base(cfg)}/v1/chat/completions", json=payload,
                   timeout=120)
    r.raise_for_status()
    return r.json()["choices"][0]["message"]["content"]


def health(cfg) -> dict:
    lc = cfg["llamacpp"]
    try:
        r = httpx.get(f"{_base(cfg)}/health", timeout=3)
        ok = r.status_code == 200
    except Exception:
        ok = False
    return {"ok": ok, "model": lc.get("model", "")}
