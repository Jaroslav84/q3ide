# Q3IDE — VisionOS Design Language Reference

## How Apple Handles Spatial UI (and How We Translate It)

This document defines the visual design system for Q3IDE. All naming, terminology, and design patterns follow Apple VisionOS Human Interface Guidelines, translated to a Quake III Arena FPS context.

**Source of truth:** ./plan/04-Q3IDE_SPECIFICATION.md is the full feature spec. This document covers visual design, interaction design, and design tokens only.

---

## 1. The Three Scene Types

Apple organizes everything into three levels of immersion:

### Window
The basic 2D container.
- Made of **Glass Material** — translucent, adapts to lighting, lets the world show through
- Has a **Window Bar** at the bottom (the grab handle to reposition it)
- Contains traditional 2D content (text, buttons, images)
- Can be repositioned freely in the Space by the player
- Multiple Windows can coexist side by side

**Q3IDE:** Each captured desktop window (terminal, VSCode, browser) is a visionOS-style Window. Q3IDE defines seven presentation styles: Anchored Window, Floating Window, Window Group, Billboard, Ornament, Portal, and Widget. See ./plan/04-Q3IDE_SPECIFICATION.md for full definitions.

### Volume
A bounded 3D container — like a fishbowl you can look at from any angle.
- Displays 3D content viewable from all sides
- Has a fixed bounding box
- Can have its own **Ornaments** (floating toolbars)
- visionOS 2: auto-tilts toward the viewer when raised above eye level

**Q3IDE:** Used for 3D data visualizations — git history timelines, dependency graphs, build pipeline status cubes. Future feature (Batch 14+).

### Immersive Space (Full Space)
The entire environment becomes the app. Only one app runs in Full Space.
- **Mixed:** passthrough with virtual objects overlaid
- **Progressive:** gradually reduces passthrough
- **Full:** complete environment replacement

**Q3IDE:** The Quake 3 map itself IS the Full Space. The BSP environment is the immersive space. Everything else (Windows, Volumes, Ornaments, Portals) exists within it. Theater Mode provides a "Full" immersion equivalent — world blacks out, only Windows remain.

---

## 2. Core UI Components

### Glass Material
The signature visionOS look. Every Floating Window background is glass.

**Properties:**
- Semi-transparent — world shows through
- Dynamically adapts contrast and color based on lighting behind it
- No light/dark mode distinction — glass handles it automatically
- Specular highlights and subtle reflections
- Frosted effect with depth-of-field blur

**Q3IDE Implementation:**
```
Shader: q3ide_glass
{
    // Multi-pass: render world behind, blur, tint, composite
    // Pass 1: capture background via render-to-texture
    // Pass 2: gaussian blur (simulated with multi-sample)
    // Pass 3: overlay with alpha-blended frost tint
    // Pass 4: composite desktop capture texture on top
    
    Glass tint color: rgba(0.95, 0.95, 0.97, 0.7)
    Blur radius: 16px equivalent at reference scale
    Specular highlight: subtle top edge, 3-5% opacity
    Corner radius: 12pt at reference scale
}
```

**Where Glass is used:**
- Floating Windows — always
- Ornaments — always
- Window Groups — always
- Billboards — NO (full brightness, no glass)
- Anchored Windows — NO (bezel/frame instead)
- Portals — NO (glowing edge frame instead)

### Window Bar
The grab handle at the bottom of a Window. How players reposition Windows.

**Properties:**
- Thin horizontal bar at the **bottom** of the Window (matching visionOS convention)
- Appears on Hover Effect (150ms crosshair dwell)
- Long press left-click (~300ms) OR +use key on the bar enters drag mode
- Contains: title (app name + window title), close (×), pin/lock, visibility toggle

### Ornament
Floating accessory elements attached to a Window. The killer visionOS concept.

**Properties:**
- Floats slightly in front of its parent Window on the z-axis
- Moves with the Window, maintains relative position
- Overlaps the Window edge by ~20pt — creates a sense of depth and belonging
- Houses interactive controls: buttons, tabs, segmented controls
- Glass Material background by default
- Should be equal width or narrower than parent Window
- Can be placed at any edge (bottom, top, leading, trailing)
- **Face-the-player:** Ornaments on Anchored Windows rotate to always face the player
- **Distance-scaled:** text/icons shrink to icon-only at distance, expand with labels up close

