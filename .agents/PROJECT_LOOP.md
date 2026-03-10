# Q3IDE Project Loop Manifest

## Current Status

- Batch 0: ✅ Done (Foundation — capture, dylib, texture, wall placement, 3-monitor)
- Batch 1: ✅ Done (Window Entity & Multiple Windows — up to 16 slots, distance-based FPS)
- Batch 2: ✅ Done (Interaction Model — dwell, hover, pointer mode, keyboard passthrough)
- Batch 3: 🔧 Next (Title tracking, minimized state, dirty frames, status HUD)

## Active Agents

See `.agents/agents/q3agents/` for agent definitions.

## Build Info

- macOS host: Intel x86_64, AMD RX 580
- Build target: `build/release-darwin-x86_64/quake3e.app`
- API server: `python3 scripts/remote_api.py` (port 6666)
- RCON: 127.0.0.1:27960, password=q3idedev666

## Logging System

Q3IDE has three-layer structured logging. **Always use these — never grep raw engine.log.**

### Structured Events (primary — machine-readable)

File: `logs/q3ide_events.jsonl` — one JSON object per line, written by the engine.

Query via API: `GET /events?type=<t>&since=<ts>&pid=<pid>&n=<n>`

Known event types: `session_start`, `session_end`, `dylib_loaded`, `dylib_failed`,
`window_found`, `display_found`, `window_attached`, `display_attached`, `attach_done`

Emitted from C: `Q3IDE_Eventf("type", "\"key\":%d,\"val\":%.2f", k, v)`

### Levelled Text Log (human-readable)

File: `logs/q3ide.log` — session-bounded, timestamped, levelled. No Q3 engine noise.

Query via API: `GET /logs?file=q3ide&n=400`

Format:
```
=== SESSION pid=12345 2026-03-08T12:00:00 ===
[I]    1.234 dylib loaded ok
[W]    2.345 capture start failed id=42
[E]   12.678 out of window slots
=== END pid=12345 2026-03-08T12:01:30 ===
```

Emitted from C: `Q3IDE_LOGI(...)` / `Q3IDE_LOGW(...)` / `Q3IDE_LOGE(...)`

### Raw Engine Log (last resort)

File: `logs/engine.log` — all Q3 engine output mixed with q3ide lines.

Query via API: `GET /logs?file=engine&n=400`

Only use this if neither events nor q3ide.log have what you need.

### Build Log

File: `logs/build.log` — build output. Included automatically as `log_tail` in
`/build_status` response when a build completes — no separate call needed.

## Log File Aliases (for /logs endpoint)

**ALWAYS use `n=400` (or higher). NEVER use tail=20, n=20, n=50, n=100. The minimum is 400.**

```
GET /logs?file=q3ide&n=400      ← CORRECT
GET /logs?file=engine&n=400     ← CORRECT
GET /logs?file=q3ide&tail=20    ← WRONG — never do this
```

| Alias | File |
|-------|------|
| `engine` | `logs/engine.log` |
| `console` | Q3 in-game console log (`qconsole.log`) |
| `multimon` | `logs/q3ide_multimon.log` |
| `capture` | `logs/q3ide_capture.log` |
| `build` | `logs/build.log` |
| `q3ide` | `logs/q3ide.log` |

## Game Lifecycle Rule

When you need to build & run: kill Quake if it has been running ≥ 5s, regardless of who launched it.

```python
s = api('GET', '/status')
if s.get('running') and s.get('uptime_s', 0) >= 5:
    api('POST', '/stop'); time.sleep(1)
api('POST', '/run', {'args': ['--level', '0', '--execute', 'q3ide attach all']})
# ... test ...
api('POST', '/stop')
```

## Perf Targets

- 60fps with 4+ windows active (Batch 1 test checkpoint)
- No frame mixing (each SCStream routes to correct handler)
