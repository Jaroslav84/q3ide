# Q3IDE — Quake III IDE

## Project Identity

You are working on **q3ide** (Quake III IDE), codename **"Quake III IDE"**. This is the first post-AI developer workplace — a multiplayer FPS environment where live desktop windows are streamed as textures onto in-game surfaces, turning a Quake 3 map into a spatial coding environment.

The core loop: AI agents work on code while developers frag each other and monitor progress on in-world screens. When a task completes, the developer reviews the diff on the wall, approves, gives the next task, and goes back to fragging.

Repository: `q3ide`

---

## Architecture Layers

```
┌─────────────────────────────────────────────────┐
│                  q3ide binary                    │
│                                                  │
│  ┌────────────┐   ┌──────────────────────────┐  │
│  │  Engine     │◄──│  q3ide-capture (Rust)     │  │
│  │  Adapter    │   │                          │  │
│  │  (trait)    │   │  • ScreenCaptureKit      │  │
│  │             │   │  • Window enumeration     │  │
│  │  Currently: │   │  • Frame ring buffer      │  │
│  │  Quake3e    │   │  • C-ABI exports          │  │
│  └──────┬─────┘   └──────────┬───────────────┘  │
│         │                    │                   │
│  ┌──────▼────────────────────▼───────────────┐  │
│  │         q3ide-spatial (Game Logic)         │  │
│  │                                            │  │
│  │  • Panel system (Windows, Ornaments)       │  │
│  │  • Placement engine (wall trace, floating) │  │
│  │  • Focus system (crosshair → hover)        │  │
│  │  • Console commands (/q3ide_*)             │  │
│  └────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────┘
```

The engine layer is an abstraction. Today it's Quake3e. Tomorrow it may be a VR fork of Quake 3 or another spatial engine. All engine-specific code lives behind the adapter trait. Game logic and capture logic never call engine internals directly.

---

## Engine Abstraction

The engine is **swappable**. A VR version of Quake 3 is being developed separately and may become the target engine in the future. All code must respect this boundary.

### Engine Adapter Interface

```c
// engine_adapter.h — abstract interface to the rendering engine
// Quake3e implements this today. VR engine implements it tomorrow.

typedef struct {
    // Texture management
    int   (*texture_create)(int width, int height, int format);
    void  (*texture_upload)(int tex_id, const unsigned char *pixels, int w, int h);
    void  (*texture_destroy)(int tex_id);

    // Surface placement
    void  (*surface_create)(int id, float x, float y, float z, float w, float h);
    void  (*surface_set_texture)(int surface_id, int tex_id);
    void  (*surface_set_transform)(int surface_id, float *matrix4x4);
    void  (*surface_destroy)(int surface_id);

    // World queries
    int   (*trace_nearest_wall)(float *origin, float *direction, float *hit_pos, float *hit_normal);
    void  (*get_spawn_origin)(float *origin);
    void  (*get_view_angles)(float *angles);

    // Render hooks
    void  (*on_frame)(float delta_time);  // called each render frame
} engine_adapter_t;
```

### Rules
- **NEVER** call `qgl*`, `trap_*`, `cg_*`, or any Quake3e function outside the adapter.
- **NEVER** include Quake3e headers in capture or spatial logic.
- **ALL** engine interaction goes through `engine_adapter_t`.
- The adapter is registered at startup and can be swapped at compile time via `#define Q3IDE_ENGINE_QUAKE3E` / `#define Q3IDE_ENGINE_VR`.

---

## Design Language: Apple VisionOS

All spatial UI follows Apple VisionOS terminology and design patterns. This is non-negotiable. When the engine moves to VR, the design language will already be native.

### Terminology — Use These Terms Everywhere

| Term | Meaning in q3ide |
|------|-----------------|
| **Window** | A captured desktop window rendered as a textured panel in the game world. Always has glass material background. |
| **Glass Material** | Semi-transparent frosted background on all Windows. Never use solid/opaque backgrounds. |
| **Window Bar** | The grab handle at the bottom of a floating Window. Contains title, close, pin, visibility controls. |
| **Ornament** | A floating control strip attached to a Window's edge. Sits slightly in front on the z-axis. Overlaps the parent Window edge by `Q3T_ORNAMENT_OVERLAP` units. |
| **Volume** | A bounded 3D container for non-flat content (git graphs, schema visualizations, build pipelines). Future scope. |
| **Space** | The game map environment. Equivalent to visionOS Full Space. |
| **Shared Space** | Multiplayer mode — multiple players' Windows coexist. |
| **Hover Effect** | Visual feedback when crosshair aims at an interactive element. Brightness increase + z-lift. |
| **Focus** | The currently aimed-at element. Crosshair = eye tracking equivalent. |
| **Passthrough** | The game world itself. Not a camera feed — the rendered BSP environment. |
| **Dimming** | Reducing world brightness to draw attention to a modal/sheet. |

### Design Tokens

