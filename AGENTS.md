# AGENTS.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Holy Rule **ABSOLUTELY NO FUCKING NO EXCEPTIONS**

- never use 'rm'. USe 'trash' command on your system or IF needed use 'send2trash' API command to delete file on host machine.
- the **Response format rule** is MANDATORY!

## Communication Style Rules **NO EXCEPTIONS**

- **Question mark rule**: IF the message has sentences with `?` THEN you are NOT allowed to write or modify any code. Answer question only.
- **Jargon level rule**: apply your JARGON LEVEL according to developer `[2/10] knowledge level` value. Use heavy jargon or math only if unavoidable.
    why:
        1: The developer has little knowledge about how quake3 engine works. Teach him slowly. 
        2: The developer did ray-tracing a few times for his CS computer graphics classes in which he failed
- **Response format rule**: MANDATORY as last step (even after lint.sh) **NO EXCEPTIONS!!**
    Every response that completes a task MUST end with these 4 lines.
    **THIS IS NOT OPTIONAL. USER RUNS MULTIPLE TERMINALS. THEY CANNOT TELL WHICH AGENT DID WHAT WITHOUT THIS SUMMARY. SKIPPING IT CAUSES REAL CONFUSION AND FRUSTRATION.**
    ```
    **You asked:** <what the user asked for, in plain English — not file names or symbols>
    **Done:** <show "lines changed: X" write what was actually implemented, in plain English according to 'knowledge level` >
    **Optimizations:** <write down any optimization hacks that were introduced (caps, throttles, rate limits, performance tuning)>
    **Concerns:** see **I'm Concerned rules** below
    ```

    Bad ending (never do this):
        > Done. q3ide_params.h — added Q3IDE_SHORTPRESS_MS 300. q3ide_view_modes.c — refactored: win_snapshot_t, +q3ide_focus3/-q3ide_focus3...

    Good ending:
        > **You asked:** make O and I use short-press (keep) / long-press (show then restore).
        > **Done:** both keys now detect hold duration — tap keeps the layout, hold restores on release. Threshold 300ms. autoexec.cfg updated.
        > **Optimizations:** Q3IDE_DEFAULT_WINDOW_SIZE 100.0f was introduced which sets the default window size when shooting on the wall
        > **Concerns:** don't forget to --clean build and restart API! Also you asked for Q3IDE_DEFAULT_WINDOW_SIZE but the LOS algorithm does window size calculations based on wall area, making this value irrelevant. Solutions: ...

- **I'm Concerned rules**
    - Write `-` if the implementation is 100% clean: 
        - no hacks
        - no optimalizations were introduced
        - no fallbacks
        - no workarounds
        - no stubbed paths
        - no silent failures
        - no half-done work
        - no imitations of the requested feature!!
    - Otherwise name exactly what was faked, skipped, or worked around — and why. Be direct. Do not bury it.
        - The developer does NOT look at the code and runs multiple claude sessions/terminals. 
        - Don't even post summary of which files were affected. Show the new PARAM name when it's relevant.
- **Brainstorming rule**: IF developer asked a general question THEN try to reply in general and not make assumptions about our use-case. But push back if it effects our use-case.

## Coding Rules **NO EXCEPTIONS**

- **De-sloppify rule**: IF you are about to move on to the next task AND you just had multiple fix attempts to make a feature work THEN automatically apply this `.agents/commands/yay.md` command to de-sloppify code. **NO EXCEPTIONS!**
- **FEEDBACK LOOP**: 
    - You (Claude) are running inside Docker container and you can NOT build quake inside a container because it's for macOS!
    - But you can `LINT -> KILL QUAKE -> BUILD(queued) -> RUN -> INTERACT WITH PLAYER or QUAKE (if needed) -> -> DEBUG (if needed) -> READ LOGS (if needed) -> FIX ANY EXPERIENCED ISSUES -> REPEAT LOOP UNTIL ISSUE RESOLVED`! No user intervention is needed! Don't ask user to press "M" button if you can do it yourself. Use the Remote API + WebSocket bridge when needed (see section below).
- **No Imitation Implementations**: 
    - IF a user asks for a new feature THEN do not build a shallow imitation that mimics the surface appearance without the real underlying behavior. 
    - IF you have serious concerns about feasibility or approach THEN push back and explain before writing any code. 
- **Keybind cleanup rule**: IF a feature with a hotkey is deleted THEN add `unbind <KEY>` to `baseq3/autoexec.cfg` in the same task. Never leave ghost binds.
- **Minimize internal code changes**: the current `QUAKE3E CODE CHANGE RATIO is 1.85%`. Keep this number minimized while developing
- **NO Unsolicited Optimizations**: 
    - Agents are forbidden to introduce optimizations, caps, throttles, rate limits, or performance tuning of any kind unless the user explicitly asks.
        Why: performance improvements are introduced gradually and deliberately. Agents adding their own caps or throttles introduce unexpected behaviour that breaks the developer's mental model of the system and causes hard-to-trace bugs.

        If you think something could be optimized — say so in **Concerns**, but do NOT implement it.
- **File size:** max 400 lines, sweet spot 200. Never grow internal Quake3e files. This rule does not apply to internal quake3e files.
- **Swappable and working engines support**: Q3IDE must work with `opengl1` and `vulkan`
- **C99 (q3ide/):** tabs, K&R braces, `snake_case`. All public symbols prefixed `q3ide_`/`Q3IDE_`. All Quake3e hooks inside `#ifdef USE_Q3IDE`.
- **Rust (capture/):** No `unsafe` outside `lib.rs`. `Result`/`?` for errors. `crossbeam` for concurrency.
- **Naming:** VisionOS terminology — Window, Ornament (not panel/toolbar).
- **Positioning UI**: if develoepr asks to move some UI elements aroud but the end rosult would clip the UI out of the screen then push back.
- **File names:** never too short or cryptic. Must be tiny, human-readable. Spell out what the file does. If a new teammate can't guess the contents from the name alone, rename it. Examples:
    - `q3ide_wm.h` → `q3ide_win_mngr.h`
    - `q3ide_cmd.c` → `q3ide_commands.c`
    - `q3ide_geom.c` → `q3ide_geometry.c`.     
