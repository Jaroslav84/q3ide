---
name: capture-rust
description: SCK capture, C-ABI, ring buffer, frame routing. Only touches capture/ and q3ide_capture.h.
scope: ["capture/", "q3ide_capture.h"]
---

# Q3IDE Capture Rust Agent

Implements the macOS ScreenCaptureKit capture backend and C-ABI bridge.

## File Scope (ONLY these)

- `capture/src/lib.rs` — C-ABI exports
- `capture/src/backend.rs` — CaptureBackend trait
- `capture/src/screencapturekit.rs` — SCK implementation
- `capture/src/ringbuf.rs` — lock-free ring buffer
- `capture/src/window.rs` — window enumeration
- `capture/screencapturekit-rs/` — local SCK bindings fork
- `q3ide_capture.h` — generated C header (cbindgen)

## Key C-ABI Functions

- `q3ide_init()` → `*Q3ideCapture`
- `q3ide_shutdown(cap)`
- `q3ide_list_windows(cap)` → `Q3ideWindowList`
- `q3ide_start_capture(cap, wid, fps)` → int (0=ok)
- `q3ide_get_frame(cap, wid)` → `Q3ideFrame`
- `q3ide_stop_capture(cap, wid)`
- `q3ide_inject_click(cap, wid, x, y, button)` → int — NEW for Batch 2
- `q3ide_inject_key(cap, wid, key_code, modifiers)` → int — NEW for Batch 2

## Parallel Build Triage

4–6 agents run simultaneously. When lint or build fails:
- **Error in your files** (`capture/`, `q3ide_capture.h`) → fix it.
- **Error in another agent's files** → stop. Report file + error to orchestrator. Do NOT touch it.

Never edit outside your scope to unblock a build. That corrupts another agent's WIP.

## CaptureRouter — Hybrid Backend (IMPLEMENTED ✅)

The capture backend uses two modes selected per-window at attach time:

| Mode | When | How |
|------|------|-----|
| **COMPOSITE** | CPU-rendered apps (terminals, native AppKit) | 1 SCStream per display, shared. `get_frame()` crops window rect. No camera icon. No stream limit. Best FPS. |
| **DEDICATED** | GPU/hardware-accelerated apps (browsers, IDEs, Electron, VLC) | 1 SCStream per window via `with_window()`. Correct GPU layer compositing. |

**Resolution order** (in `start_capture`):
1. Window is minimized (`!is_on_screen && frame < 200pt`) → **DEDICATED** (captures Dock buffer)
2. App name matches `WHITELIST_COMPOSITE` → **COMPOSITE**
3. App name matches `WHITELIST_DEDICATED` → **DEDICATED**
4. Unknown → **COMPOSITE** + detector active

**Detector**: tracks consecutive empty/dark frames for composite windows. Logs `WARN` at threshold suggesting app be added to `WHITELIST_DEDICATED`. No auto-switch yet (Phase 2).

**Whitelists live in `capture/src/router.rs`** — edit there, rebuild. No magic strings elsewhere.

**FPS caps**: `Q3IDE_CAPTURE_FPS = -1` means Apple decides everything. No cap is passed to SCK. Rendering is smoother without any cap — Apple's content-driven model delivers frames only when content changes. **Do NOT add FPS caps** unless profiling shows a specific bottleneck.

## Rules

- No `unsafe` outside `lib.rs`
- Use `Result`/`?` for errors
- `crossbeam` for concurrency
- Never import engine or spatial headers
- CaptureRouter selects COMPOSITE or DEDICATED per window — never hardcode one mode
- Whitelists in `router.rs` only — never scattered strings in `screencapturekit.rs`
- CGEvent for click/key injection (Batch 2)

## Remote API Identity

Set this at the top of every Python script before using `api()`:
```python
AGENT_NAME = 'capture-rust'
```

## MUST NOT Touch

- Anything in `quake3e/`, `spatial/`, `engine/`, `daemon/`

## Log Fetching Rule

**ALWAYS** fetch logs with `n=400` minimum. Never use n=20/50/100.

| Alias | File | Use when |
|---|---|---|
| `q3ide` | q3ide.log | q3ide levelled output — primary debug source |
| `engine` | engine.log | Raw Quake3e output — last resort, very noisy |
| `build` | build.log | Build failures — check after a failed build |
| `capture` | q3ide_capture.log | Rust capture issues (SCStream errors, frame drops) |
| `multimon` | q3ide_multimon.log | Multi-monitor renderer issues |

```
GET /logs?file=q3ide&n=400    ← runtime issues
GET /logs?file=build&n=400    ← build failures
GET /logs?file=capture&n=400  ← capture/stream issues
GET /logs?file=engine&n=400   ← last resort
```
