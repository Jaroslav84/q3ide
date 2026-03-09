![Version](https://img.shields.io/badge/Version-v0.1-blue?style=for-the-badge)

# Vision for q3ide

![Quake III IDE](idea.png)

Q3IDE ("Quake III IDE") turns a Quake III Arena (quake3e fork) into a live developer workspace with all your existing tools on all of your monitors. Live macOS windows stream as real-time textures onto in-game surfaces using ScreenCaptureKit, rendered through a modified Quake3e engine. The project follows Apple VisionOS design language (Glass Material, Windows, Ornaments, Hover effects).

## Vibe Coding? Pfff! Frag Coding! 

- Image that you are inside Quake III Arena on 3 monitors or VR. 
- All your macOS/terminal/IDE is mirrored inside the game. 
- Each room, each wall has files which makes everything have a special meaning in your projects and codebase. It's all loci! It's all 3D and fun.
- Multiple AI agents and clawbots are running, coding, and shooting inside the game. 
- You are not waiting because you're fragging the shit out of your co-worker. 
- You setup an office desk in the corner with your radio station because Claude Code is ready. 
- You Run, Test, Build then lauch a rocket under the desk of your boss. 
- Then you tell the OpenClaw game bot to order pizza before rail gunning him too.

Zuk got it wrong with Meta world. We need some blood here for the love of god.

Yeah. That's my vision. Can you imagine? I'm beliver. 

- This is not only a virtual desktop, IDE
- This is not only an LLM orchastrator with multi-project support
- This is not only a new way (UML class/component diagrams) to ineract with your code
- This is not only a boring VR gimmick with stars wallpaper as background
- This is not only a multiplayer co-working environment

It's the beggining of something more with an awesome FPS engine behind it. 

---
## Roadmap

| Phase | Milestone | Batch | Status |
|-------|-----------|-------|--------|
| 0 | Vision, architecture, design language | 0 | ✅ Done |
| 1 | **MVP — terminal on nearest wall at spawn** | 0 | ✅ Done |
| 2 | **Multiple windows, floating panels** | 0 | ✅ Done |
| 3 | **Multi-window capture fix (unique windowID per SCStream)** | 0 | ✅ Done |
| 4 | **Three-monitor support** | 0 | ✅ Done |
| 5 | Window Entity data model & lifecycle management | 1 | — |
| 6 | Interaction model — Pointer Mode, Keyboard Passthrough, dwell detection | 2 | — |
| 7 | IOSurface zero-copy optimization | 3 | — |
| 8 | Window management — drag, resize, lock, snap, persist layouts | 4 | — |
| 9 | Grapple Hook, minimap, File Browser, Quick Open | 5 | — |
| 10 | Theater Mode, Office Mode, Control Center | 6 | — |
| 11 | Spaces (ASK→GARAGE) & Portal navigation | 7 | — |
| 12 | Programmable hotkeys, virtual keyboard, screenshots, video recording | 8 | — |
| 13 | Glass Material, Ornaments, Vibrancy, context menus | 9 | — |
| 14 | Project file classification & live filesystem scanning | 10 | — |
| 15 | UML Navigator — 3D architecture diagrams, node clouds, animated pipes | 11 | — |
| 16 | AI agent orchestrator — spawn, diff viewer, approve/reject, dashboard | 12 | — |
| 17 | Spatial audio, per-Window audio, ducking, notifications | 13 | — |
| 18 | Multiplayer — window sharing, proximity resolution, pair programming | 14 | — |
| 19 | quakeOS — native rendering, syntax highlighting, nano editor, focus mode | 15 | — |
| 20 | Game modes — synchronized rounds (CODE→FRAG→TEST→RUN) | 16 | — |
| 21 | Map skins, Office Mode styles, Volume baseplate | 17 | — |
| 22 | Spatialized voice chat, multiplayer audio | 18 | — |
| 23 | Session recording & async playback | 18 | — |
| 24 | Custom Q3 map designed for 8 Spaces | 19 | — |
| 25 | OpenClaw bot — fragging AI colleague with chat Window | 20 | — |
| 26 | AI runtime geometry — props + structural mesh on top of BSP | 21 | — |
| 27 | Browser-ready WASM port via Emscripten | 22 | — |
| VR | Swap engine adapter to VR Quake 3 fork | VR | — |

See [`plan/00-VISION.md`](./plan/00-VISION.md) for full roadmap.

---


## Quick Start

Add Quake 3 pack files into `/baseq3/` then:

```bash
sh ./scripts/build.sh --run --level 0 --execute 'q3ide attach all'
```

---

## `build.sh` Options

```
sh ./scripts/build.sh [options]
```

| Flag | Description |
|------|-------------|
| `--run` | Launch the game after a successful build |
| `--clean` | Run `make clean` before building (full rebuild) |
| `--engine-only` | Skip Rust dylib build — only recompile the engine (faster iteration). **Never combine with `--clean`** — clean deletes the dylib and engine-only won't copy it back. |
| `--api` | Start the Remote API server (`scripts/remote_api.py`) in the background before launching |
| `--level <map>` | Map to load. Shorthand: `0`→`q3dm0`, `7`→`q3dm7`, or full name like `q3dm17`. Default: whatever is in `autoexec.cfg` |
| `--execute '<cmd>'` | Console command(s) to run after the map loads (~60 frames after spawn). Supports semicolons: `'q3ide attach all; set cg_drawFPS 1'` |
| `--bots <n>` | Add N bots to the game (sets `bot_minplayers` to N+1) |
| `--music` | Enable random background music track on q3dm0 |

### Examples

```bash
# Full build + run on q3dm0, attach all macOS windows to walls
sh ./scripts/build.sh --run --level 0 --execute 'q3ide attach all'

# Engine-only rebuild (skip Rust, fast) + run
sh ./scripts/build.sh --engine-only --run

# Clean full rebuild
sh ./scripts/build.sh --clean --run --level 7

# Build + start API + run with bots
sh ./scripts/build.sh --run --api --level 0 --bots 3

# Mirror macOS displays into the 3 game monitors
sh ./scripts/build.sh --run --level 0 --execute 'q3ide desktop'
```

---

## Remote API (`scripts/remote_api.py`)

Run on macOS: `python3 scripts/remote_api.py` (or use `--api` flag above).

Agents / Claude Code call it from Docker at `http://host.docker.internal:6666`.

### Endpoints

| Method | Path | Body / Params | Description |
|--------|------|---------------|-------------|
| `POST` | `/build` | `{"args": [...]}` | Queue a build. Same flags as `build.sh` (e.g. `["--engine-only"]`) |
| `GET` | `/build/status` | — | Current build status + queue depth |
| `GET` | `/queue` | — | Full build queue (pending + history) |
| `DELETE`| `/queue` | — | Cancel pending builds + kill running build |
| `POST` | `/run` | `{"args": [...]}` | Launch the game. Optional `build.sh` args. Kills any running instance first (lock-protected — never spawns two). |
| `POST` | `/stop` | — | Kill the running game process |
| `POST` | `/kill` | — | Alias for `/stop` |
| `GET` | `/status` | — | Game running state, PID, uptime |
| `POST` | `/console` | `{"cmd": "..."}` | Send RCON command, returns response |
| `GET` | `/logs` | `?file=engine&n=100` | Tail log file. `file`: `engine`, `q3ide`, `capture`, `build`, `multimon` |
| `GET` | `/events` | — | Last 100 structured events (JSON lines) |
| `POST` | `/lint` | — | Run `scripts/lint.sh`, returns output |
| `GET` | `/lint` | — | Same |
| `WebSocket` | `/ws` | `?logs=engine,q3ide` | Stream log lines + status heartbeat (5s). Send `{"cmd":"..."}` for RCON. Receives `game_stopped` event when game exits. |

### Examples

```bash
# Build engine only
curl -X POST http://localhost:6666/build -H "Content-Type: application/json" \
  -d '{"args":["--engine-only"]}'

# Launch game on q3dm7 with 2 bots
curl -X POST http://localhost:6666/run -H "Content-Type: application/json" \
  -d '{"args":["--level","7","--bots","2"]}'

# Run RCON command
curl -X POST http://localhost:6666/console -H "Content-Type: application/json" \
  -d '{"cmd":"q3ide status"}'

# Tail last 50 lines of game log
curl "http://localhost:6666/logs?file=engine&n=50"

# Kill the game
curl -X POST http://localhost:6666/stop
```

---

## In-Game Console Commands

Open with `~`. Single `q3ide` dispatcher:

| Command | Description |
|---------|-------------|
| `q3ide list` | List all capturable macOS windows |
| `q3ide attach all` | Attach iTerm2 / Terminal / browser windows to walls |
| `q3ide desktop` | Mirror each macOS display onto its corresponding game monitor |
| `q3ide detach` | Detach all windows |
| `q3ide status` | Show active windows, capture status, dylib info |
| `q3ide snap` | Snap mirror portal into the q3dm0 teleporter arch |

---

## Manual Build

```bash
cd capture && cargo build --release    # Rust capture dylib
cd quake3e && make ARCH=x86_64         # or arm64
```

## Platform Requirements

- macOS 12.3+ (ScreenCaptureKit dependency)
- Quake 3 Arena game data (`pak0.pk3` in `baseq3/`)
- Rust toolchain, Xcode Command Line Tools
- Screen Recording permission for engine binary

---




## How It Works

Each room in the map can represent a section of the codebase. Walk into the `auth/` room and the authentication code is on the walls. The database room has the schema. You build *spatial memory* of your architecture — the [Method of Loci](https://en.wikipedia.org/wiki/Method_of_loci) applied to software, where every module has a place and every place has meaning.


```
macOS Desktop                         Quake III Arena
┌──────────────┐                     ┌────────────────────────┐
│              │   ScreenCaptureKit  │                        │
│  iTerm2      │ ──── BGRA frames ──►│  Wall texture (live)   │
│  VSCode      │   per-window capture│                        │
│  Browser     │                     │  You, with a railgun   │
│              │                     │                        │
└──────────────┘                     └────────────────────────┘
```

A Rust dylib captures desktop windows via Apple's `ScreenCaptureKit` framework and streams pixel data into a lock-free ring buffer. The game engine polls the buffer each frame and uploads it as a dynamic texture onto a surface in the game world.

The engine layer is abstracted — Quake3e today, potentially a VR engine tomorrow.

---

## Current Status

**Pre-alpha. Architecture and design phase.**

The vision doc, architecture doc, design language reference, and project prompt are written. Code hasn't started yet.

### MVP Goal

When you spawn, all your terminal windows appears on the nearest wall and in front of you. Live. Updating at game framerate. View only.

That's it. Everything else comes after.

---

## Design Language

All spatial UI follows [Apple VisionOS](https://developer.apple.com/visionos/) terminology and design patterns:

| VisionOS | q3ide |
|----------|-------|
| Window | Live desktop panel (glass material, never opaque) |
| Ornament | Floating control strip attached to a Window edge |
| Glass Material | Semi-transparent frosted panel background |
| Hover Effect | Crosshair-aim glow and z-lift on interactive elements |
| Shared Space | Multiplayer — everyone's Windows coexist |
| Full Space | The Quake 3 map itself |

This is intentional. When the engine moves to VR, the design language will already be native.

---

## Architecture

```
q3ide/
├── capture/                # Rust dylib — ScreenCaptureKit wrapper
│   ├── src/
│   │   ├── lib.rs          # C-ABI exports
│   │   ├── backend.rs      # CaptureBackend trait
│   │   ├── screencapturekit.rs
│   │   ├── ringbuf.rs      # Lock-free frame buffer
│   │   └── window.rs       # Window enumeration
│   └── Cargo.toml
│
├── Scrips/    
│   └── build.sh            # Builds & runs Q3ide
|
├── engine/
│   ├── adapter.h           # Abstract engine interface
│   └── quake3e/            # Quake3e adapter implementation
│
├── spatial/                # Engine-agnostic game logic
│
├── plan/
│   ├── 00-VISION.md                       # Full project vision
│   ├── 01-Q3IDE_INITAL_PROMPT.md          # Initial project prompt / architecture
│   ├── 02-VISIONOS_DESIGN_LANGUAGE.md     # VisionOS design reference
│   ├── 03-Q3IDE_ORCHESTRATION.md          # Agent orchestration setup
│   ├── 04-Q3IDE_SPECIFICATION.md          # Full feature spec + tracker
│   ├── 05-Q3IDE_PERFORMANCE_OPTIMALIZATION.md  # Perf brainstorm
│   ├── 06-Q3_CONF.md                      # Quake 3 config presets
│   └── 07-Q3_HD_UPGRADE_PROCEDURE.md      # HD texture upgrade guide
│
└── README.md
```

**Key principle:** The engine is swappable. All engine-specific code lives behind an adapter trait. Capture and spatial logic never call engine internals directly. A VR fork of Quake 3 is being developed separately and may become the target engine.

---

## Tech Stack

| Layer | Technology |
|-------|-----------|
| Engine | [Quake3e](https://github.com/ec-/Quake3e) (OpenGL + Vulkan, macOS) |
| Capture | [ScreenCaptureKit](https://developer.apple.com/documentation/screencapturekit) via [screencapturekit-rs](https://github.com/svtlabs/screencapturekit-rs) |
| Bridge | Rust dylib with C-ABI, loaded at runtime |
| Frame transport | Lock-free ring buffer (`crossbeam`) |
| Texture upload | `glTexSubImage2D` (GL) / staging buffer (Vulkan) |
| Platform | macOS 12.3+ (Apple Silicon + Intel) |

---


## Prior Art

Nothing quite like this exists. Related projects occupy adjacent spaces:

- **[SimulaVR](https://github.com/SimulaVR/Simula)** — Linux VR desktop compositor on Godot. No game, no fun.
- **[Immersed](https://immersed.com/)** — VR multi-monitor workspace. Closed source, no game.
- **[xrdesktop](https://gitlab.freedesktop.org/xrdesktop/xrdesktop)** — Linux desktop windows as OpenVR overlays. Research-grade.
- **[RiftSketch](https://github.com/brianpeiris/RiftSketch)** — VR live coding toy for Three.js. Single user proof of concept.
- **[CodeCity](https://wettel.github.io/codecity.html)** — 3D city visualization of codebases. Static, not a game.

q3ide is the first to combine: live desktop streaming + multiplayer FPS + spatial code architecture + AI agent workflow.

---

## The Idea

IDEs flatten everything into a file tree. Tabs. More tabs. A spreadsheet pretending to be a creative tool.

The human brain is wired for spatial memory, not text memory. The [Method of Loci](https://en.wikipedia.org/wiki/Method_of_loci) — the 2000-year-old memory palace technique — works because we remember *places* better than *lists*. Research shows VR environments using this technique produce ~20% better recall, and that's with passive walking. Add adrenaline, competition, and social presence? The encoding goes deeper.

Six months from now, someone asks "where's the rate limiter?" Your brain doesn't think `src/middleware/ratelimit.ts`. It thinks *"second floor, the room with the railgun pickup, near the screen that shows the Redis dashboard."*

"Meet me in the database room" becomes a real sentence with a real location.

---

## License

[BSL 1.1](LICENSE) — source-available, no production use without permission. Converts to MIT after 4 years.

---

*Glass, not solid. Depth, not flat. Spatial, not tabbed.*