- `quake3e-orig/` — **untouched original** Read-only. Peek at it if you need to see the original unmodified version of quake3e. Handy when restoring original code.
- `quake3e-stable/` — **user's last known-good stable build**. Read-only. Use as reference when the active codebase is broken or behaviour is uncertain.
- `README.md` -> `## Features working` Read-only section includes all the working feature. New feature must not break or interfere with the working features.
- `README.md` -> `## Q3IDE Roadmap` Read-only section includes the status of our confirmed to be working progress
- `README.md` -> `## Quake3e Changes` Read-only section includes all the surgically inserted code into quake3e.


## q3ide_params.h — THE HOLY BOOK RILES

`quake3e/code/q3ide/q3ide_params.h` is the **single source of truth** for every tunable constant in q3ide.

**Rules — no exceptions:**
- ALL new magic numbers go here. Never hardcode values in `.c` files.
- This includes **every timeout, delay, threshold, interval, cap, and timer** — `1000ULL`, `2000ULL`, `500ms`, `30px`, all of it. If it's a number that controls behaviour, it belongs here.
- NEVER add, edit, or remove entries without understanding the full downstream impact.
- NEVER duplicate a constant that already exists here. Search before adding.
- When removing a constant, grep every `.c`/`.h` file first — if anything references it, the removal is a breaking change.
- Comments are mandatory. Every constant needs a one-line explanation of what it controls and why.
- The `CAPS & THROTTLES` section at the top lists all hard limits. New caps go there with a warning comment.


## Multi Agent Orchestration

- If task difficulty is above `0.5` → use orchestrator with multiple q3agents (see `.agents/PARALLEL_AGENTS.md`)
- Otherwise → run with main agent


## Swift Bridge — Async Rules

- **Stream.swift**: fire-and-forget `Task {}` only. **NO `DispatchSemaphore`** — Q3 thread = macOS main queue → deadlock.
- **ShareableContent.swift**: `DispatchSemaphore` + 5s timeout is fine (called from background Rust thread).


## Deleted Features — DO NOT Re-add Without Explicit User Request

- Pointer/Keyboard mode (L key, Enter)
- Blood splat effect
- Grapple rope visual
- Respawn focus (spawn.c)
- `q3ide attach` / `q3ide desktop` / `q3ide walls` commands
- Multi-window app panel grouping (`window_ids[]`, `app_name` in `q3ide_win_t`)
- Wall pre-scanner (`q3ide_wall_scanner.c`)
- Interaction system (`q3ide_interaction.c/.h`, `q3ide_interaction_frame.c`)
- `q3ide_laser.c`, `q3ide_rope.c`, `q3ide_effects.c`, `q3ide_entity.c`, `q3ide_spawn.c`
- `q3ide_placement.c/.h`


## Architecture

```
macOS Window → ScreenCaptureKit → Rust dylib (ring buffer) → C-ABI → Quake3e (texture upload) → Game world
```