```c
// q3ide_design.h — spatial design tokens
// Derived from Apple VisionOS Human Interface Guidelines

// Window Geometry
#define Q3IDE_CORNER_RADIUS         12.0f
#define Q3IDE_WINDOW_BAR_HEIGHT     32.0f
#define Q3IDE_WINDOW_BAR_OVERLAP    20.0f

// Ornaments
#define Q3IDE_ORNAMENT_Z_OFFSET     8.0f
#define Q3IDE_ORNAMENT_OVERLAP      20.0f

// Hover / Focus
#define Q3IDE_HOVER_GLOW            1.15f
#define Q3IDE_HOVER_LIFT_Z          4.0f
#define Q3IDE_HOVER_FADE_IN_SEC     0.15f
#define Q3IDE_MIN_TAP_TARGET        60.0f

// Animation
#define Q3IDE_ANIM_SPAWN_SEC        0.35f
#define Q3IDE_ANIM_CLOSE_SEC        0.25f
#define Q3IDE_ANIM_GRAB_SCALE       1.02f
#define Q3IDE_ANIM_LERP_SPEED       8.0f

// Status Tints (on glass, subtle)
#define Q3IDE_STATUS_PASS           {0.2f, 0.9f, 0.4f, 0.15f}
#define Q3IDE_STATUS_FAIL           {1.0f, 0.3f, 0.3f, 0.15f}
#define Q3IDE_STATUS_BUILD          {1.0f, 0.8f, 0.2f, 0.15f}
```

### Design Rules
1. **Glass, not solid.** Every Window background is semi-transparent glass. Never opaque.
2. **Ornaments, not HUDs.** Controls float on Ornaments attached to Windows. Never use a fixed-screen HUD overlay for q3ide UI.
3. **Hover everything.** Every interactive element glows on crosshair aim. No invisible hit targets.
4. **Depth communicates hierarchy.** Active Windows advance toward the player. Inactive recede. Ornaments always float in front of their parent.
5. **Animate transitions.** Windows spawn with a smooth scale-in. Windows close with a smooth fade+shrink. No instant pop/vanish.
6. **Spatial audio.** Notifications have position. A build completing in the database room is heard from the database room.

---

## Capture Backend

### macOS (primary): ScreenCaptureKit

```
ScreenCaptureKit (macOS 12.3+)
    → SCStream per window
    → CMSampleBuffer callback on dispatch queue
    → IOSurface → pixel copy to ring buffer slot (BGRA8)
    → Engine polls per frame → texture upload
```

**Crate:** `screencapturekit-rs` for Rust bindings.

**Optimization path:** Skip pixel copy entirely via `CGLTexImageIOSurface2D` (OpenGL) or `VK_EXT_metal_objects` (Vulkan/MoltenVK) for zero-copy GPU→GPU transfer. Investigate in Phase 2.

### Future backends
- **Linux:** PipeWire + wlroots screen copy protocol
- **Windows:** DXGI Desktop Duplication API

Capture code is behind a trait:

```rust
pub trait CaptureBackend {
    fn list_windows(&self) -> Vec<WindowInfo>;
    fn start_capture(&mut self, window_id: u32, target_fps: u32) -> Result<()>;
    fn stop_capture(&mut self, window_id: u32);
    fn get_frame(&self, window_id: u32) -> Option<FrameData>;
    fn shutdown(&mut self);
}
```

---

## MVP Scope

### What MVP Does

**One thing only:** When the player spawns, their iTerm2 (or any terminal) window appears on the nearest wall as a live-updating texture.

That's it. View only. No interaction. No floating panels. No ornaments. No multiplayer sharing. Just: terminal on wall, updating every frame.

### MVP Components

```
1. q3ide-capture (Rust dylib)
   - Enumerate windows via SCShareableContent
   - Find terminal window (iTerm2 / Terminal.app / configurable)
   - Start SCStream at game framerate
   - Write BGRA frames to ring buffer
   - Expose C-ABI: q3ide_init(), q3ide_get_frame(), q3ide_shutdown()

2. Engine adapter (Quake3e implementation)
   - Load dylib at engine startup
   - Create one texture slot
   - On spawn: trace from spawn origin to find nearest wall surface
   - Create textured quad on that wall
   - Per frame: poll q3ide_get_frame(), upload to texture via glTexSubImage2D

3. Wall placement (minimal)
   - On spawn, cast ray forward from spawn point
   - Hit BSP wall → place quad at hit position, aligned to hit normal
   - Size: proportional to captured window aspect ratio
   - No glass material yet, no ornaments yet — just raw texture on wall
```

### MVP Console Commands

```
/q3ide_attach <window_title>   — attach a window by title substring match
/q3ide_detach                  — stop capture and remove wall texture
/q3ide_list                    — list available windows
/q3ide_status                  — show capture status (fps, resolution, window)
```

### MVP File Structure