**Q3IDE Ornament types:**
- **Bottom Ornament (Window Bar):** title, close, pin, visibility, playback controls, approve/reject buttons
- **Top Ornament:** file path breadcrumb, git branch indicator
- **Side Ornament:** status dot (green/red/amber), scrollbar, zoom controls
- **Sidebar Ornament:** collapsible file tree on left edge (like visionOS Tab Bar)

**Example: Terminal Window on wall**
```
    ┌─ Top Ornament: ~/project/src/auth/middleware.ts (main)
    │
╭───┴──────────────────────────────╮
│                                  │
│   $ cargo build                  │
│   Compiling q3ide v0.1.0         │
│   Finished release [optimized]   │
│                                  │
╰───┬──────────────────────────────╯
    │
    └─ Bottom Ornament: [≡ Title] [📌 Pin] [👁 Vis] [✕ Close]

    ◉ Green dot ─── Side Ornament (tests passing)
```

### Tab Bar
Vertical navigation on the left side of a Window.

**Properties:**
- Vertical layout, fixed on left side
- Collapses to icons when not focused
- Expands to show labels when player looks at it (Hover Effect)
- Max 6 items recommended

**Q3IDE:** For multi-file code Windows — tabs showing open files. Collapses to icons, expands on crosshair hover.

### Portal
A Window that shows a live view into another Space — and doubles as a teleporter.

**Properties:**
- Distinctive glowing edge shimmer — unmistakable, inviting
- Destination Space rendered in real-time inside the frame (distance-adaptive quality)
- Depth fog inside gives a sense of passage
- Walk into the Portal to teleport

**Three types:** Space Portal (between Spaces), Player Portal (visit coworker), File Portal (import → destination).

**Not in visionOS:** Portals are a Q3IDE-original concept combining visionOS's "portal to another world" idea with Quake's teleporter pads. See ./plan/04-Q3IDE_SPECIFICATION.md for full Portal spec.

### Widget
A persistent miniature view-only display on the HUD.

**Properties:**
- Smaller than an Ornament
- Always visible, persists across rooms and movement
- No Focus, no Pointer Mode, no interaction — view only
- Shows live data at a glance

**Q3IDE uses:** Clock, agent status dots, build progress, test count, Performance Widget (FPS/VRAM/bandwidth).

**Not in visionOS:** visionOS doesn't have persistent HUD elements. Widgets are Q3IDE-specific for the FPS context where you need data visible while moving.

### Billboard
An oversized Anchored Window for distance viewing.

**Properties:**
- No Glass Material — full brightness, high contrast
- Scaled large, readable from across the Space
- Used for dashboards, status displays, shared information

**Not in visionOS:** visionOS Windows are designed for arm's-length viewing. Billboards are Q3IDE-specific for the large spatial environment.

---

## 3. Spatial Design Principles

### Depth & Z-Axis
Everything communicates hierarchy through depth.

**Rules:**
- Foreground content is closer to the viewer
- Background/deprioritized content recedes along z-axis
- Buttons and interactive elements advance toward the viewer
- Never add depth to text (it hurts legibility)
- Shadows reinforce spatial position

**Q3IDE Implementation:**
- Floating Windows have a configurable z-offset from walls
- Ornaments float ~8 units in front of their parent Window
- Active/focused Window moves slightly toward the player
- Inactive Windows recede slightly
- Drop shadows project onto walls/floor based on Window position
- Windows are **real 3D geometry** — rockets fly in front of them

### Ergonomics
Comfort-first design for extended use.

**visionOS Rules:**
- Horizontal head movement preferred over vertical
- Content within natural field of view
- No extreme vertical positions
- Wider aspect ratios for large canvases
- Minimize jarring/fast motion

**Q3IDE Rules:**
- Default Window placement: eye level, within 60° horizontal FOV
- Anchored Windows prefer wider aspect ratios
- Window spawn animation: smooth ease-in (0.35s), not instant pop
- Window close animation: smooth fade+shrink (0.25s), not instant vanish
- Auto-arrange command to reset all Windows to ergonomic positions

