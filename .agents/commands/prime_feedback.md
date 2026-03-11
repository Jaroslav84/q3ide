# Bootstrap Feedback Loop — Q3IDE Autonomous Setup

You are being asked to perform a ONE-TIME project analysis and setup for Q3IDE.
Your goal: document the exact feedback loop for this C/Rust/Quake3e project so any agent can build, run, observe, and self-correct autonomously — forever after.

---

## PHASE 1 — Project Reconnaissance

Explore the project. Do NOT assume anything. Read the actual files.

**Discover what this project is:**
- Read `scripts/build.sh` — understand all flags: `--clean`, `--engine-only`, `--run`, `--level`, `--api`, `--bots`, `--execute`, `--release orig|stable|nightbuild|/path`
- Read `scripts/remote_api.py` — HTTP+WebSocket bridge on port 6666; understand all endpoints
- Read `quake3e/Makefile` — understand build targets, ARCH, USE_Q3IDE flag
- Read `capture/Cargo.toml` — understand crate name, dependencies, cdylib target
- Read `AGENTS.md` — project coding rules and workflow
- Read `CLAUDE.md` → points to `AGENTS.md`

**Discover what already exists for observability:**
- Check `logs/` directory: `q3ide.log`, `q3ide_multimon.log`, `remote_api.log`
- Check RCON: `127.0.0.1:27960` password `q3idedev666` — `q3ide status` is ground truth
- Check WebSocket streams (via `scripts/ws_debug.py`): `engine`, `multimon`, `capture` channels
- Check `scripts/check_logs.sh` — quick error scan helper

**Discover how processes are started:**
- `build.sh --run` launches the game after build
- `build.sh --api` starts `remote_api.py` before running (port 6666)
- Docker: POST `/build` + POST `/run` via `host.docker.internal:6666`
- macOS direct: `sh ./scripts/build.sh --clean --api --run --level 0 --execute 'q3ide attach all'`

---

## PHASE 2 — Gap Analysis

Based on what you found, determine what is MISSING for a full feedback loop:

For Q3IDE specifically:
- Does the engine write to `logs/q3ide.log`? If not → check `build.sh` tee setup.
- Does `remote_api.py` log to `logs/remote_api.log`? If not → note it.
- Is RCON reachable at `127.0.0.1:27960`? This is required for live state queries.
- Are WebSocket streams working? `ws_debug.py` must connect to `host.docker.internal:6666`.
- Is `q3ide status` command registered? It must return window counts + dylib state.

Do NOT add logging frameworks or wrappers that don't exist. Only document gaps.

---

## PHASE 3 — Verify the Feedback Loop

Confirm the existing loop is functional. For Q3IDE:

### Build Verification
- C engine: `cd quake3e && make ARCH=x86_64` — success = `build/release-darwin-x86_64/quake3e.x86_64`
- Rust capture: `cd capture && cargo build --release` — success = `target/release/libq3ide_capture.dylib`
- Full build: `sh ./scripts/build.sh --engine-only` (skip cargo for C-only iteration)

### Log Capture
- Engine stdout+stderr → `logs/q3ide.log` (via build.sh tee)
- Q3IDE subsystem → `logs/q3ide_multimon.log` (direct file write from q3ide_hooks.c)
- Remote API → `logs/remote_api.log`
- Rust capture → stderr (set `RUST_LOG=debug` for verbose)

### Live Observation (Docker)
```python
import sys
sys.path.insert(0, '/root/Projects/q3ide/scripts')
from ws_debug import Q3Debug

with Q3Debug() as dbg:
    dbg.watch(seconds=10, filter_fn=lambda f, l: True)
    print(dbg.cmd('q3ide status'))
```

### RCON Commands for Ground Truth
- `q3ide status` — active windows, dylib state, frame counts, shader slots
- `q3ide list` — capturable windows
- `status` — player/server state

---

## PHASE 4 — Write the Project Feedback Manifest

Create a file: `.agents/PROJECT_LOOP.md`

This file must contain the SPECIFIC, DISCOVERED truth about THIS project.

```
# Project Feedback Loop — Q3IDE

## Services & Their Log Files
[table: service, port, log file, start command]

## How to Start Everything
[exact commands for macOS and Docker]

## How to Build
[flags, what to check in logs]

## How to Check Build Success
[what to look for in which log file]

## How to Detect a Crash
[error patterns per log file]

## How to Get Live State
[RCON commands, WebSocket streams]

## Log Priority (for agents)
[which log to check first for engine issues, for capture issues, for spatial/placement]

## Known Error Patterns
[fill in after first test run]

## Quick Iteration Loop
[lint → build → run → verify — exact commands]
```

---

## PHASE 5 — Validate

Confirm the feedback loop works by:
1. Reading `logs/q3ide_multimon.log` (if game has been run recently)
2. Checking RCON is reachable (if game is running)
3. Verifying `scripts/check_logs.sh --errors` finds no unknown critical errors

Report:
1. What you discovered about the project
2. What is already in place
3. Any gaps found
4. Exact commands to start the project with full logging active
5. That the Q3IDE agent feedback loop protocol is now documented in `agents/PROJECT_LOOP.md`