```
q3ide/
├── capture/                    # Rust dylib
│   ├── Cargo.toml
│   ├── src/
│   │   ├── lib.rs             # C-ABI exports
│   │   ├── backend.rs         # CaptureBackend trait
│   │   ├── screencapturekit.rs # macOS SCK implementation
│   │   ├── ringbuf.rs         # Lock-free frame ring buffer
│   │   └── window.rs          # Window enumeration
│   └── cbindgen.toml          # Generate q3ide_capture.h
│
├── engine/
│   ├── adapter.h              # Engine adapter interface
│   └── quake3e/               # Quake3e adapter implementation
│       ├── q3ide_adapter.c    # Implements engine_adapter_t for Quake3e
│       ├── q3ide_texture.c    # Texture creation/upload via Q3e renderer
│       └── q3ide_placement.c  # Wall trace and quad placement
│
├── spatial/                   # Engine-agnostic game logic (mostly post-MVP)
│   ├── panel.h                # Window/Panel data structures
│   ├── placement.h            # Placement algorithms
│   └── focus.h                # Crosshair focus system
│
├── q3ide_design.h             # Design tokens (VisionOS constants)
├── q3ide_main.c               # Init, per-frame update, shutdown
└── README.md
```

### MVP Success Criteria

- [ ] `q3ide_list` shows available macOS windows in Q3 console
- [ ] `q3ide_attach iTerm` starts capturing iTerm2 window
- [ ] On spawn, a textured quad appears on the nearest wall
- [ ] The texture updates live at game framerate
- [ ] Resizing the terminal window updates the quad aspect ratio
- [ ] `q3ide_detach` removes the quad and stops capture
- [ ] No engine crash, no memory leak, no frame drops below 60fps
- [ ] Capture dylib can be unloaded cleanly

### What MVP Does NOT Do

- No floating panels (Phase 2)
- No glass material / VisionOS styling (Phase 2)
- No ornaments (Phase 2)
- No hover effects (Phase 2)
- No window interaction / click-through (Phase 3)
- No multiplayer window sharing (Phase 4)
- No room-as-module mapping (Phase 4)
- No AI agent integration (Phase 5)
- No auto map generation from codebase (Phase 6)

---

## Code Style

- **Rust** for capture layer. No `unsafe` outside the C-ABI boundary. Use `crossbeam` for lock-free primitives.
- **C99** for engine adapter and integration. Match Quake3e code style (tabs, K&R braces, `snake_case`).
- **Comments:** Use VisionOS terminology in all comments. Say "Window" not "panel". Say "Ornament" not "toolbar". Say "Glass Material" not "transparent background".
- **Naming:** All q3ide symbols prefixed with `q3ide_`. All design tokens prefixed with `Q3IDE_`. All console commands prefixed with `/q3ide_`.
- **No global state** in capture layer. All state in a struct passed via C-ABI handle.
- **Error handling:** Rust side returns Result, C-ABI side returns error codes. Never panic across FFI boundary.

---

## Build

```bash
# Build capture dylib
cd capture && cargo build --release
# Output: target/release/libq3ide_capture.dylib

# Generate C header
cbindgen --config cbindgen.toml --crate q3ide-capture --output ../q3ide_capture.h

# Build Quake3e with q3ide patches
cd ../quake3e && make ARCH=arm64  # or x86_64
# Dylib is loaded at runtime, not linked
```

---

## Phase Roadmap (for context, not for MVP)

| Phase | What | Key VisionOS Concepts Used |
|-------|------|---------------------------|
| **1 (MVP)** | Terminal on nearest wall at spawn | Surface placement |
| **2** | Multiple windows, floating panels, glass material | Window, Glass Material, Window Bar |
| **3** | Ornaments, hover effects, focus system | Ornament, Hover Effect, Focus |
| **4** | Multiplayer window sharing, voice | Shared Space, Spatial Audio |
| **5** | AI agent integration, notifications | Status tints, announcer |
| **6** | Room-as-module, spatial code navigation | Full Space, Volumes, Portals |
| **7** | Auto-generate BSP from codebase structure | Procedural Space generation |
| **VR** | Swap engine adapter to VR Quake 3 fork | Native VisionOS parity |

---

## Important Context

- **Platform:** macOS first. Developer uses a Hackintosh with Intel CPU + AMD RX 580.
- **ScreenCaptureKit** requires macOS 12.3+ and Screen Recording permission.
- **screencapturekit-rs** exists and has working examples including Metal texture streaming and Bevy integration.
- **Quake3e** uses SDL on macOS. Renderer is modular (OpenGL and Vulkan switchable via `\cl_renderer`).
- **IOSurface zero-copy** path (`CGLTexImageIOSurface2D`) is the performance holy grail — investigate but don't block MVP on it.
- The developer is a visual/spatial thinker. Code architecture as physical space is the core insight, not a gimmick.
- The developer has deep Rust experience (building SHAZAMP) and 25+ years of sysadmin/hacking experience.

---

*When in doubt, remember: the game world is the operating system. The Ornament is the toolbar. The crosshair is the eye. Glass, not solid. Depth, not flat. Spatial, not tabbed.*
