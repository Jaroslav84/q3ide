# Q3IDE — Q3IDE Specification

---


## ⚠️ LLM Implementation Instructions

**This document is the full feature spec for Q3IDE. It contains ~150 features across 24 batches. DO NOT implement everything at once.**

### How to use this document:

1. **Read the entire document** to understand the full vision, terminology, and architecture.
2. **Check the Feature Tracker** to see what's done, what's in progress, and what's next.
3. **You will be told which batch to work on.** Only implement features in that batch.
4. **Before writing any feature code,** set up the agent infrastructure and file structure (see below).
5. **Use VisionOS terminology** in all code, comments, and variable names. Say "Window" not "panel". Say "Ornament" not "toolbar". See the Terminology table at the bottom.
6. **All engine-specific code** goes in `engine/quake3e/`. Never import engine headers in `spatial/` or `capture/`.
7. **All capture-specific code** goes in `capture/`. It exposes a C-ABI. Never import Rust code in `spatial/` or `engine/`.
8. **`spatial/` is engine-agnostic.** It talks to the engine through `engine/adapter.h`. It talks to capture through `q3ide_capture.h`. It never calls `qgl*`, `trap_*`, `cg_*`, or any engine/capture internals directly.
9. **After completing a batch,** verify against its 🧪 TEST CHECKPOINT before moving on.
10. **Commit after each feature** with a descriptive message. Commit after completing the full batch.

### Agent setup (do this FIRST if these agents below doesn't exist):

This project uses **custom agents** to enforce architecture boundaries. Before writing any code, create `.agents/agents/q3agents/` with five specialist agents. Each agent is scoped to one layer and must NEVER touch files outside its scope.


Create these agent files:

**`.agents/agents/q3agents/engine-adapter.md`** — Only touches `engine/quake3e/`, `engine/adapter.h`, and `quake3e/code/q3ide/`. Keeps the adapter minimal and swappable. Never modifies core Quake3e files outside `quake3e/code/q3ide/`.

**`.agents/agents/q3agents/capture-rust.md`** — Only touches `capture/` and `q3ide_capture.h`. Pure Rust. Exposes only C-ABI functions. Never imports engine or spatial headers.

**`.agents/agents/q3agents/spatial-c.md`** — Only touches `spatial/` (all subdirectories). Engine-agnostic. Talks to engine only through `engine/adapter.h`. Talks to capture only through `q3ide_capture.h`. Uses VisionOS terminology.

**`.agents/agents/q3agents/daemon-rust.md`** — Only touches `daemon/`. Separate Rust process. Communicates with Q3IDE through `.q3ide/uml_cache.json` + IPC. Never linked into the engine.

**`.agents/agents/q3agents/reviewer.md`** — Reviews code against architecture rules: boundary violations, file length (200 sweetspot, 400 max), core Quake changes, VisionOS terminology, feature completeness against this spec, test checkpoint readiness.

Each agent file should contain: the agent's name/description in YAML frontmatter, its exact file scope, its rules, and what it must NOT touch. See `./plan/03-Q3IDE_ORCHESTRATION.md` for the full agent file templates.

### Delegation rules (how to use agents and subagents):

When implementing a batch, **delegate work to the right agent based on which files are being modified:**

- **Files in `engine/`** → delegate to `engine-adapter` agent
- **Files in `capture/`** → delegate to `capture-rust` agent
- **Files in `spatial/`** → delegate to `spatial-c` agent
- **Files in `daemon/`** → delegate to `daemon-rust` agent
- **Code review** → delegate to `reviewer` agent

**Spawn subagents for parallel work.** When a batch has independent features (e.g., Batch 5 has Grapple Hook + Minimap + File Browser — three independent modules), spawn subagents to implement them in parallel:

- Each subagent gets ONE feature or ONE file group
- Subagents should be scoped to a single file or small group of related files
- Never let a subagent modify files outside its agent's scope
- After subagent completes, review its output before integrating
- Use `Ctrl+B` to background subagents and continue working

**Subagent model config:** Subagents run on Haiku (set via `CLAUDE_CODE_SUBAGENT_MODEL`). This is 12x cheaper than Sonnet for focused tasks like file scaffolding, grep, simple edits, and test stubs. The main session runs on Sonnet for architectural decisions and complex implementation.

**Do NOT use extended thinking** for routine work. The custom agents provide the context that thinking would otherwise figure out. Save thinking for genuinely complex architectural problems.

### Code quality rules:

- **Sweetspot 200 lines per file. Maximum 400.** If a file exceeds 400, split it. No exceptions.
- **Proper folder structure and class hierarchy from the start.** Design to support all features upfront so less refactoring is needed later. Stub out future modules with header comments.
- **Avoid changing core Quake engine files as much as possible.** All Q3IDE logic goes in `spatial/`, `capture/`, `daemon/`. Engine modifications should be minimal adapter hooks only.
- **Architecture must be swappable.** The Quake3e engine OR a VR engine might be used in the future. Keep `spatial/` completely decoupled from any engine internals. Less coherency between layers = more flexibility.

### Performance checkpoints:

At major milestones, run a **full performance measuring session** — FPS benchmarks, frame time histograms, GPU/CPU usage, texture upload bandwidth, **VRAM usage** (texture memory per Window, total allocation). Record results in `.q3ide/perf_history.json` to track FPS degradation and memory growth over time. Target resolution: 1080p baseline, scalable up. Performance checkpoints occur after:

- **Batch 1** (Window Entity & Rendering Pipeline) — baseline with all 10 optimization stages
- **Batch 7** (Spaces & Navigation) — multi-Space overhead measurement
- **Batch 11** (UML Navigator) — 3D overlay rendering impact
- **Batch 14** (Multiplayer) — network + rendering combined load
- **Batch 19** (Custom Map) — final pre-VR baseline

### File structure reorganization (do this FIRST):

The current file structure is flat. Reorganize it to match the **Target File Structure** section. Steps:

```bash
# Create the full directory skeleton
mkdir -p spatial/{window,space,ui,mode,nav,agent,project,input,audio,capture_tools,uml,multiplayer,bot,quakeos,ai_geometry}
mkdir -p daemon/src/parsers
mkdir -p baseq3/{shaders,sounds/notifications,models/office}
mkdir -p config
mkdir -p web/

# Move existing files to their new homes
mv spatial/panel.h spatial/window/entity.h      # rename: panel → window entity
mv spatial/placement.h spatial/window/           # placement is part of window system for now
mv spatial/placement.c spatial/window/
mv spatial/focus.h spatial/window/

# Create stub files for all future modules (empty with header comment)
# ... (generate all .h/.c pairs listed in Target File Structure)
```

**Every file in the Target File Structure should exist after reorganization** — either with real code (for completed features) or as a stub with a header comment describing what it will contain (for future features). This makes the architecture visible and prevents merge conflicts when multiple batches are worked on.

### Batch workflow:

```
1. Read ./plan/04-Q3IDE_SPECIFICATION.md → identify all features in the assigned batch
2. Plan implementation order (dependencies first)
3. Identify which custom agent handles which files
4. Implement features (parallel subagents where features are independent)
5. Run reviewer agent on all changes
6. Verify against 🧪 TEST CHECKPOINT
7. Run 📊 PERFORMANCE CHECKPOINT if this is a perf batch
8. Commit: git commit -m "Batch N: [feature summary]"
```

### Current batch will be specified when you receive this document.

---

## Feature Tracker

Progressive implementation batches. Each batch ends with a testing checkpoint. **Order = implementation priority.** Ground-up: performance and stability first, features on top.

### BATCH 0 — Foundation ✅ Done

The capture pipeline and basic texture rendering. Getting pixels on walls.

| # | Feature | Status | Description |
|---|---------|--------|-------------|
| 0.1 | ScreenCaptureKit single window capture | ✅ Done | Capture one window via SCK, deliver frames to ring buffer |
| 0.2 | Rust dylib C-ABI bridge | ✅ Done | `libq3ide_capture.dylib` loads, exports `q3ide_*` functions |
| 0.3 | GL texture upload (single window) | ✅ Done | Frames upload to OpenGL texture via `glTexSubImage2D` |
| 0.4 | Wall placement (single window) | ✅ Done | Trace from spawn, place textured quad on nearest wall |
| 0.5 | Console commands (`/q3ide_list`, `/q3ide_attach`, `/q3ide_detach`) | ✅ Done | Basic window management from Q3 console |
| 0.6 | Multi-window capture (unique windowID per SCStream) | ✅ Done | Fix frame mixing bug — **cause:** matching windows by app name gives the same window to every SCStream. **Fix:** each SCStream must have its own `SCContentFilter(desktopIndependentWindow:)` created with the unique `CGWindowID` (not app name or bundle ID). Multiple windows of the same app (e.g., 3 iTerm2 windows) each have a distinct `CGWindowID`. Key by that, not by `owningApplication`. |
| 0.7 | Three-monitor support | ✅ Done | Q3e spans 3 monitors, wall textures render across all |
| 0.8 | **Hybrid CaptureRouter** | ✅ Done | Two-mode capture backend — see below. +20% FPS gain. No stream limit. |

**🧪 TEST CHECKPOINT 0:** Multiple terminal windows from the same app display independently on walls. No frame mixing. Stable 60fps. No memory leaks. Clean dylib unload.

#### Hybrid CaptureRouter (✅ Implemented — `capture/src/router.rs`)

The capture backend routes each window to one of two modes at attach time:

**COMPOSITE** — 1 `SCStream` per physical display, shared by all windows on it. `get_frame()` crops the window's rect from the display pixel buffer using Y-flipped Quartz→pixel coordinates. No per-app camera notification. No stream count limit (~9–11 stream OS limit bypassed entirely). Best FPS — terminals share one display stream.

**DEDICATED** — 1 `SCStream` per window via `SCContentFilter.with_window()`. Captures the window's own GPU compositor layers in isolation, regardless of Z-order, occlusion, or minimized state. Required for any app using Metal/GPU rendering.

Resolution order at attach time:
1. Window is **minimized** (`!is_on_screen && frame < 200pt`) → DEDICATED (captures Dock buffer correctly)
2. App name in **`WHITELIST_COMPOSITE`** (terminals, native AppKit) → COMPOSITE
3. App name in **`WHITELIST_DEDICATED`** (browsers, IDEs, Electron, VLC) → DEDICATED
4. **Unknown app** → COMPOSITE + detector watches for empty/dark frames, logs warning

Detector thresholds (tunable in `router.rs`): 20 consecutive empty crops → warn. 30 consecutive dark frames → warn (GPU content not composited into display frame). No auto-switch yet — Phase 2.

**FPS caps: DO NOT ADD.** `Q3IDE_CAPTURE_FPS = -1` — Apple's content-driven model delivers frames only when content changes. Idle windows cost zero. Uncapped is smoother than any cap. The spec item 1.8 "SCK minimumFrameInterval" is **open question** — measure actual impact before implementing.

---

### BATCH 1 — Window Entity, Placement & Rendering Pipeline (CURRENT)

Window data model, placement system rewrite, and rendering pipeline optimizations. Implemented in 10 stages — see `PLACEMENT.md` for full details per stage. Each stage = one Claude Code session with git commit + testing between.

| # | Feature | Status | Description |
|---|---------|--------|-------------|
| 1.1 | Kill BGRA→RGBA swizzle | ✅ Done | Add format param to RE_UploadCinematic, pass GL_BGRA native. Delete CPU swizzle loop. |
| 1.2 | Visibility-gated texture uploads | ⬜ | Dot product (behind player?) + BSP trace (behind wall?) before UploadCinematic. Skip invisible windows. |
| 1.3 | Wall scanner + cache | ⬜ | Pre-scan all walls on area entry within 60m radius. Cache wall slots. Foundation for new placement. |
| 1.4 | Area transition placement | ⬜ | **Destroy old 13 rules.** New placement: spread-even across walls, closest first, FPS-gated drain (30fps), texture throttle 2fps during placement. |
| 1.5 | Within-area leapfrog | ⬜ | Furthest window jumps to closest free slot when player moves >7m. Check every 30 frames. Dogs stay put in small rooms. |
| 1.6 | Trained positions | ⬜ | User repositions window → save position per area. On return, window goes to trained spot. Dogs remember their place. |
| 1.7 | Adaptive resolution (8 tiers) | ⬜ | SCK source-side downscale. Tier 0 (full) to tier 7 (thumbnail). Aim-override: crosshair on window = full res from any distance. |
| 1.8 | Static detection + SCK frame interval + mipmaps | ⬜ | Idle windows → 1fps capture. SCK minimumFrameInterval per tier. glGenerateMipmap after upload. |
| 1.9 | Texture Array (GL_TEXTURE_2D_ARRAY) | ⬜ | One texture array per tier. Batched draw calls. 31 binds → ~8 binds. Renderer refactor — do LAST. |
| 1.10 | Per-window performance metrics | ⬜ | Track capture_fps, upload_fps, dirty_ratio, bandwidth, latency, vram, skip_count, tier, static_flag. Console + Widget. |

**🧪 TEST CHECKPOINT 1:** Walk between areas → windows migrate smoothly (10 dogs through a doorway). FPS stays above 30 during migration. Furthest windows leapfrog as you walk. Trained positions persist. Resolution scales with distance. Aim at distant window → full res. Idle terminals at 1fps. `/q3ide_perf` shows live per-window metrics. Performance Widget shows total bandwidth.
**📊 PERFORMANCE CHECKPOINT:** Measure FPS with 31+ windows before/after each stage. Record to `.q3ide/perf_history.json`. Target: 40+ FPS with 31 windows (from current 23 FPS).

---

### BATCH 2 — Interaction Model

Pointer Mode, Keyboard Passthrough, the core work/play boundary.

| # | Feature | Status | Description |
|---|---------|--------|-------------|
| 2.1 | Dwell detection (150ms) | ⬜ | Crosshair-on-Window timer, triggers Hover Effect |
| 2.2 | Hover Effect | ⬜ | Glow + z-lift on Window surface when dwell triggers |
| 2.3 | Pointer Mode | ⬜ | Mouse maps to Window coordinate space, click sends to captured app |
| 2.4 | Distance threshold | ⬜ | Pointer Mode only within readable distance |
| 2.5 | Edge Zone exit (20px) | ⬜ | Pointer pushes past Window border → return to FPS Mode |
| 2.6 | Keyboard Passthrough | ⬜ | Enter activates, all keys route to captured app |
| 2.7 | Escape — Digital Crown | ⬜ | Always exits any mode back to FPS |
| 2.8 | Left-click dual purpose | ⬜ | Fire weapon in FPS Mode, click in app in Pointer Mode |
| 2.9 | Weapon cosmetic fire in Pointer Mode | ⬜ | Weapon fires normally (animation, sound, projectile) but ammo never goes down — `give ammo` injected after every shot. Instant reload: firing through a Window triggers immediate `give ammo` so the weapon never pauses to reload. BUTTON_ATTACK is NOT suppressed; damage to bots/players is acceptable collateral. |

