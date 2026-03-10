# Q3IDE Agent Feedback Loop

Canonical loop — all specialist agents follow this protocol.

```
ROUTE → READ SPEC → IMPLEMENT → LINT → REFACTOR? → BUILD → SPAWN WINDOWS → RUN → CHECK EVENTS → REVIEW → REPEAT or SIGN-OFF
```

## Steps

1. **ROUTE** — Orchestrator assigns batch to correct agent(s)
2. **READ SPEC** — Read `./plan/04-Q3IDE_SPECIFICATION.md` for the assigned batch
3. **IMPLEMENT** — Write code in agent's file scope only
4. **LINT** — `POST /lint {"fix": true}` → assert `ok: true` (cppcheck off by default — fast)
   - Last resort only: `POST /lint {"fix": true, "args": ["--cppcheck"]}` (slow, thorough)
5. **REFACTOR?** — Fix lint errors, repeat lint if needed (max 3 iterations)
6. **BUILD** — Enqueue build, long-poll until complete (see Robust Build Wait below)
7. **SPAWN WINDOWS** — Use AppleScript via POST /applescript to open iTerm2 test windows
8. **RUN** — `POST /run {"args": ["--level", "0"]}`; wait ~3s for game to load
9. **CHECK EVENTS** — Query `/events` for structured confirmation (see Log Checking below)
10. **REVIEW** — Route to reviewer agent
11. **REPEAT or SIGN-OFF** — Fix issues or sign off

## API Quick Reference (from Docker)

```python
BASE = "http://host.docker.internal:6666"
AGENT_NAME = 'my-agent'   # set per-agent at top of script

def api(method, path, body=None):
    import urllib.request, json, os
    BASE = f"http://{os.environ.get('Q3IDE_API_HOST','host.docker.internal')}:6666"
    data = json.dumps(body).encode() if body else None
    headers = {'Content-Type': 'application/json', 'X-Agent-ID': AGENT_NAME} if data else {'X-Agent-ID': AGENT_NAME}
    req = urllib.request.Request(f'{BASE}{path}', data=data, method=method, headers=headers)
    with urllib.request.urlopen(req, timeout=300) as r:
        return json.loads(r.read())

api('POST', '/lint', {'fix': True})                        # lint + auto-fix
api('POST', '/run', {'args': ['--level', '0']})            # launch game
api('POST', '/stop')                                       # kill game
api('POST', '/console', {'cmd': 'q3ide status'})           # RCON
api('GET',  '/events?type=attach_done&n=5')                # structured events
api('GET',  '/logs?file=q3ide&n=400')                      # q3ide structured log — ALWAYS n=400, never tail=20
```

## Robust Build Wait (REQUIRED — no sleep loops)

```python
# Step 1: enqueue
r = api('POST', '/build', {
    'args': ['--clean', '--engine-only'],
    'agent_id': AGENT_NAME,
    'auto_run': False,
})
qid = r['queue_id']
print(f"Build queued: {qid} (position {r['position']})")

# Step 2: long-poll — blocks up to 30s per call, server returns immediately on completion.
# ?wait=30 is Docker-safe: request timeout is set to 300s in api(), so no connection drop.
while True:
    s = api('GET', f'/build_status?id={qid}&wait=30')
    if s.get('timed_out'):
        continue  # server timed out waiting — retry immediately, no sleep needed
    if s['status'] in ('done', 'failed', 'cancelled', 'gone'):
        break

if s['status'] != 'done':
    # log_tail is included automatically when build is done/failed
    print('BUILD FAILED:')
    print(s.get('log_tail', '(no output)'))
    raise RuntimeError(f"Build {s['status']} rc={s.get('returncode')}")

print(f"Build done in {s.get('elapsed_s')}s")
```

## Game Lifecycle (REQUIRED)

Simple rule: **when you need to build & run, kill Quake if it has been running ≥ 5s — no matter who launched it.**

