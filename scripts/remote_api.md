# Q3IDE Remote API — Cheatsheet

Server runs on macOS. Claude (Docker) calls `http://host.docker.internal:6666/...`

## Start

```sh
# Manually
python3 scripts/remote_api.py

# Via build.sh (recommended) — starts in background, logs to logs/remote_api.log
sh ./scripts/build.sh --api

# env overrides: Q3IDE_API_PORT=6666  Q3IDE_RCON_PASSWORD=q3idedev666
```

## Endpoints

| Method | Path | Description |
|--------|------|-------------|
| GET | `/` | Health check + game running? |
| GET | `/status` | PID, uptime, running bool |
| POST | `/build` | Run build.sh, return stdout/stderr |
| POST | `/run` | Launch game in background |
| POST | `/stop` | Kill game process |
| GET | `/logs` | Tail a log file |
| POST | `/console` | Send RCON command, return response |
| GET | `/ws` | **WebSocket** — live log stream + RCON |

## WebSocket (`/ws`) — preferred debug interface

Connect from Docker:
```python
import sys
sys.path.insert(0, '/root/Projects/q3ide/scripts')
from ws_debug import Q3Debug

with Q3Debug() as dbg:
    dbg.watch(seconds=20)                      # stream all logs live
    print(dbg.cmd('q3ide status'))             # RCON, blocks for response
    print(dbg.cmd('status'))
    dbg.watch(seconds=10,
        filter_fn=lambda f, l: f == 'multimon')  # watch multimon only
```

Query params: `?logs=engine,multimon,capture` (default: `engine,multimon`)

Messages received (JSON):
- `{"type":"log", "file":"engine"|"multimon"|"capture", "line":"..."}`
- `{"type":"rcon", "cmd":"...", "response":"...", "ok":true}`
- `{"type":"status", "running":bool, "pid":int, "uptime_s":int}`
- `{"type":"hello", "logs":[...], "rcon_port":27960}`

Send to server: `{"cmd": "q3ide status"}` → RCON response

## HTTP curl examples

```sh
HOST=host.docker.internal  # or localhost if running curl on macOS directly

# Health
curl http://$HOST:6666/

# Build
curl -s -X POST http://$HOST:6666/build \
  -H 'Content-Type: application/json' \
  -d '{"args":["--clean"]}'

# Launch (level 0)
curl -s -X POST http://$HOST:6666/run \
  -H 'Content-Type: application/json' \
  -d '{"args":["--level","0"]}'

# Stop
curl -s -X POST http://$HOST:6666/stop

# RCON via HTTP
curl -s -X POST http://$HOST:6666/console \
  -H 'Content-Type: application/json' \
  -d '{"cmd":"q3ide status"}'

# Tail log (last 50 lines)
curl -s 'http://$HOST:6666/logs?file=engine&n=50'
curl -s 'http://$HOST:6666/logs?file=multimon&n=100'
```

Log file aliases: `engine`, `multimon`, `capture`

## Python HTTP helper (from Docker)

```python
import os, urllib.request, json

BASE = f"http://{os.environ.get('Q3IDE_API_HOST','host.docker.internal')}:{os.environ.get('Q3IDE_API_PORT','6666')}"

def api(method, path, body=None):
    data = json.dumps(body).encode() if body else None
    headers = {'Content-Type': 'application/json'} if data else {}
    req = urllib.request.Request(f'{BASE}{path}', data=data, method=method, headers=headers)
    with urllib.request.urlopen(req, timeout=300) as r:
        return json.loads(r.read())
```

## RCON

Password: `q3idedev666` — set in `baseq3/autoexec.cfg` as `set rconPassword "q3idedev666"`.

Manual test from macOS terminal:
```sh
echo -ne '\xff\xff\xff\xffrcon q3idedev666 status\n' | nc -u 127.0.0.1 27960
```