### Dimming
Used to focus attention.

**Q3IDE:** When a modal/sheet appears, the game world dims (brightness × 0.7). In Theater Mode, the world blacks out entirely and Windows wrap around the player in a curved panoramic layout.

---

## 4. Interaction Model

### Three Interaction States

```
┌─────────────────────────────────────────────────────────────┐
│                                                             │
│  FPS MODE (default)                                         │
│  Mouse → look around | Click → fire weapon | WASD → move   │
│                                                             │
│         │ crosshair dwells 150ms on Window surface          │
│         │ Hover Effect begins (glow + z-lift)               │
│         ▼                                                   │
│                                                             │
│  POINTER MODE                                               │
│  Mouse → pointer on Window | Click → click in app           │
│  WASD → still movement | Long press → drag Window           │
│  Cmd+Scroll → resize | Right-click → context menu           │
│  Weapon fires cosmetically (animation, sound, no damage)    │
│                                                             │
│         │ click inside Window OR press Enter                 │
│         ▼                                                   │
│                                                             │
│  KEYBOARD PASSTHROUGH                                       │
│  ALL keys → captured app | Escape → only exit               │
│                                                             │
│         EXIT: Edge Zone (20px border) OR Escape             │
│         ▼                                                   │
│                                                             │
│  FPS MODE                                                   │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### Input Translation

| visionOS | Q3IDE Equivalent |
|----------|-----------------|
| Eye gaze → Focus | Crosshair aim → Focus (150ms dwell) |
| Pinch → tap | Left-click → tap/fire |
| Long look → expand (tab bar) | Sustained hover → expand Ornament labels |
| Pinch + drag → move | Long press (~300ms) OR +use key → drag |
| Digital Crown → immersion | Escape → exit to FPS. Theater Mode key → full immersion |
| Direct touch | Not applicable (FPS context) |
| Two-handed gestures | Not applicable |

### Focus System

| Event | Visual Response |
|-------|----------------|
| Crosshair enters Window surface | Nothing (timer starts) |
| 150ms dwell reached | Hover Effect begins: glow + z-lift |
| Left-click while hovering | Enters Pointer Mode (or fires weapon if distance > threshold) |
| Enter in Pointer Mode | Activates Keyboard Passthrough |
| Mouse hits Edge Zone (20px border) | Smooth exit to FPS Mode |
| Escape (any state) | Hard exit to FPS Mode (Digital Crown) |

### Weapon in Pointer Mode

The weapon stays visible. Clicking fires the weapon cosmetically — animation plays, sound plays, projectile renders and flies through the Window. No ammo consumed, no damage dealt. The game never pauses. You're always in the arena.

### Grab / Drag

Two methods (player's choice):
- **Long press:** hold left-click ~300ms on a Window
- **+use key:** press E/F while crosshair is on a Window

Both enter drag mode. Mouse movement drags the Window. Release to place.

---

## 5. Interaction States

| State | Visual Treatment |
|-------|-----------------|
| **Idle** | Glass at base alpha, no glow, shadow at rest |
| **Hover** (150ms crosshair dwell) | +15% brightness, +4 z-lift, Ornament labels expand |
| **Focused** (selected as active via Tab) | +8% brightness, slight z-advance toward player |
| **Pointer Mode** (mouse inside Window) | Pointer cursor visible on Window surface, subtle border highlight |
| **Keyboard Passthrough** | Bright border highlight, "TYPING" indicator on Window Bar |
| **Grabbed** (long press / +use on Window Bar) | +2% scale, moves with player view, shadow stretches |
| **Dimmed** (behind modal/sheet) | -30% brightness, recedes on z-axis |
| **Theater Mode** | World blacked out, Window at full brightness, curved wrap |
| **Notification** (task complete) | Pulse glow animation (2 cycles), Ornament status changes |
| **Error** (build failed) | Red tint on Glass (subtle, 15% opacity), status Ornament red |
| **Passing** (tests passing) | Green tint on Glass (subtle, 15% opacity), status Ornament green |
| **Building** (compilation in progress) | Amber tint on Glass (subtle, 15% opacity), pulsing |

---

## 6. Q3IDE Design Token Sheet

Translated from visionOS spatial specs to Quake 3 engine units:

```c
// ═══════════════════════════════════════════════════════
// Q3IDE SPATIAL DESIGN TOKENS
// Adapted from Apple VisionOS Human Interface Guidelines
// ═══════════════════════════════════════════════════════

