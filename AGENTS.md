# AGENTS.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Communication Style

Match the question. Simple question = simple answer. No theories, no code, no walls of text unless asked.

If the user asks a question — answer it. Do not start planning, implementing, or showing code examples unless explicitly told to.

## Parallel Agents — Lint/Build Fail Triage

**4–6 agents run simultaneously. Each owns specific files. Never fix another agent's files.**

When lint or build fails:
1. Read the error. Which file?
2. Your scope? → fix it.
3. Another agent's scope? → **stop. report to orchestrator. do not touch it.**

File ownership:
- `quake3e/code/q3ide/`, `quake3e/Makefile` → **engine-adapter**
- `capture/` → **capture-rust**
- `spatial/` → **spatial-c**
- `daemon/` → **daemon-rust**

If another agent's WIP breaks the shared build, that's their fix. Report the failing file+error to orchestrator and work on independent tasks or wait. **Never edit outside your scope to unblock a build** — it corrupts their in-progress work.

## Agent Autonomy — NON-NEGOTIABLE

**The agent does everything. Never ask the user to run, build, test, or check anything.**

- Lint → build → run → read logs → fix → repeat. All of it. Autonomous loop.
- Use the Remote API (`host.docker.internal:6666`) to build, run, stop, and tail logs.
- Use `dbg.cmd(...)` / `dbg.watch(...)` to read live output. Never ask the user to paste logs.
- If the API server is unreachable, say so once and wait — do NOT ask the user to start it.
- Fix compiler errors yourself. Read the struct definitions. Don't ship code that doesn't compile.
- When something breaks, diagnose from logs before touching code. Don't guess.

## Agent Infrastructure Location

**Agent definition files always go in `.agents/agents/`** (dot-prefixed, at project root) — NOT in `.claude/` or `agents/` (no dot). Commands go in `.agents/commands/`. Project manifest at `.agents/PROJECT_LOOP.md`. Claude-only settings in `.claude/`. The `.agents/` convention is the cross-IDE standard (Claude Code, OpenCode, etc.).

## Stream Freeze Pattern — Pause All Windows at Zero Cost

**The canonical cost-saving technique for expensive operations.** Verified in production: 100% FPS restoration instantly.

```c
// Pause — call before any expensive op (area transition, placement queue drain, etc.)
Q3IDE_WM_PauseStreams();   // sets STREAMS_PAUSED atomic bool in Rust
                           // get_frame() returns None → zero texture uploads
                           // SCStreams stay warm, last frame frozen on GPU

// Resume — call when done
Q3IDE_WM_ResumeStreams();
```

**Properties:**
- No SCStream teardown, no latency. Pure flag check per `get_frame()` call.
- Last captured frame stays frozen on GPU — content looks alive, just static.
- Left overlay shows amber "PAUSED" banner while active.
- Hold ";" in-game to trigger manually.

**Use this everywhere:** area transitions, placement queue drain (Stage 1.4), any operation that would otherwise spike texture upload bandwidth.

**Files:** `capture/src/screencapturekit.rs` (`STREAMS_PAUSED`, `set_streams_paused`), `capture/src/lib.rs` (`q3ide_pause_all_streams`, `q3ide_resume_all_streams`), `quake3e/code/q3ide/q3ide_win_mngr.c` (`Q3IDE_WM_PauseStreams`, `Q3IDE_WM_ResumeStreams`).

---

## q3ide_params.h — THE HOLY BOOK

`quake3e/code/q3ide/q3ide_params.h` is the **single source of truth** for every tunable constant in q3ide.

**Rules — no exceptions:**
- ALL new magic numbers go here. Never hardcode values in `.c` files.
- NEVER add, edit, or remove entries without understanding the full downstream impact.
- NEVER duplicate a constant that already exists here. Search before adding.
- When removing a constant, grep every `.c`/`.h` file first — if anything references it, the removal is a breaking change.
- Comments are mandatory. Every constant needs a one-line explanation of what it controls and why.
- The `CAPS & THROTTLES` section at the top lists all hard limits. New caps go there with a warning comment.

## Coding Style