```python
# Before every POST /run:
s = api('GET', '/status')
if s.get('running') and s.get('uptime_s', 0) >= 5:
    api('POST', '/stop')
    time.sleep(1)
api('POST', '/run', {'args': ['--level', '0', '--execute', 'q3ide attach all']})
```

After your test, stop the game:
```python
api('POST', '/stop')
```

## Log Checking (REQUIRED — use /events, not grep)

After running the game, use structured events to confirm correct behaviour. Do NOT grep raw engine.log — it is full of Q3 engine noise.

```python
import time

# Wait for game to initialise (session_start event appears)
game_start_ts = time.time()
for _ in range(20):   # up to 10s
    evts = api('GET', f'/events?type=session_start&since={game_start_ts - 5}')['events']
    if evts:
        pid = evts[-1]['pid']
        break
    time.sleep(0.5)
else:
    raise RuntimeError("game never emitted session_start — check /logs?file=engine")

# Trigger attach
api('POST', '/console', {'cmd': 'q3ide attach all'})
time.sleep(1)  # brief wait for attach to complete

# Confirm attach succeeded
evts = api('GET', f'/events?type=attach_done&pid={pid}')['events']
if not evts:
    raise RuntimeError("No attach_done event — attach failed")
e = evts[-1]
print(f"Attached {e['attached']}/{e['total']} windows/displays")

# Check for errors in this session
err_evts = api('GET', f'/events?pid={pid}&n=200')['events']
errors = [ev for ev in err_evts if ev.get('type') in ('dylib_failed',)]
if errors:
    print("ERRORS:", errors)

# If needed: last 50 lines of q3ide structured log (levelled, timestamped)
log = api('GET', '/logs?file=q3ide&n=400')['content']
print(log)
```

**Known event types:** `session_start`, `session_end`, `dylib_loaded`, `dylib_failed`,
`window_found`, `display_found`, `window_attached`, `display_attached`, `attach_done`

## Logging System Overview

Three outputs exist simultaneously — use the right one for each purpose:

| Output | Path | When to use |
|--------|------|-------------|
| `q3ide_events.jsonl` | `logs/q3ide_events.jsonl` | Machine queries via `/events` — structured, filterable |
| `q3ide.log` | `logs/q3ide.log` | Human-readable levelled log — via `/logs?file=q3ide` |
| `engine.log` | `logs/engine.log` | Last resort — noisy Q3 engine output — via `/logs?file=engine` |

Events are written from C with `Q3IDE_Eventf("type", "\"key\":%d", val)` — one JSON object per line.

Log messages use macros: `Q3IDE_LOGI(...)` / `Q3IDE_LOGW(...)` / `Q3IDE_LOGE(...)`.

## Parallel Agent Rules (4–6 agents may run simultaneously)

When lint or build fails, **triage before touching anything**:

1. Read the error. Which file is it in?
2. Is that file in YOUR scope? → fix it.
3. Is it in ANOTHER agent's scope? → **stop. do not touch it.** Report to orchestrator.

```
File ownership map (from orchestrator.md):
  quake3e/code/q3ide/  → engine-adapter
  capture/             → capture-rust
  spatial/             → spatial-c
  daemon/              → daemon-rust
```

**Your lint must be clean on YOUR files.** If another agent's WIP breaks the shared build, that is their problem to fix — not yours. Report the conflict and either:
- Work on independent tasks while waiting, OR
- Ask orchestrator to sequence the dependency

Never edit outside your scope to unblock a build. That corrupts another agent's in-progress work.

## Rules

- Who runs the game: the top integration agent (engine-adapter or orchestrator)
- reviewer NEVER runs the game
- Max 3 fix iterations **on your own files** before escalating to user
- Commit after batch sign-off
- NEVER poll `/build_status` in a sleep loop — always use `?wait=30`
- NEVER grep `engine.log` to detect q3ide events — use `/events`
- NEVER fix lint/build errors in files outside your scope
- ALWAYS fetch **400 lines** minimum for logs — `n=400`. Never use n=40, n=50, n=100. 400 is the default.