// --- Window Geometry ---
#define Q3IDE_CORNER_RADIUS         12.0f    // In reference points
#define Q3IDE_WINDOW_BAR_HEIGHT     32.0f    // Bottom bar
#define Q3IDE_WINDOW_BAR_OVERLAP    20.0f    // How much bar overlaps window

// --- Ornaments ---
#define Q3IDE_ORNAMENT_Z_OFFSET     8.0f     // Float distance in front of window
#define Q3IDE_ORNAMENT_OVERLAP      20.0f    // Edge overlap with parent window
#define Q3IDE_ORNAMENT_CORNER_RAD   16.0f    // Slightly more rounded than window
#define Q3IDE_ORNAMENT_ALPHA        0.80f    // Slightly more opaque than window

// --- Hover / Focus ---
#define Q3IDE_HOVER_GLOW_MULT       1.15f    // Brightness multiplier on hover
#define Q3IDE_HOVER_LIFT_Z          4.0f     // Z-axis advance on hover
#define Q3IDE_HOVER_FADE_IN         0.15f    // Seconds to hover state
#define Q3IDE_HOVER_FADE_OUT        0.25f    // Seconds from hover state
#define Q3IDE_FOCUS_BRIGHTNESS      1.08f    // Subtle brighten on focus
#define Q3IDE_MIN_TAP_TARGET        60.0f    // Minimum interaction area
#define Q3IDE_DWELL_TIME            0.15f    // 150ms before Pointer Mode

// --- Edge Zone ---
#define Q3IDE_EDGE_ZONE_PX          20.0f    // Border zone for pointer exit

// --- Animation ---
#define Q3IDE_ANIM_SPAWN_DURATION   0.35f    // Window appear animation
#define Q3IDE_ANIM_CLOSE_DURATION   0.25f    // Window close animation
#define Q3IDE_ANIM_GRAB_SCALE       1.02f    // Slight enlarge when grabbed
#define Q3IDE_ANIM_LERP_SPEED       8.0f     // Position interpolation per second
#define Q3IDE_ANIM_DEPTH_SHIFT      12.0f    // Z-shift for active/inactive
#define Q3IDE_ANIM_LONG_PRESS       0.3f     // 300ms for drag mode

// --- Depth & Hierarchy ---
#define Q3IDE_ACTIVE_Z_ADVANCE      6.0f     // Active window moves toward player
#define Q3IDE_INACTIVE_Z_RECEDE     4.0f     // Inactive window moves away
#define Q3IDE_MODAL_DIM_FACTOR      0.7f     // World brightness during modals
#define Q3IDE_THEATER_DIM_FACTOR    0.0f     // World brightness in Theater Mode (blackout)
#define Q3IDE_SHADOW_OFFSET_Y       8.0f     // Drop shadow distance
#define Q3IDE_SHADOW_BLUR           12.0f    // Drop shadow softness

// --- Spacing ---
#define Q3IDE_ELEMENT_MIN_SPACING   4.0f     // Between hoverable elements
#define Q3IDE_ORNAMENT_PADDING      12.0f    // Internal padding
#define Q3IDE_WINDOW_CONTENT_PAD    16.0f    // Content inset from glass edge

// --- Typography (Q3 font system) ---
#define Q3IDE_FONT_TITLE_SCALE      1.4f     // Title relative to base
#define Q3IDE_FONT_BODY_SCALE       1.0f     // Base size
#define Q3IDE_FONT_CAPTION_SCALE    0.8f     // Small labels
#define Q3IDE_FONT_WEIGHT_BODY      0.6f     // Medium (heavier than normal)
#define Q3IDE_FONT_WEIGHT_TITLE     0.8f     // Bold
#define Q3IDE_FONT_COLOR_PRIMARY    {1.0, 1.0, 1.0, 1.0}   // White
#define Q3IDE_FONT_COLOR_SECONDARY  {0.8, 0.8, 0.82, 0.9}  // Dim white
#define Q3IDE_FONT_COLOR_ACCENT     {0.4, 0.7, 1.0, 1.0}   // Blue accent