- Minimize Quake3e internal code changes — keep engine swappable.
- **File size:** max 400 lines, sweet spot 200. Never grow internal Quake3e files.
- **C99 (q3ide/):** tabs, K&R braces, `snake_case`. All public symbols prefixed `q3ide_`/`Q3IDE_`. All Quake3e hooks inside `#ifdef USE_Q3IDE`.
- **Rust (capture/):** No `unsafe` outside `lib.rs`. `Result`/`?` for errors. `crossbeam` for concurrency.
- **Naming:** VisionOS terminology — Window, Ornament (not panel/toolbar).
- **File names:** never too short or cryptic. Must be tiny, human-readable. Spell out what the file does. `q3ide_wm.h` → `q3ide_win_mngr.h`, `q3ide_cmd.c` → `q3ide_commands.c`, `q3ide_geom.c` → `q3ide_geometry.c`. If a new teammate can't guess the contents from the name alone, rename it.
- **ALWAYS remind the user to `--clean` build** after C source changes. `make` timestamps can miss changes across Docker/macOS sync.
- **When running inside Docker** — use the Remote API + WebSocket bridge (see section below). Do NOT fall back to log polling; use the live WebSocket stream instead.


## Debug Tips

- Engine logs via `tee` are garbled (CR progress bars) — use RCON `q3ide status` to verify state
- `/run` tracks build shell PID not game process — `/stop` uses `pkill -f quake3e` as fallback
- **Stuck?** `POST /kill` — kills game + running build + clears queue in one call
- **Queue:** `POST /queue/clear` — drains pending builds + kills stuck build (leaves game running)
- **Status:** `GET /status` — returns `running`, `pid`, `uptime_s`, `build.active`, `build.pending`
- `--engine-only` flag skips `cargo build --release` for fast C-only iteration
- Use `/run` (non-blocking) instead of `/build` (blocks, 600s timeout) when build takes long
- `q3ide status` via RCON is ground truth: shows active windows, dylib state, frame counts, shader slots

## Logging

Three outputs — use the right one per purpose:

| Output | API call | When to use |
|--------|----------|-------------|
| **Structured events** | `GET /events?type=<t>&pid=<p>` | Machine queries — filterable, typed, reliable |
| **q3ide levelled log** | `GET /logs?file=q3ide` | Human-readable — no Q3 engine noise, session-bounded |
| **engine.log** | `GET /logs?file=engine` | Last resort — noisy, mixed Q3 + q3ide output |

**Always use `/events` to verify what happened. Never grep engine.log.**

```python
evts = api('GET', f'/events?type=dylib_loaded&pid={game_pid}')['events']
assert evts, "dylib never loaded"

evts = api('GET', f'/events?type=attach_done&pid={game_pid}')['events']
print(f"Attached {evts[-1]['attached']}/{evts[-1]['total']}")
```

Event types: `session_start`, `session_end`, `dylib_loaded`, `dylib_failed`,
`window_found`, `display_found`, `window_attached`, `display_attached`, `attach_done`

Log aliases: `engine`, `multimon`, `capture`, `build`, `q3ide`

**Always fetch 400 lines. Never use n=40, n=50, n=100 — 400 is the minimum.**
WebSocket streams: same aliases. In Docker use `Q3Debug.watch()` — never poll manually.

## Architecture

```
macOS Window → ScreenCaptureKit → Rust dylib (ring buffer) → C-ABI → Quake3e (texture upload) → Game world
```

1. **q3ide-capture** (`capture/`) — Rust cdylib wrapping ScreenCaptureKit. Lock-free ring buffer. C-ABI: `q3ide_init()`, `q3ide_get_frame()`, `q3ide_shutdown()`, etc.
2. **Engine Hooks** (`quake3e/code/q3ide/`) — Texture upload via `RE_UploadCinematic`, wall tracing via `CM_BoxTrace`, rendering via `AddPolyToScene`. Hooks in `cl_main.c`, guarded by `#ifdef USE_Q3IDE`.

`quake3e-orig/` — **untouched original** Quake3e source. Read-only. Never modify.
`quake3e-stable/` — **user's last known-good build**. Read-only. Use as reference when the active codebase is broken or behaviour is uncertain.

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

## Linting — MANDATORY

**Run after every C/Rust edit.** From Docker:

```python
result = api('POST', '/lint', {'fix': True})  # auto-fix clang-format, then check
print(result['output'])
assert result['ok'], "Lint errors — fix before building"
```

**cppcheck is OFF by default** (slow). Use as last resort when suspecting memory/logic bugs:

```python
result = api('POST', '/lint', {'fix': True, 'args': ['--cppcheck']})
```

On macOS: `sh ./scripts/lint.sh` (fast) or `sh ./scripts/lint.sh --cppcheck` (thorough)

| Scope | Tool | Default | Checks |
|---|---|---|---|
| `q3ide/` C files | clang-format | ✅ on | Style, indentation, braces |
| `q3ide/` C files | cppcheck | ❌ `--cppcheck` flag | Null deref, uninit vars, logic bugs |
| `q3ide/` C files | basic | ✅ on | File length (warn >200, error >400), symbol prefix |
| Modified Quake3e files | basic | ✅ on | `#ifdef USE_Q3IDE` guards |
| `capture/` Rust | basic | ✅ on | No `unsafe` outside `lib.rs` |

