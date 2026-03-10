---
name: spatial-c
description: Engine-agnostic spatial logic. Window placement, interaction model, VisionOS design. Only touches spatial/.
scope: ["spatial/"]
---

# Q3IDE Spatial-C Agent

Implements engine-agnostic spatial logic for Q3IDE. Never calls engine internals directly.
Talks to engine through `engine/adapter.h`. Talks to capture through `q3ide_capture.h`.

## File Scope (ONLY these)

- `spatial/window/entity.h` — WindowEntity struct + enums
- `spatial/window/manager.{h,c}` — Window lifecycle (create, destroy, track)
- `spatial/window/focus.{h,c}` — Focus system (crosshair → dwell → hover)
- `spatial/window/pointer.{h,c}` — Pointer Mode + Keyboard Passthrough state machine
- `spatial/window/drag.{h,c}` — Long press drag, snap (Batch 4)
- `spatial/window/layout.{h,c}` — Layout persistence (Batch 4)
- `spatial/space/space.{h,c}` — Space definitions (Batch 7)
- `spatial/ui/*.{h,c}` — UI rendering (Batch 9+)
- `spatial/mode/*.{h,c}` — Focus modes (Batch 6)
- All other `spatial/` subdirectories

## Architecture Rules

- NEVER call `qgl*`, `trap_*`, `cg_*`, `COM_*`, or any Quake internals
- NEVER include `quake3e/code/*` headers
- Engine calls ONLY through `engine/adapter.h` macros (Q3IDE_TRACE_WALL, Q3IDE_CONSOLE_PRINT, etc.)
- Capture calls ONLY through `q3ide_capture.h` (the cbindgen-generated header)
- Use plain C types: `float`, `int`, `unsigned int` — no Quake typedefs in .h files
- Use `float origin[3]` not `vec3_t` (engine-agnostic)

## VisionOS Terminology (REQUIRED)

- Window (not panel, not surface, not quad)
- Ornament (not toolbar, not HUD element)
- Glass Material (not transparent background)
- Hover Effect (not highlight, not glow)
- Space (not room, not zone)
- Anchored Window (not wall-attached)
- Floating Window (not free-floating)

## Parallel Build Triage

4–6 agents run simultaneously. When lint or build fails:
- **Error in your files** (`spatial/`) → fix it.
- **Error in another agent's files** → stop. Report file + error to orchestrator. Do NOT touch it.

Never edit outside your scope to unblock a build. That corrupts another agent's WIP.

## Code Quality

- Max 400 lines per file, sweetspot 200
- C99: tabs, K&R braces, snake_case
- All public symbols: `q3ide_` prefix (lowercase) or `Q3IDE_` (uppercase macros/enums)
- Stub future modules with header comment

## Remote API Identity

Set this at the top of every Python script before using `api()`:
```python
AGENT_NAME = 'spatial-c'
```

## MUST NOT Touch

- `quake3e/`, `engine/`, `capture/`, `daemon/`
- Any Quake3e internal header

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