// --- Status Colors (as glass tints, not solid) ---
#define Q3IDE_STATUS_PASS_TINT      {0.2, 0.9, 0.4, 0.15}  // Green, subtle
#define Q3IDE_STATUS_FAIL_TINT      {1.0, 0.3, 0.3, 0.15}  // Red, subtle
#define Q3IDE_STATUS_BUILD_TINT     {1.0, 0.8, 0.2, 0.15}  // Amber, subtle
#define Q3IDE_STATUS_IDLE_TINT      {0.5, 0.5, 0.55, 0.10}  // Neutral

// --- Portal ---
#define Q3IDE_PORTAL_EDGE_GLOW      0.8f     // Edge shimmer intensity
#define Q3IDE_PORTAL_FOG_DENSITY    0.3f     // Depth fog inside portal
#define Q3IDE_PORTAL_PREVIEW_FPS    15       // Default portal preview framerate
```

---

## 7. Component Anatomy Cheat Sheet

### Floating Window (Glass Material)

```
                    ┌─ Top Ornament (file path / breadcrumb)
                    │  z-offset: +8 from Window
                    │  overlaps top edge by 20pt
                    │  faces the player (billboards)
                    │
    ╭───────────────┴──────────────────╮  ◄── Glass corner radius: 12pt
    │                                  │
    │   ┌──────────────────────────┐   │  ◄── Content area
    │   │                          │   │      (desktop capture texture)
    │   │   Live Terminal Window   │   │
    │   │                          │   │
    │   │   $ cargo build          │   │
    │   │   Compiling q3ide v0.1   │   │
    │   │                          │   │
    │   └──────────────────────────┘   │
    │                                  │  ◄── 20px Edge Zone (pointer exit border)
    ╰───────────────┬──────────────────╯
                    │
                    └─ Bottom Ornament (Window Bar + controls)
                       [≡ Title] [📌 Pin] [👁 Vis] [✕ Close]
                       z-offset: +8 from Window
                       overlaps bottom edge by 20pt
                       
                    ░░░░░░░░░░░░░░░  ◄── Drop shadow on floor/wall
```

### Anchored Window (Wall-Mounted)

```
    ╔══════════════════════════════════╗  ◄── Bevel/frame (no Glass Material)
    ║                                  ║
    ║   ┌──────────────────────────┐   ║
    ║   │                          │   ║
    ║   │   Live VSCode Window     │   ║
    ║   │                          │   ║
    ║   │   function auth() {      │   ║
    ║   │     return jwt.verify()  │   ║
    ║   │   }                      │   ║
    ║   │                          │   ║
    ║   └──────────────────────────┘   ║
    ║                                  ║
    ╚══════════════════════════════════╝
    
    ◉ ◉ ◉ ─── Status Ornament (flush below frame, faces player)
    [●] Tests passing  |  main  |  2m ago
    
    💡 Dynamic light emission from screen content
```

### Billboard

```
    ╔══════════════════════════════════════════════════════════╗
    ║                                                          ║
    ║     AGENT DASHBOARD              BUILD: ✅  TEST: ❌     ║
    ║                                                          ║
    ║  Claude    │ refactoring auth │ ████████░░ 80%  │ 45k   ║
    ║  GPT-4     │ researching WS   │ ██░░░░░░░░ 20%  │ 12k   ║
    ║  GLM-4     │ idle             │ ░░░░░░░░░░  0%  │  0k   ║
    ║                                                          ║
    ║  Tasks: 3 pending │ 1 active │ 7 completed              ║
    ║                                                          ║
    ╚══════════════════════════════════════════════════════════╝
    
    ^^^ Full brightness, no Glass Material, readable from across the Space
```

### Portal

```
    ┌─────────────────────────────┐
    │ ✦✦✦  BUILD SPACE  ✦✦✦      │ ◄── Glowing edge shimmer
    │                             │
    │   ╔═══════════════════╗     │
    │   ║ Live preview of   ║     │ ◄── Destination Space rendered
    │   ║ BUILD Space with  ║     │     at distance-adaptive quality
    │   ║ its Billboards,   ║     │
    │   ║ Windows, players  ║     │
    │   ╚═══════════════════╝     │
    │                             │
    │   ░░░ depth fog ░░░         │ ◄── Depth fog for passage feel
    │                             │
    └─────────────────────────────┘
    
    Walk into it → teleport to BUILD Space