macOS: `brew install clang-format cppcheck`. Docker: clang-format-14 static binary installed at `/usr/local/bin/clang-format` (persistent in container).

**Agents MUST run clang-format on every C file they write or modify. No exceptions.**

## Build & Run — Docker Workflow

**From Docker always use `host.docker.internal:6666`** — `localhost` does NOT reach the Mac.
Detect Docker: `os.path.exists('/.dockerenv')`.

```python
import os, sys, json, time, uuid, urllib.request
sys.path.insert(0, '/root/Projects/q3ide/scripts')
from ws_debug import Q3Debug

AGENT_NAME = 'engine-adapter'  # set per agent
SESSION_ID = uuid.uuid4().hex[:8]

def api(method, path, body=None, timeout=600):
    data = json.dumps(body).encode() if body else None
    headers = {'Content-Type': 'application/json', 'X-Agent-ID': f'{AGENT_NAME}/{SESSION_ID}'}
    req = urllib.request.Request(f'http://host.docker.internal:6666{path}',
                                  data=data, method=method, headers=headers)
    with urllib.request.urlopen(req, timeout=timeout) as r:
        return json.loads(r.read())

# 1. lint
result = api('POST', '/lint', {'fix': True})
assert result['ok'], result['output']

# 2. build — long-poll, no sleep loop, no Docker timeout risk
r = api('POST', '/build', {'args': ['--clean', '--engine-only'], 'agent_id': AGENT_NAME})
qid = r['queue_id']
while True:
    s = api('GET', f'/build_status?id={qid}&wait=30')  # blocks up to 30s server-side
    if s.get('timed_out'): continue
    if s['status'] in ('done', 'failed', 'cancelled', 'gone'): break
if s['status'] != 'done':
    print(s.get('log_tail', '(no output)'))  # build.log tail included automatically
    raise RuntimeError(f"Build {s['status']} rc={s.get('returncode')}")

# 3. run + WebSocket debug
api('POST', '/run', {'args': ['--level', '0']})
time.sleep(5)
with Q3Debug() as dbg:
    dbg.watch(seconds=15, filter_fn=lambda f, l: True)
    print(dbg.cmd('q3ide status'))

# 4. stop
api('POST', '/stop')
```

Rules: always `--clean` after C changes · always `host.docker.internal` · use `--engine-only` to skip Rust · `/run` auto-stops old game.

## Build & Run (macOS)

```sh
sh ./scripts/build.sh --api --clean --run --level 0
```

| Flag | Description |
|------|-------------|
| `--clean` | `make clean` before build — required after C changes |
| `--run` | Launch game after build |
| `--api` | Start `remote_api.py` in background |
| `--level <map>` | `0`→`q3dm0`, `7`→`q3dm7`, or full map name |
| `--bots <n>` | Add N bots |
| `--execute '<cmd>'` | Console command after map loads |
| `--release <alias\|path>` | Engine source to build. Aliases: `nightbuild` (default=`quake3e/`), `stable` (`quake3e-stable/`), `orig` (`quake3e-orig/`). Or pass an absolute path. |

## Task Completion — MANDATORY response format

**THIS IS NOT OPTIONAL. USER RUNS 4+ TERMINALS. THEY CANNOT TELL WHICH AGENT DID WHAT WITHOUT THIS SUMMARY. SKIPPING IT CAUSES REAL CONFUSION AND FRUSTRATION.**

**Every response that completes a task MUST end with these three lines. No exceptions.**

```
**You asked:** <what the user asked for, in plain English — not file names or symbols>
**Done:** <what was actually implemented, in plain English>
**Concerns:** <see below>
```

**Concerns rules:**
- Write `-` if the implementation is 100% clean: no hacks, no fallbacks, no workarounds, no stubbed paths, no silent failures, nothing half-done.
- Otherwise name exactly what was faked, skipped, or worked around — and why. Be direct. Do not bury it.

**The zero-copy cautionary tale:** Claude implemented "zero-copy IOSurface upload", silently fell back to CPU copy when it failed, and never told the user. The feature appeared done. It wasn't. This section exists to prevent that exact failure.

Bad ending (never do this):
> Done. q3ide_params.h — added Q3IDE_SHORTPRESS_MS 300. q3ide_view_modes.c — refactored: win_snapshot_t, +q3ide_focus3/-q3ide_focus3...

Good ending:
> **You asked:** make O and I use short-press (keep) / long-press (show then restore).
> **Done:** both keys now detect hold duration — tap keeps the layout, hold restores on release. Threshold 300ms. autoexec.cfg updated.
> **Concerns:** -
