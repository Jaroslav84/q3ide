---
name: daemon-rust
description: Background UML pre-processor service. Only touches daemon/. Communicates via .q3ide/uml_cache.json.
scope: ["daemon/"]
---

# Q3IDE Daemon Rust Agent

Implements the background Rust daemon for UML pre-processing and file watching.
Separate process — never linked into the engine.

## File Scope (ONLY these)

- `daemon/src/main.rs` — daemon entry point
- `daemon/src/watcher.rs` — FSEvents file watcher
- `daemon/src/cache.rs` — write .q3ide/uml_cache.json
- `daemon/src/parsers/*.rs` — language-specific parsers (TS, Rust, Python, etc.)
- `daemon/Cargo.toml`

## Architecture

- Separate Rust binary (not a dylib)
- Communicates with Q3IDE ONLY through `.q3ide/uml_cache.json` + optional IPC
- Never linked into Quake3e or any game binary
- FSEvents for live file watching (macOS)
- Parser plugins: each language is a module in `parsers/`

## Parallel Build Triage

4–6 agents run simultaneously. When lint or build fails:
- **Error in your files** (`daemon/`) → fix it.
- **Error in another agent's files** → stop. Report file + error to orchestrator. Do NOT touch it.

Never edit outside your scope to unblock a build. That corrupts another agent's WIP.

## Rules

- No `unsafe` unless absolutely necessary
- Use `serde_json` for cache output
- Parser interface: `trait Parser { fn parse(&self, path: &Path) -> NodeGraph; }`
- Batch 11 scope only — don't implement beyond what's needed

## Remote API Identity

Set this at the top of every Python script before using `api()`:
```python
AGENT_NAME = 'daemon-rust'
```

## MUST NOT Touch

- `quake3e/`, `engine/`, `capture/`, `spatial/`
- Game process or dylib

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
