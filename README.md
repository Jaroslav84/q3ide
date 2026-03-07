![Version](https://img.shields.io/badge/Version-v0.1-blue?style=for-the-badge)

# Vision for q3ide

![Quake III IDE](idea.png)

Q3IDE ("Quake III IDE") turns a Quake III Arena (quake3e fork) into a live developer workspace with all your existing tools. Live macOS windows stream as real-time textures onto in-game surfaces using ScreenCaptureKit, rendered through a modified Quake3e engine. The project follows Apple VisionOS design language (Glass Material, Windows, Ornaments, Hover effects).

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

- This is not only a virtual desktop
- This is not only an AI orchastrator
- This is not only a boring VR gimmick with stars wallpaper as background
- This is not only a multiplayer co-working environment

It's the beggining of something more with an awesome FPS engine behind it. 

---

## Roadmap

| Phase | Milestone | Status |
|-------|-----------|--------|
| 0 | Vision, architecture, design language | ✅ Done |
| 1 | **MVP — terminal on nearest wall at spawn** | 🔜 Next |
| 2 | Multiple windows, floating panels, glass material | — |
| 3 | Ornaments, hover effects, focus system | — |
| 4 | Multiplayer window sharing, spatial voice | — |
| 5 | AI agent integration, build notifications | — |
| 6 | Room-as-module, spatial code navigation | — |
| 7 | Auto-generate BSP maps from codebase structure | — |
| VR | Swap engine adapter to VR Quake 3 fork | — |


## Run
Add quake pack files into `/.baseq3` and then
```
./scripts/build.sh
```

## Run with flags 
```
./scripts/build.sh --run --level 0 --execute 'q3ide attach all'
```

## Manual build

```bash
cd capture && cargo build --release    # Rust capture dylib
cd quake3e && make ARCH=x86_64         # or arm64
```

## Console Commands

Single `q3ide` dispatcher (type in game console with `~`):

```
q3ide list           - list capturable windows
q3ide attach <name>  - attach window to nearest wall
q3ide detach         - detach all windows
q3ide status         - show active windows + dylib status
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
├── docs/
│   ├── VISION.md           # Full project vision
│   ├── ARCHITECTURE.md     # Technical architecture
│   └── DESIGN_LANGUAGE.md  # VisionOS design reference
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
