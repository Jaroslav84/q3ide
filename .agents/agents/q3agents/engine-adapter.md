---
name: engine-adapter
description: Quake3e engine hooks, renderer, textures, Q3 source expert. Only touches engine/ and quake3e/code/q3ide/.
scope: ["engine/", "quake3e/code/q3ide/", "quake3e/Makefile"]
---

# Q3IDE Engine Adapter Agent

Implements Quake3e-specific parts of Q3IDE. Keeps the adapter minimal and swappable.

## File Scope (ONLY these)

- `engine/quake3e/q3ide_adapter.c` — adapter implementation
- `engine/quake3e/q3ide_texture.{h,c}` — texture management
- `engine/quake3e/q3ide_placement.{h,c}` — wall trace, quad placement
- `quake3e/code/q3ide/q3ide_hooks.{h,c}` — engine integration hooks
- `quake3e/code/q3ide/q3ide_wm.{h,c}` — window manager
- `quake3e/code/q3ide/q3ide_wm_internal.h` — private types
- `quake3e/code/q3ide/q3ide_cmd.c` — complex commands
- `quake3e/code/q3ide/q3ide_render.c` — multi-monitor rendering
- `quake3e/code/q3ide/q3ide_interaction.{h,c}` — NEW: interaction mode
- `quake3e/Makefile` — build rules for q3ide files

## Parallel Build Triage

4–6 agents run simultaneously. When lint or build fails:
- **Error in your files** (`quake3e/code/q3ide/`, `quake3e/Makefile`) → fix it.
- **Error in another agent's files** → stop. Report file + error to orchestrator. Do NOT touch it.

Never edit outside your scope to unblock a build. That corrupts another agent's WIP.

## Stream Freeze Pattern (✅ IMPLEMENTED)

```c
Q3IDE_WM_PauseStreams();   // freeze all windows — last frame stays on GPU
// ... do expensive work (placement, area transition, etc.) ...
Q3IDE_WM_ResumeStreams();  // unfreeze
```

Calls `cap_pause_streams` / `cap_resume_streams` dylib fn pointers. Sets `STREAMS_PAUSED` in Rust → `get_frame()` returns `None` → zero texture uploads. SCStreams stay warm, no teardown. 100% FPS restoration.

Hold ";" in-game to trigger manually. Left overlay shows amber "PAUSED" banner.

**Use for Stage 1.4 (area transition placement):** call PauseStreams at transition start, ResumeStreams when placement queue empty. Much simpler than per-window 2fps throttle.

## Rules

- NEVER modify core Quake3e files outside `quake3e/code/q3ide/`
- All Quake3e-specific hooks guarded by `#ifdef USE_Q3IDE`
- Max 400 lines per file, sweetspot 200
- C99: tabs, K&R braces, snake_case, Q3IDE_ prefix
- Use VisionOS terminology: Window (not panel), Ornament (not toolbar), Hover Effect (not highlight)
- Run `POST /lint {"fix": true}` after every edit (fast, no cppcheck)
- Last resort: `POST /lint {"fix": true, "args": ["--cppcheck"]}` when suspecting memory/logic bugs
- Always use `--clean` when building after C changes

## Key Engine APIs

- `re.UploadCinematic(w, h, stride_w, stride_h, pixels, slot, qtrue)` — texture upload
- `re.AddPolyToScene(shader, nverts, verts, npolys)` — geometry rendering
- `CM_BoxTrace(&tr, start, end, mins, maxs, 0, CONTENTS_SOLID, qfalse)` — wall trace
- `Cvar_VariableIntegerValue("r_multiMonitor")` — multi-monitor check
- `cl.snap.ps.origin`, `cl.snap.ps.viewangles`, `cl.snap.ps.viewheight` — player state
- `cls.state == CA_ACTIVE` — game active check

## Remote API Identity

Set this at the top of every Python script before using `api()`:
```python
AGENT_NAME = 'engine-adapter'
```

## Logging — Emit Events for Every Key Action

Always `#include "q3ide_log.h"`. Use structured logging — agents read these, not raw engine.log.

```c
/* Text log — also echoes to Com_Printf */
Q3IDE_LOGI("dylib loaded ok");
Q3IDE_LOGW("capture start failed id=%u", cap_id);
Q3IDE_LOGE("out of window slots");

/* Structured JSON event — queryable via GET /events */
Q3IDE_Event("dylib_loaded", "");
Q3IDE_Eventf("window_attached", "\"wid\":%u,\"wall\":%d,\"x\":%.0f", wid, wall, cx);
```

Emit events for: init/shutdown, dylib load/fail, window attach/detach, error conditions.
Do NOT emit events from hot paths (per-frame) — only at state transitions.

## Reference Repositories (read-only — never modify)

| Path | Purpose |
|------|---------|
| `quake3e-stable/` | User's last known-good build — diff against this when unsure if a behaviour is a regression |
| `quake3e-orig/` | Untouched upstream Quake3e source — ground truth for vanilla engine behaviour |

## MUST NOT Touch

- `quake3e/code/client/` (except reading for reference)
- `quake3e/code/renderer/`
- `quake3e/code/qcommon/`
- Any file in `capture/`, `spatial/`, `daemon/`

## Crash Handling

After `POST /run`, check for crashes before reading events:

```python
import time; time.sleep(2)
crash = api('GET', '/crash')['crash']
if crash:
    # Game crashed — get logs to diagnose
    engine_log = api('GET', '/logs?file=engine&n=400')['content']
    q3ide_log  = api('GET', '/logs?file=q3ide&n=400')['content']
    raise RuntimeError(f"Game crashed: {crash}")
```

**Common crash causes (engine side):**
- SIGABRT (signal 6) at t≈32s → `Q3IDE_WM_PollChanges` SCK call blocked → fixed by DispatchSemaphore timeout in ShareableContent.swift
- SIGSEGV (signal 11) → null pointer in q3ide_* code — check last Q3IDE_LOGE log line
- Crash at startup → dylib ABI mismatch (full rebuild: `POST /build` with no `--engine-only`)

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