```

### Theater Mode

```
    ████████████████████████████████████████████████
    ████████████████████████████████████████████████
    ████                                    ████████
    ████  ╭──────────────────────────────╮  ████████
    ████  │                              │  ████████
    ████  │    Curved Window Group       │  ████████
    ████  │    wrapping around player    │  ████████
    ████  │                              │  ████████
    ████  │    Code    │  Terminal  │    │  ████████
    ████  │    Editor  │  Output   │    │  ████████
    ████  │            │           │    │  ████████
    ████  ╰──────────────────────────────╯  ████████
    ████                                    ████████
    ████████████████████████████████████████████████
    ████████████████████████████████████████████████
    
    ^^^ World blacked out. Only Windows visible. Full focus.
```

---

## 8. VisionOS → Q3IDE Translation Table

| VisionOS Concept | Q3IDE Equivalent |
|-----------------|-----------------|
| Window | Floating Window (Glass Material) |
| Volume | 3D Data Visualization (future) |
| Full Space / Immersive Space | The Quake 3 map itself |
| Shared Space | Multiplayer server (everyone's Windows coexist) |
| Glass Material | Semi-transparent frosted shader on Floating Windows |
| Window Bar | Bottom Ornament with title + controls |
| Ornament | Floating control strips attached to Windows |
| Tab Bar | Sidebar Ornament (file tree, vertical tabs) |
| Hover Effect | 150ms crosshair dwell → glow + z-lift |
| Pinch gesture | Left-click (fire button) |
| Pinch + drag | Long press OR +use key → drag |
| Eye tracking → Focus | Crosshair → Focus |
| Digital Crown (immersion) | Escape key (exit to FPS Mode) |
| Digital Crown (environment blend) | Theater Mode key (blackout + curved wrap) |
| Spatial Audio | Per-Window audio from physical position + distance ducking |
| Passthrough | The game world IS the passthrough |
| Dimming | World brightness × 0.7 during modals |
| Vibrancy | Dynamic text contrast toggle on Glass Material |
| Portal | Portal: peek + teleport between Spaces/players |
| — | Widget: persistent HUD mini-display (Q3IDE-specific) |
| — | Billboard: oversized distance-readable display (Q3IDE-specific) |
| — | Edge Zone: 20px pointer exit border (Q3IDE-specific) |
| — | Keyboard Passthrough: full keyboard capture (Q3IDE-specific) |
| — | Weapon cosmetic fire in Pointer Mode (Q3IDE-specific) |

---

## 9. Design Rules Summary

1. **Glass, not solid.** Every Floating Window background is semi-transparent glass. Never opaque. Billboards and Anchored Windows are exceptions.
2. **Ornaments, not HUDs.** Controls float on Ornaments attached to Windows. The only HUD elements are Widgets and Ornaments (persistent viewport-pinned).
3. **Hover everything.** Every interactive element glows on 150ms crosshair dwell. No invisible hit targets.
4. **Depth communicates hierarchy.** Active Windows advance toward the player. Inactive recede. Ornaments always float in front of their parent.
5. **Animate transitions.** Windows spawn with smooth scale-in (0.35s). Close with smooth fade+shrink (0.25s). No instant pop/vanish.
6. **Spatial audio.** Notifications have position. A build completing in BUILD Space is heard from BUILD Space.
7. **3D geometry, not overlay.** Windows are real scene geometry. Rockets fly in front of them. Depth sorting, occlusion, and lighting work naturally.
8. **Distance = quality.** Close = full detail, full FPS, full resolution. Far = degraded. The GPU budget self-manages through spatial proximity.
9. **Don't fuck up the game.** The IDE is additive. Bots run. Physics work. Game FPS is sacred. If GPU budget is tight, Windows degrade first.

---

*The crosshair is the pointer. The grapple is the shortcut. The Portal is the door. Glass, not solid. Depth, not flat. Spatial, not tabbed.*