**🧪 TEST CHECKPOINT 2:** Aim at a terminal, dwell → hover glow appears. Click → clicks in terminal. Enter → type commands in terminal. Escape → back to fragging. Edge zone exit works. Weapon fires cosmetically through the Window. No accidental clicks during combat.

---

### BATCH 3 — Live Window Management

The world stays in sync with macOS automatically. No manual re-attach.

| # | Feature | Status | Description |
|---|---------|--------|-------------|
| 3.1 | New window detection | ✅ | `q3ide_poll_window_changes` diffs SCK snapshot every 2s; new terminal/browser windows auto-attach to nearest wall |
| 3.2 | Closed window detection | ✅ | Removed windows auto-detach; quad disappears within 2s |
| 3.3 | Auto-attach mode flag | ✅ | `auto_attach` enabled by `q3ide attach all`, cleared by `q3ide detach` |
| 3.4 | Window title change tracking | ⬜ | Re-label quad when captured window title changes (tab rename, shell cwd) |
| 3.5 | Minimized / hidden state | ⬜ | Detect when a window is minimized or hidden; fade quad to 30% opacity; restore on un-minimize |
| 3.6 | Dirty frame detection | ⬜ | Skip texture upload when captured window content is identical to last frame (compare CMSampleBuffer timestamps) |
| 3.7 | Status HUD | ⬜ | Small overlay quad showing live window count, capture health, idle count |

**🧪 TEST CHECKPOINT 3:** Open a terminal → appears on wall within 2s. Close it → disappears within 2s. Minimize it → fades. Idle terminal → 0 texture uploads. Status HUD is accurate.
**📊 PERFORMANCE CHECKPOINT:** Baseline with dirty-frame detection active. Record idle CPU%, texture upload rate, VRAM to `.q3ide/perf_history.json`.

---

### BATCH 4 — Window Placement & Layout

Precise placement, persistence, and navigation.

| # | Feature | Status | Description |
|---|---------|--------|-------------|
| 4.1 | Long press → drag | ⬜ | Hold click ~300ms on Window enters drag mode; move by aiming |
| 4.2 | Wall snap | ⬜ | While dragging: aim at wall → window snaps flush to surface |
| 4.3 | Float snap | ⬜ | Aim away from wall → window floats at fixed depth from player |
| 4.4 | Scroll → resize | ⬜ | Mouse wheel while focused resizes Window, preserves aspect ratio |
| 4.5 | Lock / Pin | ⬜ | Toggle lock on focused window — prevents accidental move/resize |
| 4.6 | Edge-to-edge snap | ⬜ | Windows snap edge-to-edge when dragged near a sibling |
| 4.7 | Layout persistence | ⬜ | Save window positions/sizes to `config/layout.json`; restore on restart |
| 4.8 | Tab cycling | ⬜ | Tab / Shift+Tab moves Focus across all active Windows |

**🧪 TEST CHECKPOINT 4:** Drag a window to a new wall — it snaps and sticks. Resize it. Lock it — drag does nothing. Layout saved and restored after game restart. Tab cycles focus.

---

### BATCH 5 — Grapple Hook & Spatial Tools

Core movement and navigation tools.

| # | Feature | Status | Description |
|---|---------|--------|-------------|
| 5.1 | Grapple Hook (`X`) — movement | ⬜ | Hook to surfaces, pull player |
| 5.2 | Grapple Hook → Window navigation | ⬜ | Grapple to Window = pull to perfect reading distance |
| 5.3 | Minimap Ornament (`M`) | ⬜ | Persistent minimap showing Spaces, players, activity |
| 5.4 | File Browser (`F`) | ⬜ | Floating Window with recent Projects + full list |
| 5.5 | Quick Open (`O`) | ⬜ | fzf-style fuzzy file search, as-you-type results (upgrade to vector DB later) |
| 5.6 | Run Project (`R`) | ⬜ | Auto-detect run command, output in RUN Space |
| 5.7 | Bookmarks | ⬜ | Save map positions, bind to hotkeys |

**🧪 TEST CHECKPOINT 5:** Grapple to a Window across the room — arrive at reading distance. Minimap shows your position. Quick Open finds files instantly. File Browser loads Projects. Bookmarks work.

---

### BATCH 6 — Theater Mode, Office Mode & Focus States

Comfort modes for focused work.

| # | Feature | Status | Description |
|---|---------|--------|-------------|
| 6.1 | Theater Mode (`T`) | ⬜ | Blackout world, curved panoramic Window wrap |
| 6.2 | Office Mode (`L`) | ⬜ | Spawn desk/monitors/chair around player |
| 6.3 | Control Center Widget | ⬜ | Persistent bottom-right: Space, agents, project, time |
| 6.4 | Control Center expanded (`C`) | ⬜ | Full settings panel: agents, windows, toggles, layouts |
| 6.5 | Billboard presentation style | ⬜ | Oversized, full-brightness, distance-readable |

**🧪 TEST CHECKPOINT 6:** Theater Mode blacks out world, wraps Windows around you. Office Mode spawns desk. Control Center shows status. Billboard visible from across the map.

---

### BATCH 7 — Spaces & Navigation

The 8 Spaces, teleportation, Portals.

| # | Feature | Status | Description |
|---|---------|--------|-------------|
| 7.1 | Space definitions (8 zones in map) | ⬜ | ASK through GARAGE mapped to map areas |
| 7.2 | Teleport (`Shift+1`–`Shift+8`) | ⬜ | Instant teleport to Space |
| 7.3 | Portal rendering (visual peek) | ⬜ | Render destination Space inside Portal frame, distance-adaptive quality. **Search Quake 3 community repos for existing Portal rendering implementations — don't build from scratch.** |
| 7.4 | Portal teleport (walk-through) | ⬜ | Step into Portal → teleport to destination |
| 7.5 | Space-aware Window assignment | ⬜ | Windows belong to a Space, only render in their Space |
| 7.6 | Smart rendering — Space-based | ⬜ | Pause capture for Windows in other Spaces |
| 7.7 | Home View (`H`) | ⬜ | Full overview of all Spaces, Projects, agents as Portal thumbnails |

**🧪 TEST CHECKPOINT 7:** Press Shift+1-8 to teleport between Spaces. Each Space has its own Windows. Portals show live preview. Walk through to teleport. Windows in other Spaces paused. Home View shows everything.
**📊 PERFORMANCE CHECKPOINT:** Multi-Space overhead. Compare FPS with 1 Space vs 8 Spaces. Portal rendering cost. Record to perf history.

---

### BATCH 8 — Programmable Hotkeys & Screenshots

| # | Feature | Status | Description |
|---|---------|--------|-------------|
| 8.1 | Virtual Keyboard (`K`) | ⬜ | Visual keyboard showing all bindings, color-coded |
| 8.2 | Hotkey rebinding | ⬜ | Click any key on virtual keyboard to reassign |
| 8.3 | Bookmark hotkeys | ⬜ | Bind map positions to key combos |
| 8.4 | Screenshot (`[`) | ⬜ | Capture Window or full view |
| 8.5 | Video recording (`]`) | ⬜ | Toggle session recording |
| 8.6 | Magic Mouse gestures | ⬜ | Swipe, pinch, force touch enhancements |

**🧪 TEST CHECKPOINT 8:** Press K, see all bindings, rebind a key. Screenshot saves. Video records. Magic Mouse gestures work in Pointer Mode.

---

### BATCH 9 — Ornaments, UI Chrome & Visual Polish

Ornaments, Vibrancy, context menus.

| # | Feature | Status | Description |
|---|---------|--------|-------------|
| 9.1 | Window Bar Ornament (bottom) | ⬜ | Title, close, pin, visibility controls |
| 9.2 | Top Ornament (file path breadcrumb) | ⬜ | File path / branch info above Windows |
| 9.3 | Side Ornament (status indicator) | ⬜ | Status dot, scrollbar |
| 9.4 | Sidebar Ornament (file tree) | ⬜ | Collapsible file tree on left edge |
| 9.5 | Face-the-player Ornaments | ⬜ | Ornaments billboard toward viewer |
| 9.6 | Distance-scaled Ornaments | ⬜ | Scale text/icons based on player distance |
| 9.7 | Vibrancy toggle (`Shift+T`) | ⬜ | Dynamic text contrast boost on Window backgrounds |
| 9.8 | Context menus (right-click) | ⬜ | File ops, git ops, agent ops, window ops |
| 9.9 | Chromeless Window option | ⬜ | No chrome — raw content on surface |

**🧪 TEST CHECKPOINT 9:** Ornaments show controls. Right-click opens context menu. Ornaments face the player. Vibrancy makes text legible over busy backgrounds.

---

### BATCH 10 — Project File Classification & Live Scanning

| # | Feature | Status | Description |
|---|---------|--------|-------------|
| 10.1 | Heuristic file classifier | ⬜ | Glob patterns, path conventions, extensions → Space assignment |
| 10.2 | Live filesystem watcher | ⬜ | Files move between Spaces as project structure changes |
| 10.3 | Window-to-Space auto-suggest | ⬜ | Hybrid: auto-suggest Space for new Windows, player confirms/overrides |
| 10.4 | Project scanner (`~/Projects/`) | ⬜ | Enumerate all projects, classify on load |

**🧪 TEST CHECKPOINT 10:** New terminal → Q3IDE suggests BUILD Space. File renamed `test_*` → moves to TEST Space. New project in ~/Projects/ appears in File Browser.

---

### BATCH 11 — UML Navigator

The architecture-first navigation system. 3D diagrams, node clouds, animated pipes, Transformer unfolding.

| # | Feature | Status | Description |
|---|---------|--------|-------------|
| 11.1 | UML pre-processor daemon | ⬜ | Background Rust process: file watcher + language-specific parser plugins |
| 11.2 | Parser: TypeScript/JavaScript | ⬜ | Extract classes, methods, imports, relationships from .ts/.js/.tsx/.jsx |
| 11.3 | Parser: Rust | ⬜ | Extract structs, impls, use statements, traits from .rs |
| 11.4 | Parser: Python | ⬜ | Extract classes, functions, imports from .py |
| 11.5 | UML cache output (`.q3ide/uml_cache.json`) | ⬜ | Daemon writes parsed graph data, Q3IDE reads it |
| 11.6 | Full UML view (`U` key) | ⬜ | 3D node cloud centered on current file, interactive |
| 11.7 | Node cloud recentering | ⬜ | Click node → cloud reshuffles with new center |
| 11.8 | 3D node box — Transformer unfold animation | ⬜ | Compact → methods → full params on progressive dwell |
| 11.9 | Pipe rendering (animated flow, thickness, styles) | ⬜ | Visual pipe styles per relationship type, particle flow |
| 11.10 | Mini UML in HUD bar | ⬜ | Always-visible compact node graph between health and ammo |
| 11.11 | Portal suck-in teleport from UML | ⬜ | Click node teleport button → vortex pulls player to file's Window |
| 11.12 | `N` key — spawn agent on UML node | ⬜ | Aim at node, press N → iTerm + Claude with file path |
| 11.13 | Diagram type switching (`Shift+U`) | ⬜ | Class Diagram ↔ Component Diagram ↔ Dependency Graph |
| 11.14 | Node status overlays (test red/green) | ⬜ | Nodes turn red/green matching test results for that file |
| 11.15 | Agent activity on nodes (dot + glowing pipe) | ⬜ | Status dot on node + glowing pipe to class agent is working on |
| 11.16 | Pipe interaction (hover highlights) | ⬜ | Aim at pipe → both nodes highlight + relationship details tooltip |
| 11.17 | Git diff overlay (changed nodes glow) | ⬜ | Nodes changed since last commit glow/pulse |
| 11.18 | Git blame coloring (author colors) | ⬜ | Nodes color-coded by last author |
| 11.19 | Git time slider | ⬜ | Scrub through history, watch UML architecture evolve over time |
| 11.20 | Multiplayer UML visibility | ⬜ | Other players see your UML diagram floating in front of you |
| 11.21 | UML Window spawning | ⬜ | Click node → opens file as new Floating Window at current position |

**🧪 TEST CHECKPOINT 11:** Press U → 3D UML appears, follows player. Aim → unfolds. Click → recenters. Click teleport → Portal vortex. Click node → new Floating Window spawns with file. N → Claude spawns. Mini UML in HUD updates on focus change. Pipes animate, interactive on hover. Red/green test status on nodes. Agent dots visible. Other players see your UML. Git slider scrubs history.
**📊 PERFORMANCE CHECKPOINT:** 3D overlay rendering impact. FPS with UML open vs closed. Node count vs frame time. Record to perf history.

---

### BATCH 12 — AI Agent Integration

LLM agent orchestration, diff viewer, approve/reject, dashboard.

| # | Feature | Status | Description |
|---|---------|--------|-------------|
| 12.1 | Agent data model in WindowEntity | ⬜ | agent_id, model, status, context_tokens fields |
| 12.2 | Agent spawning (auto-open terminals) | ⬜ | Q3IDE spawns iTerm, launches Claude Code/Aider, captures output |
| 12.3 | Native API agent (direct LLM calls) | ⬜ | Chat Window for simple tasks via Anthropic/OpenAI/Zhipu APIs |
| 12.4 | Agent status display on Windows | ⬜ | Status Ornament: thinking/writing/idle/error |
| 12.5 | Diff Viewer Window | ⬜ | Syntax-highlighted diff display, scrollable |
| 12.6 | Approve/Reject Ornament | ⬜ | [✓ Approve] [✗ Reject] [💬 Comment] [↻ Retry] buttons |
| 12.7 | Task Queue Billboard | ⬜ | Central Billboard: all agents, tasks, status, context usage, cost |
| 12.8 | Kill Feed notifications | ⬜ | Agent events scroll like frag notifications |
| 12.9 | Q3 Announcer integration | ⬜ | "BUILD COMPLETE" / "TESTS FAILING" in announcer voice |
| 12.10 | Room lighting from status | ⬜ | TEST Space red/green, BUILD Space amber during compilation |
| 12.11 | API key management | ⬜ | `.q3ide/.env` + Control Center, encrypted at rest, per-model config |
| 12.12 | War Room layout | ⬜ | Multi-agent command aesthetic for ASK/PLAN Spaces |

**🧪 TEST CHECKPOINT 12:** Spawn Claude Code from in-game. Diff appears when done. Approve it. Billboard shows status. Announcer fires. Room lights change.

---

### BATCH 13 — Audio & Notifications

| # | Feature | Status | Description |
|---|---------|--------|-------------|
| 13.1 | Spatial notification audio | ⬜ | Work events positioned in 3D from source Space |
| 13.2 | Per-Window audio | ⬜ | Each Window emits its app's audio from its position |
| 13.3 | Audio ducking | ⬜ | Focused Window full volume, others ducked 70% |
| 13.4 | Distinct work sound palette | ⬜ | Work sounds never confused with game sounds |
| 13.5 | Window pulse animation | ⬜ | Completed task = Window glow pulse (2 cycles) |

**🧪 TEST CHECKPOINT 13:** YouTube on wall = audio from wall. Focus terminal = YouTube ducks. Build complete = spatial notification + announcer.

---

### BATCH 14 — Multiplayer