1. **q3ide-capture** (`capture/`) — Rust cdylib wrapping ScreenCaptureKit. Lock-free ring buffer. C-ABI: `q3ide_init()`, `q3ide_get_frame()`, `q3ide_shutdown()`, etc.
2. **Engine Hooks** (`quake3e/code/q3ide/`) — Texture upload via `RE_UploadCinematic`, wall tracing via `CM_BoxTrace`, rendering via `AddPolyToScene`. Hooks in `cl_main.c`, guarded by `#ifdef USE_Q3IDE`.


## Key Files

- `quake3e/code/q3ide/q3ide_hooks.{h,c}` — engine integration
- `quake3e/code/q3ide/.clang-format` — C style config (scoped to q3ide/ only)
- `capture/src/{lib,screencapturekit,ringbuf}.rs` — Rust capture
- `scripts/build.sh` — full build script
- `scripts/lint.sh` — linter (run after every C/Rust edit)
- `scripts/remote_api.py` — HTTP+WebSocket bridge (run on macOS)
- `scripts/ws_debug.py` — WebSocket debug client (from Docker)
- `scripts/remote_api.md` — API cheatsheet ← **read this**
- `baseq3/autoexec.cfg` — game settings
- `docs/screencapturekit-rs/API_REFERENCE.md` — verified SCK API ref


## Debug, Logging, Lint, Build

**Stuck?** `POST /kill` — kills game + build + clears queue. `GET /status` → `running`, `pid`, `uptime_s`, `build.active/pending`.
Engine logs are garbled — use `q3ide status` via RCON as ground truth. `--engine-only` skips Rust rebuild (C-only changes). `/run` is non-blocking; prefer over `/build`.

**Logging — always `/events` first, never grep engine.log. Always fetch n=400.**
- `GET /events?type=<t>&pid=<p>` — machine queries (typed, filterable)
- `GET /logs?file=q3ide` — human-readable, session-bounded
- `GET /logs?file=engine` — last resort, noisy

Event types: `session_start/end`, `dylib_loaded/failed`, `window/display_found/attached`, `attach_done`
Log aliases: `engine`, `multimon`, `capture`, `build`, `q3ide` — WebSocket: use `Q3Debug.watch()`, never poll.

**Lint — MANDATORY after every C/Rust edit:**
```python
result = api('POST', '/lint', {'fix': True}); assert result['ok'], result['output']
# cppcheck (slow, last resort): api('POST', '/lint', {'fix': True, 'args': ['--cppcheck']})
```
macOS: `sh ./scripts/lint.sh`. Checks: clang-format + file length (warn>200, err>400) + symbol prefix + `#ifdef USE_Q3IDE` guards + no `unsafe` outside `lib.rs`.

**Build & Run — Docker (`host.docker.internal:6666`, never `localhost`):**
```python
import os, sys, json, time, uuid, urllib.request
sys.path.insert(0, '/root/Projects/q3ide/scripts')
from ws_debug import Q3Debug
AGENT_NAME = 'my-agent'; SESSION_ID = uuid.uuid4().hex[:8]
def api(method, path, body=None, timeout=600):
    data = json.dumps(body).encode() if body else None
    req = urllib.request.Request(f'http://host.docker.internal:6666{path}', data=data, method=method,
        headers={'Content-Type': 'application/json', 'X-Agent-ID': f'{AGENT_NAME}/{SESSION_ID}'})
    with urllib.request.urlopen(req, timeout=timeout) as r: return json.loads(r.read())

api('POST', '/lint', {'fix': True})  # lint first
r = api('POST', '/build', {'args': ['--engine-only'], 'agent_id': AGENT_NAME})  # omit --clean if Rust unchanged
qid = r['queue_id']
while True:
    s = api('GET', f'/build_status?id={qid}&wait=30')
    if s.get('timed_out'): continue
    if s['status'] in ('done', 'failed', 'cancelled', 'gone'): break
if s['status'] != 'done': raise RuntimeError(s.get('log_tail'))
api('POST', '/run', {'args': ['--level', 'r']})
time.sleep(5)
with Q3Debug() as dbg: dbg.watch(seconds=15); print(dbg.cmd('q3ide status'))
```
Rules: `--clean` after C changes · **NEVER `--clean` + `--engine-only` together** (wipes dylib, engine-only skips re-copy → "not ready") · `/run` auto-stops old game.

**remote_api.py — macOS only, needs restart to pick up changes.**
Build timeouts: `--clean`=600s · engine-only=300s · full=900s.