| # | Feature | Status | Description |
|---|---------|--------|-------------|
| 14.1 | Per-Window visibility (Private/Public/Team) | ⬜ | Visibility flag in WindowEntity |
| 14.2 | Low-res thumbnail sync | ⬜ | Public Windows send compressed thumbnails over network |
| 14.3 | Proximity-based resolution | ⬜ | Walk closer to shared Window = higher resolution |
| 14.4 | Player Portal (visit coworker) | ⬜ | Portal to another player's Space |
| 14.5 | Pair Programming (permission grant) | ⬜ | Request/allow/deny control of another player's Window |
| 14.6 | Player presence on minimap | ⬜ | See teammate dots on minimap |

**🧪 TEST CHECKPOINT 14:** Two players on same server. Shared Window visible. Walk closer = higher res. Pair programming works.
**📊 PERFORMANCE CHECKPOINT:** Network + rendering combined load. FPS with 2+ players and shared Windows. Thumbnail bandwidth. Record to perf history.

---

### BATCH 15 — quakeOS Native Rendering

The native rendering library inside Q3IDE. Renders text, code, markdown, and images directly in the engine — no macOS app capture needed. A nano-level editor for quick changes without leaving Quake.

| # | Feature | Status | Description |
|---|---------|--------|-------------|
| 15.1 | FreeType/stb_truetype font renderer | ⬜ | Sharp scalable text rendering in-engine, monospace + proportional |
| 15.2 | Syntax highlighting engine | ⬜ | tree-sitter tokenizer for language-aware coloring (separate from UML daemon's class-level parsing) |
| 15.3 | Line numbers + git blame gutter | ⬜ | Left gutter with line numbers, optional blame coloring per line |
| 15.4 | Focus mode toggle (`.` key) | ⬜ | Aim at any captured code Window (FPS or Pointer Mode), press `.` → flips to quakeOS native render. Press `.` again → back to captured macOS. Press `.` on nothing → new empty file as Floating Window. |
| 15.5 | Scroll + search + jump-to-line | ⬜ | Mouse wheel scroll, Ctrl+F search, Ctrl+G jump-to-line in focus mode |
| 15.6 | Nano-level editor | ⬜ | Type, delete, undo, save. Basic text editing for quick fixes without leaving Quake |
| 15.7 | Markdown renderer | ⬜ | Render .md files with headings, bold, code blocks, links |
| 15.8 | Image viewer | ⬜ | Display .png/.jpg/.svg as textures on Windows natively |

**🧪 TEST CHECKPOINT 15:** Aim at VS Code Window showing a .ts file, press `.` → flips to quakeOS-rendered syntax highlighted code with line numbers. Scroll through it. Press Ctrl+F → search works. Type a change → file saves. Press `.` → back to VS Code capture. Open a markdown file → renders with formatting. Open a PNG → displays as texture.

---

### BATCH 16 — Game Modes (Future)

| # | Feature | Status | Description |
|---|---------|--------|-------------|
| 16.1 | Synchronized rounds (CODE→FRAG→TEST→RUN) | ⬜ | Structured multiplayer game mode |
| 16.2 | Scoreboard (frags + code metrics) | ⬜ | Combined scoring |
| 16.3 | File Portal (import → destination) | ⬜ | Mini-portal on import statements |

---

### BATCH 17 — Map Skins & Polish (Future)

| # | Feature | Status | Description |
|---|---------|--------|-------------|
| 17.1 | Map skins / environments | ⬜ | Same layout, different aesthetics (office, cyberpunk, space station) |
| 17.2 | Office Mode styles | ⬜ | Different office aesthetics |
| 17.3 | Volume baseplate | ⬜ | Space boundary indicators |

---

### BATCH 18 — Advanced Audio & Recording (Future)

| # | Feature | Status | Description |
|---|---------|--------|-------------|
| 18.1 | Spatialized voice chat | ⬜ | Distance-based voice between players |
| 18.2 | Multiplayer Window audio | ⬜ | Hear coworker's music from their Space |
| 18.3 | Session recording & playback | ⬜ | Record full sessions for async review |

---

### BATCH 19 — Custom Map (Future)

| # | Feature | Status | Description |
|---|---------|--------|-------------|
| 19.1 | Custom Q3 map for 8 Spaces | ⬜ | Purpose-built map with rooms designed for workflow |

---

### BATCH 20 — OpenClaw Bot Integration

AI agent as a game character. Frags, chats, works.

| # | Feature | Status | Description |
|---|---------|--------|-------------|
| 20.1 | OpenClaw bot player entity | ⬜ | Bot runs as a Quake 3 bot player with full movement, combat AI |
| 20.2 | OpenClaw API bridge | ⬜ | HTTP/WebSocket bridge: Q3IDE ↔ OpenClaw API (send/receive messages with token) |
| 20.3 | Dedicated chat Window | ⬜ | Floating Window with full conversation history, scrollable, typeable |
| 20.4 | Bot game AI (frag + navigate) | ⬜ | Bot plays the game — picks up weapons, frags other players/bots, navigates map |
| 20.5 | Bot responds while playing | ⬜ | Bot can frag AND respond to chat simultaneously — no pause for thinking |

**🧪 TEST CHECKPOINT 20:** OpenClaw bot spawns as a player, runs around, frags bots. Open chat Window, type a message, bot responds. Bot keeps fragging while chatting.

---

### BATCH 21 — AI Runtime Geometry (Moonshot)

AI-generated game world in real-time. Take that, Google Genie.

| # | Feature | Status | Description |
|---|---------|--------|-------------|
| 21.1 | AI runtime object creation — props | ⬜ | AI generates furniture, props, models placed in existing rooms |
| 21.2 | AI runtime geometry creation — structural | ⬜ | AI generates walls, rooms, corridors (dynamic mesh geometry on top of BSP) |

**🧪 TEST CHECKPOINT 21:** Ask AI to generate a desk → desk appears in room. Ask AI to generate a new corridor → walkable corridor appears.

---

### BATCH 22 — Browser-Ready Port

Full Q3IDE compiled to WebAssembly via Emscripten. Play in the browser.

| # | Feature | Status | Description |
|---|---------|--------|-------------|
| 22.1 | Emscripten/WASM compile of Quake3e + Q3IDE | ⬜ | Full engine running in browser with WebGL |
| 22.2 | Browser capture adapter | ⬜ | Replace ScreenCaptureKit with browser-compatible capture (screen share API or iframe capture) |
| 22.3 | Browser input handling | ⬜ | Pointer lock, keyboard capture, mouse events mapped to Q3IDE interaction model |

**🧪 TEST CHECKPOINT 22:** Open Q3IDE in Chrome. Full FPS gameplay. Windows visible on walls. Interaction model works with browser input.

---

### BATCH VR — Engine Swap

| # | Feature | Status | Description |
|---|---------|--------|-------------|
| VR.1 | Swap engine adapter to VR Quake 3 fork | ⬜ | New engine adapter, spatial/ untouched |

---

### Progress Summary

| Batch | Name | Features | Status |
|-------|------|----------|--------|
| 0 | Foundation | 7 | ✅ Done |
| 1 | **Window Entity, Placement & Rendering** | **10** (stages 1.1-1.10) | 🔧 In Progress |
| 2 | Interaction Model | 9 | ⬜ |
| 3 | Live Window Management | 7 | ⬜ |
| 4 | Window Layout (drag, resize, snap, persist) | 8 | ⬜ |
| 5 | Grapple Hook & Spatial Tools | 7 | ⬜ |
| 6 | Theater, Office & Focus | 5 | ⬜ |
| 7 | Spaces & Navigation | 7 | ⬜ |
| 8 | Programmable Hotkeys & Screenshots | 6 | ⬜ |
| 9 | Ornaments, Vibrancy & Visual Polish | 9 | ⬜ |
| 10 | Project Classification & Live Scanning | 4 | ⬜ |
| 11 | **UML Navigator** | **21** | ⬜ |
| 12 | AI Agent Integration | 12 | ⬜ |
| 13 | Audio & Notifications | 5 | ⬜ |
| 14 | Multiplayer | 6 | ⬜ |
| 15 | **quakeOS Native Rendering** | **8** | ⬜ |
| 16 | Game Modes | 3 | ⬜ Future |
| 17 | Map Skins & Polish | 3 | ⬜ Future |
| 18 | Advanced Audio & Recording | 3 | ⬜ Future |
| 19 | Custom Map | 1 | ⬜ Future |
| 20 | **OpenClaw Bot** | **5** | ⬜ Future |
| 21 | **AI Runtime Geometry** | **2** | ⬜ Moonshot |
| 22 | **Browser WASM Port** | **3** | ⬜ Future |
| VR | Engine Swap | 1 | ⬜ Last |
| **Total** | | **~150** | |

---

## Target File Structure

```
q3ide/
├── README.md
├── LICENSE
├── plan/00-VISION.md                  # Original vision / pitch document
├── plan/04-Q3IDE_SPECIFICATION.md     # This file — full feature spec + tracker
├── CLAUDE.md                          # Claude project prompt
├── plan/03-Q3IDE_ORCHESTRATION.md     # Agent orchestration setup (custom agents, subagents)
│
├── .agents/                            # Cross-IDE agent infrastructure
│   ├── agents/q3agents/               # Custom agent definitions (architecture-scoped)
│   │   ├── engine-adapter.md          # Only touches engine/, quake3e/code/q3ide/
│   │   ├── capture-rust.md            # Only touches capture/
│   │   ├── spatial-c.md               # Only touches spatial/
│   │   ├── daemon-rust.md             # Only touches daemon/
│   │   └── reviewer.md                # Code review — reads everything, writes nothing
│   ├── commands/                      # Slash commands
│   └── PROJECT_LOOP.md                # Build/log/verify manifest
├── Q3IDE_PROMPT.md                    # LLM coding prompt
├── TODO.md
│
├── capture/                           # Rust dylib — ScreenCaptureKit wrapper
│   ├── Cargo.toml
│   ├── Cargo.lock
│   ├── cbindgen.toml
│   ├── build.rs
│   ├── src/
│   │   ├── lib.rs                     # C-ABI exports (q3ide_init, q3ide_get_frame, etc.)
│   │   ├── backend.rs                 # CaptureBackend trait (macOS today, Linux/Windows tomorrow)
│   │   ├── screencapturekit.rs        # macOS SCK implementation
│   │   ├── ringbuf.rs                 # Lock-free frame ring buffer
│   │   └── window.rs                  # Window enumeration, CGWindowID tracking
│   ├── screencapturekit-rs/           # Local fork of SCK Rust bindings
│   └── test_capture.rs
│
├── engine/                            # Engine adapter layer (swappable)
│   ├── adapter.h                      # Abstract engine interface
│   └── quake3e/                       # Quake3e adapter implementation
│       ├── q3ide_adapter.c            # Implements engine_adapter_t for Quake3e
│       ├── q3ide_texture.c            # Texture creation, upload, management
│       ├── q3ide_texture.h
│       ├── q3ide_placement.c          # Wall trace, quad placement, surface alignment
│       └── q3ide_placement.h
│
├── spatial/                           # Engine-agnostic spatial logic
│   ├── window/                        # Window system
│   │   ├── entity.h                   # WindowEntity struct + enums
│   │   ├── manager.h                  # Window lifecycle (create, destroy, track)
│   │   ├── manager.c
│   │   ├── focus.h                    # Focus system (crosshair → dwell → hover)
│   │   ├── focus.c
│   │   ├── pointer.h                  # Pointer Mode + Keyboard Passthrough state machine
│   │   ├── pointer.c
│   │   ├── drag.h                     # Long press drag, snap alignment
│   │   ├── drag.c
│   │   ├── layout.h                   # Layout persistence (save/restore JSON)
│   │   ├── layout.c
│   │   ├── placement.h               # Placement system public API (area transition + leapfrog)
│   │   ├── placement.c
│   │   ├── wall_scanner.h            # Wall pre-scanner (BSP ray casting, qualification)
│   │   ├── wall_scanner.c
│   │   ├── wall_cache.h              # CachedWall_t, WallSlot_t, TrainedPosition_t
│   │   ├── wall_cache.c
│   │   ├── placement_queue.h         # FPS-adaptive placement queue + texture throttle
│   │   ├── placement_queue.c
│   │   ├── visibility.h              # Pre-upload visibility gating (dot product + BSP trace)
│   │   ├── visibility.c
│   │   ├── adaptive_res.h            # 8-tier resolution + SCK reconfiguration + static detection
│   │   ├── adaptive_res.c
│   │   ├── perf_metrics.h            # Per-window performance tracking
│   │   └── perf_metrics.c
│   │
│   ├── space/                         # Space system (8 workflow zones)
│   │   ├── space.h                    # Space definitions, teleport
│   │   ├── space.c
│   │   ├── portal.h                   # Portal rendering + walk-through teleport
│   │   └── portal.c
│   │
│   ├── ui/                            # Spatial UI rendering
│   │   ├── vibrancy.h                 # Vibrancy text contrast logic
│   │   ├── vibrancy.c
│   │   ├── ornament.h                 # Ornament system (attach, position, face-player)
│   │   ├── ornament.c
│   │   ├── billboard.h               # Billboard presentation style
│   │   ├── billboard.c
│   │   ├── widget.h                   # Widget system (persistent HUD mini-displays)
│   │   ├── widget.c
│   │   ├── minimap.h                  # Minimap Ornament
│   │   ├── minimap.c
│   │   ├── menu.h                     # Context menus / popovers
│   │   └── menu.c
│   │
│   ├── mode/                          # Focus modes
│   │   ├── theater.h                  # Theater Mode (blackout + curved wrap)
│   │   ├── theater.c
│   │   ├── office.h                   # Office Mode (spawn desk/monitors)
│   │   └── office.c
│   │
│   ├── nav/                           # Navigation tools
│   │   ├── grapple.h                  # Grapple Hook (movement + Window nav)
│   │   ├── grapple.c
│   │   ├── bookmark.h                 # Saved positions + hotkey binding
│   │   └── bookmark.c
│   │
│   ├── agent/                         # LLM agent integration
│   │   ├── agent.h                    # Agent data model + lifecycle
│   │   ├── agent.c
│   │   ├── diff.h                     # Diff viewer rendering
│   │   ├── diff.c
│   │   ├── dashboard.h                # Agent dashboard Billboard
│   │   └── dashboard.c
│   │
│   ├── project/                       # Project management
│   │   ├── scanner.h                  # ~/Projects/ scanner + file classification
│   │   ├── scanner.c
│   │   ├── browser.h                  # File Browser (F key)
│   │   ├── browser.c
│   │   ├── quickopen.h                # Quick Open (O key) — fuzzy search (fzf-style), vector DB later
│   │   └── quickopen.c
│   │
│   ├── input/                         # Input system
│   │   ├── hotkeys.h                  # Programmable hotkey system
│   │   ├── hotkeys.c
│   │   ├── keyboard_display.h         # Virtual Keyboard overlay (K key)
│   │   ├── keyboard_display.c
│   │   ├── magic_mouse.h              # Magic Mouse gesture handling
│   │   └── magic_mouse.c
│   │
│   ├── audio/                         # Spatial audio system
│   │   ├── spatial.h                  # Per-Window audio positioning
│   │   ├── spatial.c
│   │   ├── ducking.h                  # Audio ducking (focused vs background)
│   │   ├── ducking.c
│   │   ├── notifications.h            # Work notification sounds + announcer
│   │   └── notifications.c
│   │
│   ├── capture_tools/                 # Screenshot & recording
│   │   ├── screenshot.h               # [ key — capture Window or view
│   │   ├── screenshot.c
│   │   ├── recording.h                # ] key — video recording toggle
│   │   └── recording.c
│   │
│   ├── uml/                           # UML Navigator (U key)
│   │   ├── navigator.h                # Full UML view — 3D node cloud
│   │   ├── navigator.c
│   │   ├── mini.h                     # Mini UML in HUD bar
│   │   ├── mini.c
│   │   ├── node.h                     # 3D node box — Transformer unfold animations
│   │   ├── node.c
│   │   ├── pipe.h                     # Pipe rendering — styles, thickness, flow animation
│   │   ├── pipe.c
│   │   ├── cloud.h                    # Node cloud layout — centering, reshuffling
│   │   ├── cloud.c
│   │   ├── cache.h                    # Read .q3ide/uml_cache.json
│   │   └── cache.c
│   │
│   └── multiplayer/                   # Multiplayer extensions
│       ├── visibility.h               # Per-Window Private/Public/Team
│       ├── visibility.c
│       ├── thumbnail.h                # Low-res thumbnail network sync
│       ├── thumbnail.c
│       ├── pair.h                     # Pair programming permission system
│       └── pair.c
│
│   ├── bot/                           # OpenClaw bot integration
│   │   ├── openclaw.h                 # Bot player entity — spawning, lifecycle
│   │   ├── openclaw.c
│   │   ├── api_bridge.h              # HTTP/WebSocket bridge to OpenClaw API
│   │   ├── api_bridge.c
│   │   ├── chat_window.h             # Dedicated chat Window (Floating, scrollable, typeable)
│   │   └── chat_window.c
│   │
│   ├── quakeos/                       # quakeOS — native rendering library
│   │   ├── renderer.h                 # Core text/document renderer (FreeType/stb_truetype)
│   │   ├── renderer.c
│   │   ├── syntax.h                   # Syntax highlighting via tree-sitter (token-level)
│   │   ├── syntax.c
│   │   ├── editor.h                   # Nano-level editor (type, delete, undo, save)
│   │   ├── editor.c
│   │   ├── markdown.h                 # Markdown renderer (headings, bold, code blocks)
│   │   ├── markdown.c
│   │   ├── image_viewer.h             # Native image display (.png/.jpg/.svg as textures)
│   │   ├── image_viewer.c
│   │   ├── focus_mode.h               # `.` key toggle — captured ↔ native render
│   │   └── focus_mode.c
│   │
│   └── ai_geometry/                   # Runtime AI geometry generation (Moonshot)
│       ├── generator.h                # AI object/geometry creation interface
│       ├── generator.c
│       ├── props.h                    # AI-generated props (furniture, models)
│       ├── props.c
│       ├── structural.h              # AI-generated structural geometry (dynamic mesh on top of BSP)
│       └── structural.c
│
├── daemon/                            # UML pre-processor (separate Rust process)
│   ├── Cargo.toml
│   ├── src/
│   │   ├── main.rs                    # Daemon entry — file watcher + parser orchestration
│   │   ├── watcher.rs                 # FSEvents filesystem watcher
│   │   ├── cache.rs                   # Write .q3ide/uml_cache.json
│   │   └── parsers/
│   │       ├── mod.rs                 # Parser plugin interface
│   │       ├── typescript.rs          # TS/JS/TSX/JSX parser
│   │       ├── rust.rs                # Rust parser
│   │       ├── python.rs              # Python parser
│   │       ├── go.rs                  # Go parser
│   │       ├── swift.rs               # Swift parser
│   │       └── c_cpp.rs              # C/C++ parser
│
├── web/                               # Browser WASM port
│   ├── index.html                     # Entry point
│   ├── q3ide_wasm.js                  # Emscripten glue code
│   ├── capture_adapter.js             # Browser capture adapter (Screen Share API / iframe)
│   └── input_adapter.js               # Browser input (pointer lock, keyboard capture)
│
├── q3ide_main.c                       # Init, per-frame update, shutdown
├── q3ide_capture.h                    # Generated C header (cbindgen)
├── q3ide_design.h                     # VisionOS design tokens
│
├── quake3e/                           # Forked Quake3e engine
│   ├── code/
│   │   ├── q3ide/                     # Q3IDE hooks inside engine
│   │   │   ├── q3ide_hooks.c
│   │   │   ├── q3ide_hooks.h
│   │   │   └── q3ide_params.h
│   │   ├── client/
│   │   ├── renderer/
│   │   ├── renderer2/
│   │   ├── renderervk/
│   │   └── ...
│   ├── Makefile
│   └── build/
│
├── baseq3/                            # Game data
│   ├── pak0-8.pk3
│   ├── autoexec.cfg
│   ├── q3ide_autocmd.cfg
│   ├── shaders/                       # Custom shaders (future)
│   │   ├── portal_frame.shader
│   │   └── hover_glow.shader
│   ├── sounds/                        # Work notification sounds (future)
│   │   ├── build_complete.wav
│   │   ├── tests_failing.wav
│   │   └── agent_idle.wav
│   ├── models/                        # Office Mode models (future)
│   │   ├── office/
│   │   │   ├── desk.md3
│   │   │   ├── chair.md3
│   │   │   └── monitor.md3
│   │   └── ...
│   └── ...
│
├── config/                            # Default configuration templates
│   ├── default_hotkeys.toml           # Default keybindings (copied to .q3ide/ on first run)
│   └── default_layout.json            # Default Window layout (copied to .q3ide/ on first run)
│
├── docs/                              # Technical documentation
│   ├── apple-screencapturekit/
│   ├── graphics/
│   ├── q3-engine-internals/
│   ├── quake3e/
│   ├── rust-ffi/
│   └── screencapturekit-rs/
│
├── scripts/
│   ├── build.sh                       # Full build (capture dylib + engine)
│   └── check_logs.sh
│
└── logs/
    └── engine.log
```

### Key Structural Decisions

**`spatial/` is the brain.** All engine-agnostic logic lives here, organized by domain. When the engine swaps to VR, nothing in `spatial/` changes.

**`spatial/window/`** is the core — entity model, focus, pointer, drag, layout, AND the full placement system (wall scanner, cache, placement queue, visibility gating, adaptive resolution, perf metrics). Everything touches Windows.

**`spatial/space/`** owns the 8 Spaces and Portal system. Portals are first-class citizens with their own module.

**`spatial/ui/`** renders Ornaments, Widgets, Billboards, minimap, menus. Pure presentation.

**`spatial/mode/`** handles Theater Mode and Office Mode — full-screen state changes.

**`spatial/agent/`** is the LLM integration point — diff viewer, approve/reject, dashboard.

**`spatial/input/`** is the programmable hotkey system, virtual keyboard, Magic Mouse.

**`capture/`** stays pure Rust. Only talks to `spatial/` through C-ABI. Never touches engine.

**`daemon/`** is a separate Rust process — the UML pre-processor. Watches filesystem, parses code with language-specific plugins, outputs `.q3ide/uml_cache.json`. Communicates with Q3IDE through the cache file + optional IPC notifications. Never linked into the engine.

**`engine/`** adapter stays thin. Only implements the abstract interface for Quake3e. Swappable.

**`spatial/bot/`** is the OpenClaw integration — bot player entity, API bridge, dedicated chat Window. Talks to OpenClaw via HTTP/WebSocket. The bot is a real Quake bot player that also responds to chat.

**`spatial/quakeos/`** is the native rendering library — FreeType text, tree-sitter syntax highlighting, markdown, images, nano editor, focus mode toggle. Renders content directly in-engine without macOS capture. **Important architectural note:** quakeOS needs to draw into the engine's rendering pipeline (create textures, draw quads, handle font atlases). This cannot be 100% engine-agnostic. Solution: a **third adapter layer** between quakeOS and the engine — `engine/adapter.h` gets extended with a 2D rendering API (create texture, draw textured quad, draw text glyph). quakeOS calls these abstract functions, never engine internals directly. When the engine swaps (Quake → VR → Unreal), only the adapter implementation changes. Minimize pain, can't eliminate it entirely.

**`spatial/ai_geometry/`** is the moonshot — AI-generated geometry at runtime (props + structural dynamic mesh on top of BSP). This is future work and may require deep engine integration.

**`web/`** is the Emscripten/WASM browser port. Replaces platform-specific capture (ScreenCaptureKit) with browser APIs. Replaces native input with pointer lock + keyboard capture. Everything else in `spatial/` stays identical.

**`config/`** holds default templates copied to project-level `.q3ide/` on first run. Not buried in engine dirs.

---

## Overview

Q3IDE is a spatial operating environment for managing multiple projects and multiple LLM agents inside a Quake III Arena map. Desktop applications (terminals, editors, browsers) are captured and presented as Windows on in-game surfaces. The game map becomes a workspace where developers navigate between projects, monitor AI agents, review code, and collaborate — all while playing.

The core insight: AI agents are writing code now. Developers wait. Q3IDE turns that waiting into a multiplayer FPS where work happens on the walls and life happens in the room.

### Core Design Principles

1. **Don't fuck up the game.** Quake stays Quake. Bots run. Rockets fly. Physics work. The IDE is additive — it never degrades the game experience. If GPU budget is tight, Windows degrade first. Game FPS is sacred.
2. **Multiple projects, multiple agents, no limits.** Any number of projects can be open simultaneously. Each project can have multiple LLM agents. Machine resources are the only constraint. You never leave the game to open a new project.
3. **Q3IDE is an orchestrator.** It spawns agents, manages API calls, auto-opens terminals, captures output, presents diffs, and processes approvals. It's not a viewer — it's mission control.
4. **Distance = quality.** The closer you are to a Window, the more resources it gets. Far away = cheap. Close up = full quality. The GPU budget self-manages through spatial proximity.
5. **The game world is the operating system.** Windows are 3D geometry. Rockets fly in front of terminals. Portals are real doorways. Spaces are physical rooms. Everything is spatial, nothing is overlay.
6. **Bots are always running.** Solo mode has bots. You're never alone. There's always someone to frag between tasks.

### MVP Test Map

**q3dm17 (The Longest Yard)** — iconic, open, lots of wall space. Perfect for testing Anchored Windows and Floating Windows across the space platform geometry.

### Renderer Support

Both OpenGL (renderer1) and Vulkan (renderervk) are supported. Player picks their renderer, Q3IDE adapts its texture upload strategy accordingly.

---

## Project Discovery

Q3IDE scans `~/Projects/` at launch. Each subdirectory is treated as a **Project**.

- **Cold scan** — runs before the Space loads. Enumerates all Projects, classifies file structure, builds the initial spatial map.
- **Live scan** — watches active Projects via filesystem events. When files change, move, or are created, the spatial map updates and Windows reflect the new state in real-time.
- Classification uses a heuristic rules engine (glob patterns, path conventions, file extensions). LLM-assisted classification is a future enhancement.

---

## Three Monitor Setup

Q3IDE spans three physical monitors. The game world wraps 180° around you.

- **Left monitor** — peripheral vision. Failing tests light a room red in the corner of your eye. A teammate rocket-jumps past. You catch movement without turning your head.
- **Center monitor** — where you aim, where you frag, where you read code on the wall in front of you.
- **Right monitor** — your AI agent's terminal output, your build dashboard, the diff viewer. Always visible in your peripheral.

No alt-tab. No window management. You're IN the codebase. The codebase is around you. Your code is on the left wall, your game is in front of you, your AI's work is on the right wall.

The three-monitor Q3 experience already existed — competitive players ran it for years. We just give every extra pixel of wall space a purpose.

**Status:** Currently being implemented.

---

## Window Presentation Styles

Q3IDE defines seven presentation styles:

### 1. Anchored Window

A Window mounted flush against a BSP surface (wall, floor, ceiling). Subtle bevel or frame. Emits dynamic light from its content. Used for persistent displays in a room — the files that *live* here.

### 2. Floating Window

A single Window hovering in the Space, not attached to any surface. Opaque background, rounded corners, Window Bar at the bottom for repositioning. Hover Effect on crosshair aim (glow + z-lift).

### 3. Window Group

Multiple related Windows arranged in a predefined spatial layout — arc, grid, stack, or side-by-side. Moves and scales as a unit. Used when you need to see several related files or outputs simultaneously.

### 4. Billboard

An oversized Anchored Window scaled to be visible from a distance. Full-brightness, high-contrast rendering for maximum legibility across the Space. Used for dashboards, status displays, and shared information everyone in the multiplayer session needs to see.

### 5. Ornament

A compact, always-visible Window pinned to the edge of the player's viewport. HUD-layer element that persists across rooms and movement. Used for quick-access controls, status indicators, and persistent tools.

### 6. Portal

A Window that shows a live view into another Space — and doubles as a teleporter. By default, a visual peek: you see the destination rendered inside the Portal frame. Walk into the Portal and you physically teleport to that Space.

Portals connect Spaces, coworkers, and even specific files. See the Portals section for full detail.

### 7. Widget

A persistent miniature display always available on screen, smaller than an Ornament. Shows live data at a glance — agent status, clock, build progress, test count. No Focus, no Pointer Mode. View only.

### Presentation Style Summary

| Style | Placement | Background | Interactive | Use Case |
|-------|-----------|-----------|-------------|----------|
| Anchored Window | On BSP surface | Bezel frame | Yes (Pointer Mode) | Room-resident files, persistent displays |
| Floating Window | In Space, unattached | Opaque | Yes (Pointer Mode) | Active work, desktop capture, close interaction |
| Window Group | In Space, arranged | Opaque | Yes (Pointer Mode) | Multi-file views, diff comparisons, Theater Mode |
| Billboard | On BSP surface, oversized | Full brightness | Yes (at close range) | Dashboards, shared status, LLM monitors |
| Ornament | Viewport-pinned HUD | Subtle | Yes (click) | Quick-access controls, persistent tools |
| Portal | On BSP surface or floating | Glowing edge frame | Walk-through to teleport | Space nav, cross-Space peek, multiplayer visit |
| Widget | Viewport-pinned HUD, small | Minimal | No (view only) | Live data glance — clock, agent status, build |

---

## Portals — Core Navigation Concept

Portals are a **major core idea**. They serve three purposes simultaneously:

### 1. Spatial Peek

A Portal shows a live, real-time view into another Space. Standing in the TEST Space, you see a Portal on the wall showing the BUILD Space — complete with its Billboard, Anchored Windows, and any players currently there. You don't need to teleport to check build status. You see it through the Portal.

### 2. Teleportation

Walk into a Portal and you teleport to that Space. The transition is seamless — the Portal frame expands around you as you step through. More immersive than `Shift+number` keys. You can SEE where you're going before you go.

### 3. Multiplayer Connection

Portals connect to coworkers' Spaces. Open a Portal to another player's BUILD Space and you see their screens, their work, their presence. Walk through and you're standing in their workspace. Teleport through Portal Windows to visit coworkers.

### Portal Types

**Space Portal** — connects two Spaces within the same map. BUILD ↔ TEST, PLAN ↔ BUILD, etc.

**Player Portal** — connects to a coworker's Space in multiplayer. See their screens, their presence.

**File Portal** — mini-Portal on a Window showing the destination of an import/dependency. Hover over `import { auth } from './auth'` and a Portal opens showing the auth module's Window in its Space.

### Portal Rendering

**⚠️ Performance critical.** Each visible Portal is a full second render pass. Multiple visible Portals multiply frame cost. This is the biggest performance risk in Q3IDE. Search Quake 3 community repos for existing Portal implementations — don't build from scratch.

- Distinctive glowing edge shimmer — unmistakable, inviting
- Destination Space rendered in real-time inside the frame (reduced resolution for performance)
- Depth fog inside the Portal gives a sense of passage
- Walking in triggers a brief transition — frame expands, destination fades in
- **Performance mitigations:** render at reduced resolution, limit update rate by distance, cap simultaneous active Portals

---

## Grapple Hook — `X` Key

The Grapple Hook serves two purposes:

### 1. Physical Movement

`X` fires the grapple. It hooks to any surface and pulls you there. Fast, fun, Quake-native movement for getting around quickly within a Space. Swing between rooms, launch yourself across corridors, reach high wall-mounted screens.

### 2. Window Navigation

Shoot the grapple at a Window — any Window, on any wall — and it pulls you to that Window, positioning you at perfect reading distance, centered on the screen. This is the fastest way to get to a specific Window. See a terminal across the room? Grapple to it. See a diff on a high wall? Grapple up to it.

The grapple is always available. `X` always fires it. It replaces the standard Quake grapple hook weapon.

---

## Window Data Model

Every Window carries rich metadata for LLM agent integration, multiplayer sharing, and spatial management.

**Note:** This struct is pseudocode. Implement in C with equivalent types (`char*` for String, `float[3]` for Vec3, nullable fields as pointer-or-NULL, etc.). The Rust capture layer has its own types that map to these via C-ABI.

```
WindowEntity {
    // Identity
    id:                 UUID
    title:              String
    type:               PresentationStyle   // Anchored | Floating | Group | Billboard | Portal | Ornament | Widget

    // Source
    capture_window_id:  CGWindowID          // unique per window, not per app
    capture_app_name:   String              // "iTerm2", "VSCode", "Chrome"
    capture_app_bundle: String              // "com.googlecode.iterm2"

    // Project
    project_id:         UUID
    project_path:       Path                // ~/Projects/my-app/

    // Spatial
    space:              Space               // ASK | RESEARCH | PLAN | BUILD | TEST | RUN | MAINTAIN | GARAGE
    position:           Vec3
    rotation:           Quat
    scale:              Vec2
    surface_anchor:     Option<SurfaceID>   // BSP surface if Anchored
    locked:             bool                // pinned in place

    // State
    status:             WindowStatus        // Active | Idle | Error | Building | Passing | Failing
    focused:            bool
    pointer_mode:       bool
    keyboard_captured:  bool
    visible:            bool
    dirty:              bool                // new frame available

    // LLM Agent
    agent_id:           Option<UUID>
    agent_model:        Option<String>      // "claude-sonnet-4-20250514", "gpt-4", "glm-4"
    agent_status:       Option<AgentStatus> // Idle | Thinking | Writing | Waiting | Error
    agent_context_tokens: Option<u32>

    // Multiplayer
    owner_player_id:    PlayerID
    visibility:         Visibility          // Private | Public | Team
    thumbnail:          Option<CompressedFrame>

    // Persistence
    layout_group:       Option<String>
    persistent:         bool
    last_active:        Timestamp

    // File association
    file_path:          Option<Path>
    file_classification: Option<String>
}
```

---

## Window Placement & Movement

Full spec in `PLACEMENT.md`. Summary here.

### Two Placement Modes

**Mode 1 — Area Transition:** Player enters new area → pre-scan ALL walls (cache) → queue ALL windows → drain 1/frame FPS-gated (>30fps) → spread evenly across walls → closest walls first. Trained windows get their saved spots first. Texture throttle: all windows drop to 2fps during placement, restore to 25fps when done.

Visual: 10 dogs follow you through a doorway. Each finds a spot and sits down one at a time. TVs freeze while dogs are moving, unfreeze when settled.

**Mode 2 — Within-Area Leapfrog:** Player moves >7m → furthest window jumps to closest free slot. Checked every 30 frames. One window per check. Trained windows never leapfrog. Small rooms: windows never move.

### Window Constraints

- Minimum 100u diagonal (100" TV). Aspect ratio sacred. Never stretched.
- Single horizontal row per wall. Vertically centered. Like a projector.
- Never stacked, never on ceilings/floors, never intersecting.
- 85% of wall height (aspect-fit). 3u wall offset.
- Only vertical walls (±5°). Exception: in-map monitors/billboards ±30°.
- Double-sided: render from behind with flipped image.
- Collision: trace against EVERYTHING (BSP, entities, models, players).

### Trained Positions (Dog Memory)

User manually moves window → position saved per area. Leave area, come back → window returns to trained spot. Trained windows get priority over auto-placed. Teaching dogs where their place is.

### Wall Scanner

Pre-scan on area entry within 60m radius. Cache walls with pre-computed slots. Walls qualify: height ≥ 79u, width ≥ 114u, vertical ±5°. Results persist until next area transition.

### Performance During Placement

- FPS-gated: only place if last frame >30fps
- Texture throttle: 2fps during queue drain (vs 25fps normal)
- Window moves = just update x,y,z,angle. Never destroy/recreate entity.
- Queue cap: 32. Excess waits for leapfrog.

### Constants (in q3ide_params.h)

See `PLACEMENT.md` for full list. Key values: placement_radius=2400u (60m), leapfrog_distance=280u (7m), fps_gate=30, texture_fps_normal=25, texture_fps_placing=2, max_windows=64, max_cached_walls=128, max_wall_slots=8.

---

## Spaces: The Seven Stages + Garage

| # | Space | Purpose | LLM Agent Role | Key |
|---|-------|---------|---------------|-----|
| 1 | ASK | Query, explore, ideate | Conversationalist | `Shift+1` |
| 2 | RESEARCH | Investigate, read, compile | Researcher | `Shift+2` |
| 3 | PLAN | Architect, specify, design | Architect | `Shift+3` |
| 4 | BUILD | Write code, generate, iterate | Coder | `Shift+4` |
| 5 | TEST | Validate, verify, measure | Tester | `Shift+5` |
| 6 | RUN | Execute, deploy, monitor | Operator | `Shift+6` |
| 7 | MAINTAIN | Update, audit, refactor | Maintainer | `Shift+7` |
| 8 | GARAGE | Git, config, cleanup, setup | Handyman | `Shift+8` |

**ASK** — Open, bright, inviting. The lobby.
**RESEARCH** — Library or war room. Dense with information.
**PLAN** — Clean, structured. Whiteboard energy.
**BUILD** — Industrial, workshop. The engine room.
**TEST** — Clinical, precise. Room lighting reflects test health.
**RUN** — Mission control. Active, operational.
**MAINTAIN** — Workshop, maintenance bay. Methodical.
**GARAGE** — Utilitarian. Tools on walls.

Navigation between Spaces: `Shift+1`–`Shift+8` for instant teleport, Portals for visual peek + walk-through, physical movement through the map.

---

## AI Agent Integration

Q3IDE is a **full orchestrator**, not just a display layer. It spawns agents, assigns them to projects, gives them tasks, reviews their output — all from inside the game. Multiple projects run simultaneously, each with their own agents.

**Note:** This section covers **coding agents** (Claude Code, Aider, custom scripts) — tools that write, refactor, and test code. The **OpenClaw bot** (Batch 20) is separate: a conversational AI that runs around the map fragging and chatting. Different tools for different jobs. Coding agents work in terminals. OpenClaw works in the game world.

### Agent Spawning

Q3IDE auto-spawns iTerm/Terminal windows for agents. When you assign an agent to a task, Q3IDE:

1. Opens a new Terminal window (or reuses an idle one)
2. Launches the appropriate tool (Claude Code, Aider, custom script, or native API call)
3. Captures the Terminal window via ScreenCaptureKit
4. Places the Window in the appropriate Space
5. Tracks the agent's status in the WindowEntity

**Two execution modes:**
- **Native API** — Q3IDE calls LLM APIs directly (Anthropic, OpenAI, Zhipu, etc.) for simple tasks: ask a question, generate a snippet, review code. Faster, no terminal needed.
- **Terminal spawn** — Q3IDE spawns a terminal process for complex agents (Claude Code, Aider, Cursor CLI). Full terminal capture, rich interactive output.

### Giving Agents Tasks

Three ways, depending on context:

1. **Chat Window** — a dedicated Floating Window with text input, like ChatGPT. Type your task, hit enter. For native API agents.
2. **Console command** — `/q3ide_ask "refactor the auth module"`. Quick one-liners from the Q3 console.
3. **Captured terminal** — enter Pointer Mode on the agent's Terminal Window, activate Keyboard Passthrough, type directly into Claude Code / Aider. Full interactive session.

### API Key Configuration

- **Primary:** `.q3ide/.env` in the project directory. Never committed to git.
- **In-game:** Control Center panel shows configured models and key status (valid/invalid/expired). Can add/edit keys from in-game.
- **Security:** Keys stored encrypted at rest. Never displayed in plain text on any Window. Never logged. Shared secrets manager for team use is a future feature.
- **Per-model config:** Each LLM model (Claude, GPT-4, GLM, etc.) configured separately with API key, endpoint, and default parameters.

### Approve/Reject from In-Game

When an LLM agent completes a task, the relevant Window pulses. Walk to it (or grapple to it). The diff is displayed with syntax highlighting on the Window surface. An Ornament below the Window shows:

`[✓ Approve]  [✗ Reject]  [💬 Comment]  [↻ Retry]`

Aim crosshair at the button, click. The agent receives your decision and continues. You never leave the game.

**Agent communication protocol:** depends on agent type:
- **Native API agents** (direct LLM calls): Q3IDE communicates via HTTP/REST to the LLM provider API. Approve/Reject triggers the next API call in the conversation.
- **Terminal agents** (Claude Code, Aider): Q3IDE communicates via stdin/stdout of the spawned terminal process. Approve sends a keystroke or command to the terminal. Reject sends a cancel/interrupt signal.

### Diff Viewer

A dedicated Window (or Window Group) showing AI-generated code changes. Syntax highlighted, scrollable in Pointer Mode. Red/green diff formatting. File path breadcrumb in a top Ornament. This is where you review every change before it hits your codebase.

### Task Queue & Agent Dashboard

A Billboard in a central location showing:

- **All active agents** — model name, current task, Space assignment
- **Task queue** — pending, in-progress, completed
- **Context usage** — token count per agent, approaching limits
- **Cost tracker** — API spend per agent (if using pay-per-token)
- **Agent status lights** — green (working), yellow (thinking), red (error), grey (idle)

### War Room Layout

For managing multiple LLM agents simultaneously, the ASK or PLAN Space can adopt a **War Room** aesthetic: large central Billboard, surrounding screens showing each agent's output, status board with all tasks. Military situation room energy. Multiple agents, one commander (you).

---

## Minimap

**Absolute must.** A persistent Ornament (or toggleable overlay) showing:

- **Floorplan view** of the current map — all Spaces visible as zones
- **Player dot** — your position in real-time
- **Teammate dots** — where every player is (multiplayer)
- **Space labels** — ASK, BUILD, TEST, etc.
- **Activity indicators** — which Spaces have active agents, which have failing tests
- **Portal connections** — lines showing Portal links between Spaces

The minimap is always available as an Ornament. Toggle with `M` key to enlarge it as a Floating Window for full detail.

---

## UML Navigator — `U` Key

### The Core Idea

**The developer doesn't read code anymore. The developer reads architecture.** Files are implementation details that LLMs handle. The UML is the primary interface for understanding and commanding your codebase. You look at the system through diagrams, not through source.

Pressing `U` opens a full interactive 3D UML diagram centered on the current file/class. Click any node to recenter the cloud. Press a button on a node or `N` to spawn an LLM agent on that class. The UML IS the IDE.

### Two Modes

**Mini UML (always visible)**

A compact node graph embedded in the Q3 HUD bar at the bottom of the screen — between health and ammo. No text, just node titles and connection lines. **Updates live** as you move between files and Windows — when you Focus a different Window, the Mini UML recenters on that file's code neighborhood. Always there, always current. You see the architecture while you frag.

Mini UML and Minimap (`M`) are **separate** HUD elements. They coexist — Minimap shows physical space, Mini UML shows code space. Two different maps of two different worlds.

**Full UML (`U` key)**

A full 3D interactive diagram floating in front of the player. The current file is centered. Related files orbit around it as 3D boxes connected by animated pipes. **Floats with the player** — moves with you as you walk, always in front. Aim at any node to interact. Stays open until you press `U` again — persists through teleportation, movement, combat, and death.

### 3D Node Boxes — Transformer Unfolding

Each class/file is a 3D box. The box has layers that **unfold like a Transformer** with animated reveal:

```
COMPACT (default)
┌──────────────────┐
│   AuthService     │
└──────────────────┘

    ↓ aim at it, dwells 150ms

UNFOLDED (methods revealed)
┌──────────────────┐
│   AuthService     │
├──────────────────┤
│ + login()        │
│ + logout()       │
│ + verifyToken()  │
│ + refreshToken() │
└──────────────────┘

    ↓ aim at a method, dwells again

FULLY UNFOLDED (parameters + types revealed)
┌──────────────────────────────┐
│   AuthService                 │
├──────────────────────────────┤
│ + login(email: str,          │
│         password: str)       │
│   → Result<Token, AuthError> │
├──────────────────────────────┤
│ + verifyToken(token: str)    │
│   → Result<Claims, JwtError> │
└──────────────────────────────┘
```

Each unfold is animated — panels slide out, rotate into place, snap into alignment. Folding back up when you look away. Smooth, satisfying, informative.

### Node Cloud Navigation

The UML is a **node cloud** centered on the current file. Related files float around it, positioned by relationship strength:

- **Closest nodes** = strongest connections (most imports, most shared calls)
- **Farther nodes** = weaker connections (one import, few calls)
- **Node count** = adaptive. Close nodes are detailed (full box with title). Far nodes are dots with labels. The cloud stays readable regardless of project size.

**Recentering:** Click any node and the entire cloud animates — the clicked node moves to center, all other nodes reshuffle around it based on ITS relationships. Like a living mind map. You're always at the center, browsing outward.

**Window Spawn:** Click a node and it opens the file as a new **Floating Window at your current position**. If the file already has a Window somewhere in the world, clicking still spawns a fresh one in front of you — you don't need to know where the original is. The UML is a Window spawner.

**Portal Teleport:** Click the teleport button on a node (or double-click the node) and a **Portal vortex** opens on the node — it sucks the player in and spits them out perfectly positioned in front of that file's Anchored Window on the wall. Dramatic, satisfying, functional.

### UML Behavior

**Follows the player:** The full UML view floats in front of you and moves with you as you walk. It's a holographic display strapped to your field of view, not anchored in world space. You never walk away from your diagram.

**Collision-aware:** The UML node cloud is a living creature. It **bounces off walls, ceilings, and BSP geometry** — nodes that would clip through surfaces push away and rearrange. Walk into a narrow corridor and the cloud compresses. Enter a large room and it expands. The cloud is always subtly moving, rearranging, breathing — like a floating tag cloud organism that respects the physical space around it.

**Pure code architecture:** The UML shows only code structure — classes, methods, relationships. No Q3IDE Space classification visuals (BUILD/TEST/etc.) are mixed in. The UML is about the code, not the workspace.

**Current project only:** The UML shows only the active project. Cross-project dependencies (npm packages, shared libs) are not displayed. One project, one diagram.

**Persists until dismissed:** The UML stays open through teleportation, movement, combat, and death. Press `U` again to close it. It never auto-dismisses.

### Node Status Overlays

**Test status on nodes:** UML nodes turn **red** or **green** matching the test results for that file. If `auth.test.ts` is failing, the `AuthService` node glows red. If passing, green. You see your codebase health at a glance on the architecture diagram.

**Agent activity on nodes:** When an LLM agent is working on a class, the UML shows it in two ways:
- **Status dot** on the node itself — green (working), yellow (thinking), red (error), grey (idle)
- **Glowing pipe** between the agent's origin and the class it's working on — the pipe pulses with activity, brighter = more active

This means the UML doubles as a **real-time agent command view**. You see which nodes have agents working on them, what those agents are doing, and how far along they are. War Room through the architecture diagram.

### Pipe Interaction

Pipes (connections between nodes) are interactive. Aim your crosshair at a pipe and:

- Both connected nodes **highlight** simultaneously — you see the full relationship at a glance
- A **tooltip** appears showing relationship details: type (dependency/inheritance/delegation/etc.), weight (import count + call count), and direction
- The pipe itself brightens and its flow animation speeds up

This makes pipes first-class navigation surfaces, not just decoration.

### Git Integration

The UML integrates with git history to show temporal context:

**Diff overlay:** Nodes that changed since the last commit glow/pulse with a subtle "fresh" animation. You see what's been recently touched.

**Blame coloring:** Nodes can be color-coded by last author. Each team member gets a color. You see who's been working where.

**Time slider:** A scrubber control (on the UML Ornament) lets you scrub through git history and watch the UML **evolve over time**. Nodes appear as files are created, pipes form as dependencies are added, nodes grow as classes get methods. Like a timelapse of a city being built. Scrub backward to see how the architecture was 6 months ago. Scrub forward to see how it grew.

The time slider is a future feature but architecturally planned for — the pre-processor daemon stores historical snapshots alongside the current UML cache.

### Multiplayer UML Visibility

**UML is visible to all players.** When you open the UML, other players see your diagram floating in front of you. They can see what you're browsing, which node you're focused on, and what you're exploring.

This enables:
- **Architectural discussions** — "look at this coupling between auth and db" while pointing at the pipe
- **Spontaneous collaboration** — a teammate sees you browsing a complex module and walks over to help
- **Teaching/onboarding** — a senior dev walks a new hire through the architecture visually, in real-time, in the game world

### Pipes — Animated Relationship Flow

Connections between nodes are **3D pipes** with animated flow particles moving through them:

**Pipe thickness** = combined weight of the relationship (imports + function calls + shared types). A thick pipe from `routes.ts` to `db.ts` means heavy coupling. A thin pipe means loose coupling. You see the architecture's health at a glance.

**Pipe visual styles** per relationship type — all visible simultaneously, distinguishable by appearance:

| Relationship | Pipe Style | Color |
|-------------|-----------|-------|
| **Import / Dependency** | Solid pipe, particle flow | Blue |
| **Inheritance / Extends** | Double-walled pipe | Purple |
| **Composition / Contains** | Thick solid pipe, fast flow | Green |
| **Aggregation** | Pipe with diamond markers | Teal |
| **Delegation** | Dashed pipe, intermittent flow | Orange |
| **Callback / Event** | Wavy/pulsing pipe | Yellow |
| **Notification / Observer** | Dotted pipe, broadcast pattern (one→many) | Red |
| **Protocol / Interface** | Transparent pipe, outline only | White |

**Flow direction** is visible — particles move FROM the caller TO the callee. You can see which direction the dependency flows.

### Diagram Types

Three diagram types available, switchable within the full UML view:

**Class Diagram (default)**
- Classes as 3D boxes (unfolding)
- Pipes showing all relationship types
- Node cloud layout centered on current class

**Component Diagram**
- Higher-level view: modules, services, APIs as larger boxes
- Groups of classes collapsed into single component nodes
- Pipes show inter-module dependencies
- Good for "big picture" architectural overview

**Dependency Graph**
- Pure import/dependency visualization
- All pipes are the same style (just import arrows)
- Thickness = import weight
- Simplest, cleanest view for "who depends on who"

Toggle between diagram types with Ornament buttons on the UML view or cycle with `Shift+U`.

### LLM Agent Integration from UML

Two ways to send a class to an LLM agent:

**Small button on each node** — a subtle button appears on the unfolded box. Click it → opens agent context menu:
- "Ask about this class"
- "Refactor this class"
- "Write tests for this class"
- "Document this class"
- "Explain this class"

**`N` key (New agent)** — while aiming at any UML node, press `N`. Q3IDE instantly:
1. Spawns a new iTerm window
2. Launches Claude Code (or configured agent) with the file path pre-loaded
3. Captures the terminal Window
4. Places it in the BUILD Space
5. The agent has full context of the file

One key from "I see a problem in the architecture" to "an AI is working on it."

### Pre-Processor Daemon

A background daemon watches the filesystem and maintains the UML data:

**Architecture:**
- Runs as a separate process alongside Q3IDE
- Watches all active Project directories via FSEvents (macOS)
- On file change: re-parses only the changed file + files that reference it
- Outputs updated UML graph data to `.q3ide/uml_cache.json`
- Q3IDE reads the cache on `U` press and on daemon update notifications

**Parsers:**
- Language-specific parsers as **plugins/modules** within the daemon
- Each plugin handles one language family:
  - TypeScript/JavaScript (`.ts`, `.js`, `.tsx`, `.jsx`)
  - Rust (`.rs`)
  - Python (`.py`)
  - Go (`.go`)
  - Swift (`.swift`)
  - C/C++ (`.c`, `.h`, `.cpp`)
- Plugin interface: input = file path → output = list of classes, methods, imports, relationships
- New language support = new plugin module, no daemon changes

**Parser output per file:**
```json
{
  "file": "src/auth/service.ts",
  "classes": [
    {
      "name": "AuthService",
      "methods": [
        {"name": "login", "params": [{"name": "email", "type": "string"}, {"name": "password", "type": "string"}], "return": "Result<Token>", "visibility": "public"},
        {"name": "verifyToken", "params": [{"name": "token", "type": "string"}], "return": "Result<Claims>", "visibility": "public"}
      ],
      "properties": [
        {"name": "jwtSecret", "type": "string", "visibility": "private"}
      ]
    }
  ],
  "imports": ["../db/queries", "../config", "jsonwebtoken"],
  "exports": ["AuthService"],
  "relationships": [
    {"type": "dependency", "target": "../db/queries", "weight": 5},
    {"type": "dependency", "target": "jsonwebtoken", "weight": 3},
    {"type": "delegation", "target": "../cache/redis", "weight": 1}
  ]
}
```

### UML in the File Structure

```
spatial/
├── uml/
│   ├── navigator.h          # Full UML view (U key) — 3D node cloud
│   ├── navigator.c
│   ├── mini.h               # Mini UML in HUD bar
│   ├── mini.c
│   ├── node.h               # 3D node box — unfold animations
│   ├── node.c
│   ├── pipe.h               # Pipe rendering — styles, thickness, flow animation
│   ├── pipe.c
│   ├── cloud.h              # Node cloud layout — centering, reshuffling, adaptive count
│   ├── cloud.c
│   ├── cache.h              # Read .q3ide/uml_cache.json
│   └── cache.c

daemon/                       # Separate process — UML pre-processor
├── Cargo.toml                # Rust daemon
├── src/
│   ├── main.rs               # Daemon entry — file watcher + parser orchestration
│   ├── watcher.rs            # FSEvents filesystem watcher
│   ├── cache.rs              # Write .q3ide/uml_cache.json
│   └── parsers/
│       ├── mod.rs            # Parser plugin interface
│       ├── typescript.rs     # TS/JS/TSX/JSX parser
│       ├── rust.rs           # Rust parser
│       ├── python.rs         # Python parser
│       ├── go.rs             # Go parser
│       ├── swift.rs          # Swift parser
│       └── c_cpp.rs          # C/C++ parser
```

---

## Programmable Hotkeys

Players can program custom hotkeys while playing. This is essential — you should be able to bind any action to any key on the fly.

### Virtual Keyboard — `K` key

Pressing `K` opens a **Virtual Keyboard** as a Floating Window. It shows:

- A visual representation of your full keyboard
- Every key labeled with its current binding
- Color-coded: game controls (red), Q3IDE shortcuts (blue), custom hotkeys (green), unbound (grey)
- Click any key to reassign it

### Programmable Bindings

Any of these actions can be bound to any key:

- Teleport to a specific Space
- Open a specific Window
- Run a specific command
- Toggle a specific agent
- Jump to a bookmarked location
- Open a Portal to a specific destination
- Run the project
- Take a screenshot
- Any `/q3ide_*` console command

### Bookmarks as Hotkeys

Save any position in the map as a bookmark. Bind it to a key. Now `Ctrl+5` teleports you to "my debugging spot in the auth room." Personal spatial shortcuts that build up over time as you learn the map.

---

## Game Modes While Coding

### Solo Mode

Playing alone with AI agents. The core loop:

1. Give your AI agent a task
2. Play solo Quake (bots, exploration, practice)
3. See progress on walls as you play
4. Announcer: "TASK COMPLETE"
5. Grapple to the relevant Window, review the diff
6. Approve, give next task
7. Back to fragging bots

**Pause → Code → Continue.** At any point, enter Keyboard Passthrough on a terminal and type. When done, Escape back to the game. The bots wait (or don't — configurable).

### Multiplayer: Synchronized Rounds

The killer multiplayer game mode. Work and play in structured rounds:

```
ROUND STRUCTURE:

  ┌─── CODE ROUND (5 min) ──────────────────────┐
  │ All players at their Spaces                  │
  │ LLM agents receiving tasks, writing code     │
  │ Review diffs, approve changes, plan next     │
  │ No combat — weapons disabled                 │
  │ Timer visible on Dashboard Billboard         │
  └──────────────────────────────────────────────┘
                      │
                      ▼
  ┌─── FRAG ROUND (3 min) ──────────────────────┐
  │ Deathmatch / Team Deathmatch / CTF           │
  │ LLM agents continue working in background    │
  │ Progress visible on walls during combat       │
  │ Scoreboard shows frags AND code metrics       │
  └──────────────────────────────────────────────┘
                      │
                      ▼
  ┌─── TEST ROUND (2 min) ──────────────────────┐
  │ Tests run on all committed code              │
  │ TEST Space lights up — green or red          │
  │ Team with better test results gets bonus     │
  │ Quick review of agent output                 │
  └──────────────────────────────────────────────┘
                      │
                      ▼
  ┌─── FRAG ROUND (3 min) ──────────────────────┐
  │ Winner of test round gets power weapons      │
  │                                              │
  └──────────────────────────────────────────────┘
                      │
                      ▼
  ┌─── RUN ROUND (2 min) ──────────────────────┐
  │ Deploy and run the project                  │
  │ RUN Space shows live output                 │
  │ Successful deploy = team scores points      │
  └──────────────────────────────────────────────┘
                      │
                      ▼
         (repeat cycle)
```

**Multiple game modes within coding:**

- **Deathmatch Coding** — free-for-all. Everyone codes, everyone frags. Most productive coder + most frags wins.
- **Team Code Arena** — two teams. Each team has their own Spaces. Code rounds are collaborative. Frag rounds are team vs team. Test results determine advantages.
- **Capture the Build** — CTF variant. Capture the other team's Build Space. Holding it gives you access to their agents.
- **Race to Deploy** — first team to pass all tests and deploy wins. Frag rounds are intermissions.

The scoreboard tracks BOTH game performance and code metrics. The ultimate developer is the one who writes clean code AND lands headshots.

---

## Multiplayer Features

### Screen Sharing by Proximity

Walk up to a coworker's Window and it gets sharper. From across the room, shared Windows render as low-res thumbnails (bandwidth-friendly). As you approach, resolution increases. At reading distance, it's full resolution. Natural, proximity-based screen sharing — just like walking up to someone's desk.

### Pair Programming

Two players can work on the same Window. Player A owns the Window. Player B requests access. Player A sees a permission prompt:

`[Player B wants to view your terminal] [Allow] [Deny]`

If allowed, Player B sees the full-resolution Window. If Player A grants **control**, Player B can enter Pointer Mode and Keyboard Passthrough on that Window — true pair programming, both standing at the same wall screen.

### Voice Chat (Future)

Spatialized, distance-based voice. Louder when close, fades when far. Just like a real office. Talk to the person next to you at normal volume. Shout across the BUILD Space to someone in the TEST Space.

---

## quakeOS — Native Rendering Library

quakeOS is a library inside Q3IDE that renders content directly in the engine — no macOS app capture needed. It's the native alternative to ScreenCaptureKit streaming for content that Q3IDE can render itself.

### Default behavior: macOS capture

By default, everything works through capture. You see your actual macOS apps on walls: VS Code, iTerm, Chrome. ScreenCaptureKit streams their pixels. This is the foundation.

### Focus Mode: `.` key

Aim at any captured code Window — in FPS Mode or Pointer Mode — and press `.`. The captured macOS stream flips to **quakeOS native rendering**: the same file, rendered directly by the engine with FreeType fonts, syntax highlighting, line numbers, and git blame gutter. Sharp, fast, zero-latency. No macOS app needed.

Press `.` again — back to the captured macOS app.

**`.` on nothing:** If you press `.` while NOT aiming at any Window, quakeOS opens a **new empty file** as a Floating Window at your position. Instant scratchpad.

### What quakeOS renders natively

- **Source code** — syntax highlighted with tree-sitter (third-party tokenizer), line numbers, git blame gutter. Tree-sitter provides token-level parsing (keywords, strings, comments, operators) — different from the UML daemon which parses at class/method level.
- **Markdown** — headings, bold/italic, code blocks, links. Rendered with proper formatting.
- **Images** — .png/.jpg/.svg displayed as textures on Windows natively.

**NOT rendered:** PDFs, browser content, terminal output, or any interactive app. Those stay as captured macOS Windows.

### Nano-Level Editor

quakeOS includes a basic text editor — type, delete, undo, save. Like nano. Enough to fix a typo, edit a config file, or make a quick change without leaving Quake to open an editor. Not a replacement for VS Code — a complement for when you don't need the full power.

### Why it matters

- **Performance:** Native rendering is cheaper than streaming a VS Code window for a file you're just reading
- **Independence:** quakeOS-rendered content works without any macOS app running — important for the browser WASM port
- **Focus:** No distracting VS Code chrome, tabs, sidebar. Just the code on the wall.

### Font Rendering

FreeType or stb_truetype for sharp, scalable monospace text. This is real font rendering, not Quake's bitmap console font. The text must be as readable as a real editor at reading distance.

---

## OpenClaw Bot

An AI agent as a real Quake 3 bot player. OpenClaw runs around the map, frags other players/bots, picks up weapons, navigates — AND responds to your messages. It's your AI colleague with a railgun.

### Architecture

- OpenClaw has its own simple API: send/receive messages with a token
- Q3IDE bridges to OpenClaw via HTTP or WebSocket (`spatial/bot/api_bridge`)
- The bot uses Quake 3's native bot AI for movement and combat
- Chat messages are routed through the API bridge, not through Q3 network chat

### Chat Interface

A **dedicated Floating Window** — full conversation history, scrollable, with a text input field at the bottom. This is separate from Q3's built-in chat system. The chat Window works like any other Floating Window — can be dragged, pinned, resized.

### Behavior

The bot plays the game while it thinks. It doesn't pause to respond. It frags you while explaining your architecture. It picks up the BFG while suggesting a refactor. The bot is always in FPS Mode — it never stops playing.

---

## AI Runtime Geometry (Moonshot)

AI-generated game world in real-time. The environment adapts, grows, and restructures itself while you play.

### Props

AI generates furniture, objects, and models that get placed into existing rooms. "Put a desk here." "Add a whiteboard to the BUILD room." The AI creates the model and places it as a game entity.

### Structural Geometry

AI generates walls, rooms, corridors — dynamic mesh geometry layered on top of the static BSP map. "Create a new room connected to GARAGE." "Expand the TEST space." This uses a runtime mesh system separate from the compiled BSP — BSP stays static, AI-generated geometry is additive. Extremely ambitious, may require custom mesh collision and rendering alongside BSP.

---

## Browser-Ready Port

Full Q3IDE compiled to WebAssembly via Emscripten. Open a URL, play Q3IDE in the browser.

### Capture Adapter

ScreenCaptureKit doesn't exist in browsers. The browser port replaces it with browser-native alternatives: Screen Share API (getDisplayMedia), iframe capture, or server-side streaming. The `capture/` layer's C-ABI contract stays the same — only the implementation behind it changes.

### Input Adapter

Browser pointer lock for FPS mouse, keyboard event capture for Passthrough mode. The interaction model (dwell, Pointer Mode, Edge Zone) maps cleanly to browser events.

### What stays the same

Everything in `spatial/` — the entire Window system, UML, Spaces, Portals, Ornaments, agent integration — is engine-agnostic and works unchanged. This is why the swappable architecture matters.

---

## Window Management

### Close

- Floating Windows: close button (×) on Window Bar
- Anchored Windows: close Ornament button or `/q3ide_detach`
- Closing stops capture and removes the Window entity

### Move — Long Press

Long press (hold left-click ~300ms) on a Window enters drag mode. Mouse movement drags the Window. Release to place. Near a wall = snaps to Anchored. In open Space = remains Floating.

### Resize — Cmd + Scroll

`Cmd` + scroll wheel resizes the focused Window. Scroll up = larger, scroll down = smaller. Aspect ratio preserved.

### Lock / Pin

Lock a Window in place. Lock icon on Window Bar Ornament. Locked Windows skip drag mode on long press.

### Snap Alignment

Dragging a Floating Window near another = they snap edge-to-edge. Subtle guide lines appear during drag. Toggleable in settings.

### Window Persistence

Layouts persist across Space changes, death/respawn, and session restarts. Windows are permanent fixtures in the world — dying and respawning does NOT affect your Windows. Saved to `.q3ide/layout.json` in the project directory. Includes position, scale, style, anchor, lock state, file association, agent connection.

All config lives in the project-level `.q3ide/` folder:
- `.q3ide/layout.json` — Window positions and presentation styles
- `.q3ide/hotkeys.toml` — custom keybindings
- `.q3ide/.env` — API keys (encrypted at rest, never committed to git)
- `.q3ide/spatial_map.json` — file-to-Space classification cache

### Smart Rendering

Windows not visible don't consume capture resources. See the **Performance & Rendering** section for the full adaptive distance-based quality system, dirty frame detection, IOSurface zero-copy, and GPU budget management.

---

## Interaction Model

### The Core Principle

The crosshair is the pointer. The railgun is the mouse. You aim at the world to play. You aim at a Window to work. There is no mode switch — there is only what you're looking at.

### Three Interaction States

```
FPS MODE (default)
    Mouse → look around | WASD → movement | Click → fire | Keys → game + Q3IDE shortcuts
         │
         │ crosshair dwells 150ms on Window (Hover Effect begins)
         ▼
POINTER MODE
    Mouse → pointer on Window | Click → click in app | WASD → movement
    Long press → drag | Cmd+Scroll → resize | Right-click → context menu
         │
         │ click in Window or press Enter
         ▼
KEYBOARD PASSTHROUGH
    Mouse → pointer on Window | Click → click in app | ALL keys → captured app
    Escape → exit (Digital Crown, always works)
         │
         EXIT: Edge Zone (20px border push) OR Escape
         ▼
FPS MODE
```

### Dwell Time (150ms)

Prevents accidental interaction during combat. Fast flick across a Window = no trigger.

### Distance Threshold

Too far away = left-click shoots. Close enough to read = left-click interacts.

### Edge Zone Exit (20px)

Push pointer past the Window border = smooth return to FPS. Like sliding past a monitor bezel.

### Escape — The Digital Crown

Always works. Instant FPS Mode. The one key that always means "back to the game."

### Weapon Behavior in Pointer Mode

The weapon stays visible. If you click (which sends a click to the captured app), the weapon ALSO fires cosmetically — animation plays, sound plays, projectile renders and flies through the Window. But no ammo is consumed and no damage is dealt. Pure vibes. The railgun beam literally shoots through the terminal while you click a button in vim.

This reinforces the design: the game never pauses. You're always in the arena. Even when you're working.

### Magic Mouse Enhancements

| Gesture | Action |
|---------|--------|
| Two-finger swipe | Scroll inside Window |
| Pinch | Zoom in/out on Window content |
| Three-finger swipe left/right | Switch Windows (Tab cycle) |
| Force Touch / deep press | Enter Keyboard Passthrough directly |

---

## Edge Cases & Combat Rules

### Invincibility While Working

When a player is in **Pointer Mode**, **Keyboard Passthrough**, or **Theater Mode**, they take **no damage**. They are invincible. This is a hard rule.

You chose to work. You're protected. A bot's rocket hits you, it does nothing. Your health doesn't change. You don't flinch. You keep typing.

When you exit back to FPS Mode, you're vulnerable again immediately.

### Windows Persist Through Death

When you die and respawn, all your Windows stay exactly where they are. Anchored Windows remain on their walls. Floating Windows remain in their positions. Nothing moves, nothing resets. Windows are permanent fixtures in the world, not tied to your player entity.

### Captured App Quits or Crashes

If the captured application quits or crashes, its Window persists in the world. The last captured frame is frozen on the texture. A "Disconnected" overlay appears on the Window (subtle, in the Window Bar Ornament). If the application is relaunched, Q3IDE detects it by matching the app bundle ID and window title, reconnects the capture stream, and the Window resumes live updating. No manual intervention needed.

### Window Overlap (Z-Order)

When two Floating Windows overlap in 3D space, the Window **closer to the player** wins (renders in front). This is natural depth sorting — the same way real objects work. No special z-order management needed, the 3D renderer handles it.

### Agent Notification During Keyboard Passthrough

If an LLM agent finishes a task while you're deep in Keyboard Passthrough on another Window, you are notified through **all passive channels simultaneously** but **never forced out** of Passthrough:

- Kill feed shows the event (top-right, scrolls like a frag notification)
- Spatial audio notification from the agent's Window position
- The agent's Window pulses (glow animation, 2 cycles)
- The agent's status Ornament updates

You notice naturally and decide when to exit Passthrough. The system never interrupts your typing.

### Screen Recording Permission Denied

If macOS Screen Recording permission is not granted, Q3IDE shows an error overlay with clear instructions on how to grant permission in System Settings → Privacy & Security → Screen Recording. The game itself continues to run — you can play, but all Windows are empty/placeholder until permission is granted and the engine is restarted.

### q3dm17 as MVP Map

q3dm17 (The Longest Yard) is one big open Space for MVP. There is no physical separation between the 8 Spaces on this map. All Windows go wherever you place them. Proper room-based Spaces with physical separation come with the custom map (Batch 16).

---

## Theater Mode — `T` Key

Pressing `T` activates **Theater Mode** — full distraction-free focus.

- Game world blacks out. BSP geometry, skybox, lighting — all fade to black.
- Focused Window (or Window Group) expands into a **curved panoramic layout** wrapping around the player.
- All other Windows hide. Only focused content visible.
- Spatial audio mutes game sounds, keeps work notifications.
- Other players' rockets still fly but you can't see them. You're in the zone.

Exit: press the key again, or Escape. World fades back in.

---

## Office Mode — `L` Key

Pressing `L` toggles **Office Mode**. The game world spawns an office environment around the player:

- A desk appears at your position
- Monitors on the desk displaying your active Windows
- An office chair (your player sits)
- Ambient office lighting replaces game lighting
- Keyboard and mouse visible on desk

Office Mode is a comfort zone — when you want to feel like you're at a desk, not in an arena. Your Windows arrange themselves on the virtual monitors. Toggle `L` again and the office despawns, you're back in the arena with your weapons.

Future enhancement: different office styles (cozy home office, corporate, standing desk, café).

---

## Visual Design

### Vibrancy — `Shift+T`

Dynamic text contrast boosting on Window backgrounds. When the game world behind a Floating Window is visually busy, Vibrancy increases text weight and adjusts background opacity for legibility. `Shift+T` toggles it on/off (since `T` is Theater Mode).

### Chromeless Windows

Remove all chrome — no Window Bar, no Ornaments, no bezel. Raw captured content on a surface. Used for: clean embedded displays, clocks, status lights, immersive video playback, ambient art.

---

## Ornament System

### Sidebar Ornament

Vertical Ornament on left edge of a Window showing file tree navigation. Collapses to icons, expands on hover.

### Volume Ornaments (Future)

Controls attached to 3D Volume containers (dependency graphs, schemas).

### Distance Scaling

Ornament text and icons scale with player distance. Close = full labels. Far = icon-only indicators.

### Face-the-Player

Ornaments on Anchored Windows rotate to always face the player. Window content stays flat, Ornaments billboard toward the viewer.

### Menus & Popovers

Right-click in Pointer Mode opens a context menu. Panel extending beyond Window boundaries. Actions: file ops, git ops, agent ops, window ops.

---

## Home View — `H` Key

Full-screen overlay showing:

- **All Spaces** as Portal thumbnails with live previews
- **All active Projects** with status indicators
- **All LLM agents** with status, Space assignment, token usage
- **Player locations** (multiplayer)

The mission control overview. Press `H` again or `Escape` to close.

---

## File Browser — `F` Key

Centered Floating Window:

- **Top:** 5 most recently opened Projects (large tiles)
- **Below:** Full Project list from `~/Projects/`, scrollable, alphabetical
- **Each tile:** name, language icon, last modified, active agent count

---

## Quick Open — `O` Key

Centered Floating Window with auto-focused search field:

- Powered by **fzf-style fuzzy matching** for MVP (fast, in-memory, no dependencies)
- Indexes file names, paths, and symbol names across the active Project
- As-you-type results, ranked by relevance
- Selecting navigates to the file's Space and Window
- Future upgrade: vector database indexing for semantic search (content snippets, code understanding)

Instantaneous. Equivalent to `Cmd+P` / `Cmd+Shift+O`.

---

## Run — `R` Key

Triggers the Project's run command. Auto-detects from project config. Output appears in RUN Space. Spatial audio notification on start and exit.

---

## Control Center

### Persistent Widget (bottom-right)

Shows: current Space name, active agent count + status dots, project name, time.

### Expanded Panel — `C` Key

- Volume control (game vs. work sounds)
- Agent overview (all agents, models, status, context usage)
- Window list (all open Windows with style, Space, status)
- Quick toggles (Theater Mode, Vibrancy, snap, notifications)
- Layout management (save, restore, reset)

---

## Capture & Screenshots

### Screenshot — `[` Key

- Pointer Mode on Window: captures that Window only (high-res)
- FPS Mode: captures full game view with all visible Windows
- Saved to `~/Pictures/Q3IDE/` with timestamp
- Flash effect + shutter sound

### Video Recording — `]` Key

- Toggles recording of full game view
- Red dot Widget indicator when active
- Saved to `~/Movies/Q3IDE/` as `.mp4`
- Session recording for async review (future core feature)

---

## Audio

### Per-Window Spatial Audio

Each Window emits its app's audio from its physical position. YouTube on a wall = audio from that wall. Volume attenuates with distance.

### Audio Ducking

Focused Window at full volume. All others ducked ~70%. Smooth crossfade on Focus change. Music/video playback respects ducking.

### Notification Sounds

- **Spatial** — positioned in 3D from source Space
- **Q3 Announcer** — "BUILD COMPLETE" / "TESTS FAILING" / "DEPLOY SUCCESSFUL"
- **Distinct from game audio** — never confused with weapons/frags
- Not user-configurable — we design the right sounds

### Multiplayer Audio (Future)

Other players' Window audio spatialized from their position. Coworker's music faintly heard from their Space.

---

## Performance & Rendering

### Core Principle: Don't Fuck Up the Game

The game FPS is sacred. Quake III Arena must run at full speed at all times. Bots, projectiles, physics, netcode — none of it degrades because of IDE features. If the GPU budget is tight, Windows degrade first. The game never stutters.

### Current Bottleneck (measured)

- 0 windows = 40-50 FPS
- 31 windows = 23 FPS
- Each window costs ~0.5-0.7 FPS

**The murder weapon:** CMSampleBuffer → CVPixelBuffer → CPU BGRA→RGBA swizzle (per pixel!) → glTexSubImage2D (GL_RGBA). ALL 31 windows upload unconditionally at full 1920×1080. Even windows behind you, behind walls, in other rooms. 31 × 8MB × 25fps = **~6.4 GB/sec CPU→GPU bandwidth.** Insane.

### 7 Optimization Wins (implementation order)

| # | Win | What | Impact |
|---|-----|------|--------|
| 1 | Kill BGRA→RGBA swizzle | Format flag on UploadCinematic → GL_BGRA native | Eliminates 64M per-pixel CPU ops/frame |
| 2 | Visibility-gated uploads | Dot product + BSP trace before UploadCinematic | Skips 50-70% of uploads |
| 3 | Adaptive 8-tier resolution | SCK source-side downscale, 1 stream per window | 4-64x smaller uploads for distant windows |
| 4 | Static content detection | Dirty frame counter → 1fps for idle windows | Idle terminals cost nothing |
| 5 | SCK frame interval per tier | minimumFrameInterval matches distance tier | SCK never generates unneeded frames |
| 6 | Mipmap generation | glGenerateMipmap after upload | Distant windows look smooth, almost free |
| 7 | Texture Array (GL_TEXTURE_2D_ARRAY) | One array per tier, batched draw calls | 31 binds → ~8 binds |

**Projected result:** 6,400 MB/sec → ~155 MB/sec CPU→GPU bandwidth. **~40x reduction.** 23 FPS → 40+ FPS with 31 windows.

### Resolution Tiers (8 levels)

| Tier | Distance | Scale | Resolution (1080p) | Upload size |
|------|----------|-------|-------------------|-------------|
| 0 | 0-120u (0-3m) | 1.0 | 1920×1080 | ~8.3 MB |
| 1 | 120-240u (3-6m) | 0.75 | 1440×810 | ~4.7 MB |
| 2 | 240-480u (6-12m) | 0.5 | 960×540 | ~2.1 MB |
| 3 | 480-720u (12-18m) | 0.375 | 720×405 | ~1.2 MB |
| 4 | 720-960u (18-24m) | 0.25 | 480×270 | ~0.5 MB |
| 5 | 960-1200u (24-30m) | 0.1875 | 360×202 | ~0.3 MB |
| 6 | 1200-1800u (30-45m) | 0.125 | 240×135 | ~0.13 MB |
| 7 | >1800u (>45m) | 0.0625 | 120×67 | ~0.03 MB |

**Aim override:** Crosshair aimed directly at any window (dot product > 0.95) → full res regardless of distance. Sniper zoom on a terminal across the map.

**Distance override:** Within 120u (3m) → always tier 0.

**SCK frame interval per tier:** tier 0 = 1/25, tier 1-2 = 1/15, tier 3-4 = 1/10, tier 5-6 = 1/5, tier 7 = 1/2.

### Visibility-Gated Texture Uploads

Before calling UploadCinematic for each window:
1. **Dot product:** player view direction · (window_pos - player_pos). If < 0 → behind player → **skip upload**
2. **BSP trace:** ray from player eye to window center. If hits anything → occluded → **skip upload**

Skipped windows keep their last texture on GPU (stale but invisible). Refresh instantly when visible again.

### Static Content Detection

Per window: count dirty frames over rolling 1-second window. If < 2 dirty frames → classify as "static" → drop to 1fps capture. Idle terminal with blinking cursor = nearly free. Resets instantly when content changes.

### Texture Throttle During Placement

While placement queue is draining (area transition, dogs sitting down):
- ALL windows throttle to 2fps texture updates (from normal 25fps)
- 31 × 25fps = crushing. 31 × 2fps = breathing room for placement work.
- Restore to 25fps when queue empty (all dogs settled).

### Render Order

Windows are **real 3D scene geometry**. They render as part of the game world, not as overlays:
- Rockets fly between you and a Window
- Depth sorting, occlusion, and lighting work naturally
- A Window behind a pillar is occluded by the pillar
- Dynamic light from Window content illuminates nearby surfaces

### IOSurface Zero-Copy (Future)

Eliminates CPU memcpy entirely. Current: SCK → CPU swizzle → glTexSubImage2D. Future: SCK → IOSurface → CGLTexImageIOSurface2D (GPU direct). For Vulkan: `VK_EXT_metal_objects` via MoltenVK. This is a future optimization — implement the 7 wins above first, they may be sufficient.

### Portal Preview Budget

⚠️ Each visible Portal is a full second render pass. Biggest performance risk. Search Quake 3 community repos for existing implementations — don't build from scratch.

- Close to Portal: quarter resolution, 15fps
- Medium distance: thumbnail, 5fps
- Far away: static snapshot, refresh every few seconds
- Looking directly at Portal (dwell): half resolution, 30fps
- Cap simultaneous active Portals

### Per-Window Performance Metrics

Exposed via Performance Widget and `/q3ide_perf` console command:

| Metric | Description |
|--------|-------------|
| capture_fps | Actual frames received from SCK |
| upload_fps | Actual texture uploads to GPU |
| dirty_ratio | % of frames that were dirty |
| bandwidth | MB/s for this window's texture data |
| latency | ms from SCK callback to texture visible |
| vram | MB consumed by texture + mipmaps |
| skip_count | Frames skipped (visibility culling) |
| current_tier | Resolution tier (0-7) |
| static_flag | Is window classified as static |

### Memory Budget

At full optimization:
- Tier 0 (close, ~3 windows): 3 × 8.3 MB = ~25 MB
- Tier 2-4 (medium, ~10 windows): 10 × 1 MB avg = ~10 MB
- Tier 5-7 (far/thumbnail, ~18 windows): 18 × 0.1 MB avg = ~2 MB
- **Total ~37 MB** for 31 windows (vs current ~250 MB)

RX 580 has 8GB VRAM. Quake 3 uses ~200-500MB depending on map and textures. Leaves plenty of room. But the adaptive system ensures we never hit VRAM limits — distant Windows get thumbnailed aggressively.

### Three Monitor Rendering

With triple monitors, the visible area triples. More walls visible = more Windows potentially in view = higher capture demand. The adaptive budget handles this naturally — more visible Windows means each gets a smaller slice of the budget. Windows on peripheral monitors (left/right) render at reduced quality since you can't read them without turning your head anyway.

---

## Keybinds

### Core Shortcuts

| Key | Action |
|-----|--------|
| `Tab` | Cycle Focus to next Window (globally) |
| `Shift+Tab` | Cycle Focus to previous Window |
| `Shift+1`–`Shift+8` | Teleport to Space |
| `1`–`8` | Select weapon (standard Q3 binds preserved) |
| `X` | Grapple Hook (movement + Window navigation) |
| `T` | Theater Mode (toggle blackout + curved wrap) |
| `Shift+T` | Vibrancy toggle (text contrast boost on Windows) |
| `U` | UML Navigator (full 3D interactive diagram) |
| `Shift+U` | Cycle UML diagram type (Class → Component → Dependency) |
| `N` | New agent on aimed UML node (spawns iTerm + Claude with file context) |
| `.` | Toggle quakeOS focus mode on aimed Window / new empty file if aiming at nothing |
| `F` | File Browser |
| `O` | Quick Open (fuzzy search) |
| `R` | Run Project |
| `H` | Home View |
| `C` | Control Center |
| `M` | Minimap (toggle enlarged view) |
| `K` | Virtual Keyboard (show/edit hotkey bindings) |
| `L` | Office Mode (toggle desk/monitors/chair) |
| `[` | Screenshot |
| `]` | Toggle video recording |
| `Escape` | Exit to FPS Mode (Digital Crown) |

### Pointer Mode

| Input | Action |
|-------|--------|
| Long press (~300ms) | Drag Window |
| `Cmd` + Scroll | Resize Window |
| Right-click | Context menu |
| Enter | Keyboard Passthrough |
| Edge Zone push | Exit to FPS Mode |

### All Keys Are Programmable

Press `K` to see and edit every binding. Any key can be rebound to any action. Bookmarks, teleports, commands, agent controls — all bindable.

---

## Map Skins / Environments (Future)

Same layout, different aesthetics:

- **Default** — Quake III Arena classic
- **Office** — glass walls, whiteboards (see Office Mode)
- **Space Station** — sci-fi, holographic displays
- **Cyberpunk** — neon, rain, holographic billboards
- **War Room** — military situation room (ideal for multi-agent management)

---

## Volume Baseplate (Future)

Subtle glowing floor perimeter showing Space boundaries. Color matches Space aesthetic. Helps spatial awareness in large maps.

---

## Project Decisions

Foundational decisions about the project itself — not features, but how Q3IDE exists in the world.

| # | Decision | Answer |
|---|---------|--------|
| PD1 | **License** | BSL 1.1 (Business Source License) — source-available, no production use without permission. Converts to MIT after 4 years. |
| PD2 | **Team** | Solo project. |
| PD3 | **First user** | Any developer who wants a spatial IDE. |
| PD4 | **Current usage** | Already in use — monitoring terminals on walls while coding alongside. |
| PD5 | **Platform** | macOS first (ScreenCaptureKit foundation). Linux/Windows when contributed by others. |
| PD6 | **Dogfooding** | Building Q3IDE inside Q3IDE after VR batch is complete. |
| PD7 | **Scope management** | 119+ features managed by batch milestones. Stop, test, refine after each batch. No runaway scope. |
| PD8 | **Launch strategy** | Private until actually useful. No premature hype. |
| PD9 | **Distribution** | Users bring their own `pak0.pk3` (legal copy of Quake 3). Q3IDE ships engine + spatial layer only. |

---

## Open Questions

Decisions deferred. Will resolve as implementation progresses.

| # | Question | Options Considered | Notes |
|---|---------|-------------------|-------|
| OQ1 | **Multi-project Space layout** — when multiple projects are open, how do they share the 8 Spaces? | Each project gets its own 8 Spaces / All share same Spaces with project tags / Each project gets a map wing | Big architectural decision. Impacts map design, navigation, and window routing. Will decide after playing with multi-window in-game. |
| OQ2 | **Window-to-Space auto-assignment rules** — what specific heuristics determine iTerm→BUILD, Chrome→RESEARCH? | App bundle ID / Window title keywords / User-defined rules | Hybrid confirmed (auto-suggest + player confirms). Exact rules TBD. Algorithm is swappable. |
| OQ3 | **File-to-Room organization algorithm** — how are files within a Space distributed to specific walls and positions? | By importance (git frequency) / By relationship (imports = adjacent walls) / By role (entry point = door wall) | The algorithm is swappable. Will figure out by experimentation. Not blocking any batch. |
| OQ4 | **Plugin/extension system** — how do community contributors add features to Q3IDE? | Lua scripts / WASM plugins / Native dylibs / TBD | Planned but mechanism undecided. Architecture should keep extension points in mind. |
| OQ5 | **Logging strategy** — where does debug output go and how verbose? | `.q3ide/debug.log` file (async buffered writes to avoid FPS impact). Claude Code runs sandboxed in Docker — logs are the only debugging channel. Need structured log levels (error/warn/info/debug). Must not drop FPS. |
| OQ6 | **First-run / onboarding experience** — what does a new user see on first launch? | Not designed yet. Currently prototype stage — just drop in the map. Future: possibly a HUD overlay with keybinds on first launch. |
| OQ7 | **Focus mode file detection** — when pressing `.` on a captured VS Code window, how does Q3IDE know which file is displayed? | Window title parsing ("auth.ts — VS Code") is fragile. May need editor-specific plugins, LSP bridge, or filesystem watcher correlation. TBD. |
| OQ8 | **Portal rendering performance** — each Portal is a full second render pass of the destination scene. Multiple visible Portals = multiple render passes per frame. This is the biggest performance risk in the project. | Existing Quake 3 Portal rendering implementations exist in community repos — LLM should search for and reference working implementations rather than building from scratch. It's hard to get right. Optimize aggressively: render Portal views at reduced resolution, limit Portal update rate by distance, cap simultaneous active Portals. |
| OQ9 | **Central state management** — no event bus or state coordinator is defined. WindowEntity, Spaces, agents, UML, layout, perf are all independent systems. When agent finishes → Window pulses → UML turns green → Kill Feed fires, what orchestrates that chain? | Intentionally deferred to the very end of the project. Want to stay flexible during development — premature state management causes avalanche refactoring. Build the systems independently first, add coordination layer last when all behaviors are known. |
| OQ10 | **IOSurface zero-copy threading model** — with ring buffer, threading is clear (SCK writes background, renderer reads main). With IOSurface zero-copy (Batch 3), SCK and OpenGL/Vulkan share a GPU surface. Who synchronizes access? IOSurfaceLock stalls the GPU pipeline. | Not designed yet. LLM implementing Batch 3 should run a Q&A session with the project owner about synchronization strategy before writing code. Options: double-buffered IOSurfaces, fence-based sync, or accept one-frame latency with async handoff. |

---

## Terminology

| Term | Origin | Meaning |
|------|--------|---------|
| Window | visionOS | 2D content container |
| Anchored Window | visionOS | Window on a BSP surface |
| Floating Window | visionOS | Window hovering in Space |
| Window Group | visionOS | Multi-Window layout |
| Billboard | Q3IDE | Oversized Window for distance |
| Ornament | visionOS | Viewport-pinned controls |
| Portal | Q3IDE + visionOS | Peek + teleport between Spaces |
| Widget | Q3IDE | Persistent mini view-only display |
| Space | visionOS | Themed workflow area |
| Volume | visionOS | 3D content container (future) |
| Hover Effect | visionOS | Glow + z-lift on 150ms dwell |
| Focus | visionOS | Currently targeted Window |
| Pointer Mode | Q3IDE | Mouse controls pointer inside Window |
| Keyboard Passthrough | Q3IDE | Full keyboard capture by Window |
| Edge Zone | Q3IDE | 20px border for pointer exit |
| Digital Crown | visionOS | Escape key — always exits |
| Dwell Time | Q3IDE | 150ms before Pointer Mode |
| Theater Mode | Q3IDE | Blackout + curved Window wrap |
| Office Mode | Q3IDE | Spawn desk/monitors/chair environment |
| Vibrancy | visionOS | Dynamic text contrast boost |
| Grapple Hook | Q3IDE | `X` key — movement + Window navigation |
| War Room | Q3IDE | Multi-agent management aesthetic |
| UML Navigator | Q3IDE | 3D interactive architecture diagram (`U` key) |
| Mini UML | Q3IDE | Always-visible compact node graph in HUD bar |
| Node Cloud | Q3IDE | Adaptive layout of UML nodes centered on current file |
| Pipe | Q3IDE | Animated 3D connection between UML nodes showing relationships |
| Transformer Unfold | Q3IDE | Progressive reveal animation on UML node boxes |
| Pre-Processor Daemon | Q3IDE | Background Rust process that parses code and builds UML data |
| Git Time Slider | Q3IDE | Scrub through git history watching UML architecture evolve |
| Node Status Overlay | Q3IDE | Red/green test status + agent activity dots on UML nodes |
| OpenClaw Bot | Q3IDE | AI agent as a Quake bot player — frags, navigates, responds to chat |
| WASM Port | Q3IDE | Full Q3IDE compiled to WebAssembly for browser play |
| quakeOS | Q3IDE | Native rendering library — renders code, markdown, images directly in-engine |
| Focus Mode | Q3IDE | `.` key — toggle captured macOS Window to quakeOS native render and back |

---

*The crosshair is the pointer. The grapple is the shortcut. The Portal is the door. You aim at the world to play. You aim at a Window to work. You walk through a Portal to travel. There is no mode switch. There is only what you're looking at.*

*"Meet me in the database room."*
*— An actual sentence you'll say to your coworker, and both of you will know exactly where that is.*