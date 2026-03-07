# Quake III IDE — VisionOS Design Language Reference

## How Apple Handles Spatial UI (and How We Translate It)

---

## 1. The Three Scene Types

Apple organizes everything into three levels of immersion:

### Window
The basic 2D container. <q3t>Our floating desktop panels.</q3t>
- Made of **glass material** — translucent, adapts to lighting, lets the world show through
- Has a **Window Bar** at the top (the grab handle to reposition it)
- Contains traditional 2D content (text, buttons, images)
- Can be repositioned freely in space by the user
- Multiple windows can coexist side by side

**Q3T Mapping:** Each captured desktop window (terminal, VSCode, browser) is a visionOS-style **Window**. Glass background, window bar for grab/move, freely positionable in the game world.

### Volume
A bounded 3D container — like a fishbowl you can look at from any angle.
- Displays 3D content viewable from all sides
- Has a fixed bounding box
- Can have its own **ornaments** (floating toolbars)
- New in visionOS 2: auto-tilts toward the viewer when raised above eye level

**Q3T Mapping:** Used for 3D data visualizations — git history timelines, dependency graphs, build pipeline status cubes. A Volume sitting on a table in the database room showing the schema as a 3D model.

### Immersive Space (Full Space)
The entire environment becomes the app. Only one app runs in Full Space.
- **Mixed:** passthrough with virtual objects overlaid
- **Progressive:** gradually reduces passthrough
- **Full:** complete environment replacement

**Q3T Mapping:** The Quake 3 map itself IS the Full Space. The BSP environment is the immersive space. Everything else (windows, volumes, ornaments) floats within it.

---

## 2. Core UI Components

### Glass Material
The signature visionOS look. Every window background is glass.

**Properties:**
- Semi-transparent — world shows through
- Dynamically adapts contrast and color based on lighting behind it
- No light/dark mode distinction — glass handles it automatically
- Specular highlights and subtle reflections
- Frosted effect with depth-of-field blur

**Q3T Implementation:**
```
Shader: desktop_glass
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

### Window Bar
The grab handle at the top/bottom of a window. How users reposition windows.

**Properties:**
- Thin horizontal bar below the window (visionOS places it at bottom)
- Appears on hover/focus
- Dragging it moves the entire window
- Contains the close button (×) and optionally a title

**Q3T Mapping:** 
- Our Window Bar sits at the **bottom** of floating panels (matching visionOS convention)
- Appears when crosshair hovers over the panel
- +use key on the bar enters grab mode
- Shows: window title (app name + window title), close (×), pin/unpin, visibility toggle

### Ornament
Floating accessory elements attached to a window. The killer visionOS concept.

**Properties:**
- Floats slightly in front of its parent window on the z-axis
- Moves with the window, maintains relative position
- Overlaps the window edge by ~20pt — creates a sense of depth and belonging
- Houses interactive controls: buttons, tabs, segmented controls
- Glass background by default
- Should be equal width or narrower than parent window
- Can be placed at any edge (bottom, top, leading, trailing)

**Q3T Mapping — this is huge for us:**
- **Bottom ornament:** playback controls for YouTube, build action buttons (approve/reject/rerun)
- **Top ornament:** file path breadcrumb for code windows, git branch indicator
- **Side ornament:** scrollbar, zoom controls
- **Status ornament:** build status light (green/red/amber dot), AI agent status

Example: A terminal window on the wall has:
- The terminal content as the main window
- A bottom ornament with: [Clear] [Copy] [Send to AI] buttons
- A top ornament showing: `~/project/src/auth/middleware.ts` breadcrumb
- A tiny side ornament showing: green dot = tests passing

### Tab Bar
Vertical navigation on the left side of a window.

**Properties:**
- Vertical layout, fixed on left side
- Collapses to icons when not focused
- Expands to show labels when user looks at it
- Max 6 items recommended

**Q3T Mapping:** For multi-file code windows — tabs on the left showing open files in that module room. Collapses to icons, expands on crosshair hover.

### Sidebar
Sub-navigation within a tab, inside the window.

**Q3T Mapping:** File tree explorer inside a code window when in expanded mode.

### Hover Effect
The most important interaction feedback in visionOS.

**Properties:**
- Elements visually "lift" and highlight when the user looks at them
- Subtle glow/brightening
- Shape-defined — the highlight follows the element's shape
- Interactive elements MUST have hover effects for discoverability
- Minimum 60pt tap target area (even if visual element is smaller)
- At least 4pt spacing between hoverable cells

**Q3T Mapping:**
- Crosshair hover on any interactive panel element triggers the glow
- Floating panels brighten subtly when crosshair passes over them
- Ornament buttons highlight on crosshair aim
- Wall-mounted screens get a subtle edge glow when aimed at
- Interactive elements: min 60-unit equivalent target area in game space

### Sheets (Modals)
Modal views presented at the center of the app.

**Properties:**
- Parent window pushes back and dims
- Close/back button always in top-left corner
- Appears at the same z-position as the window

**Q3T Mapping:** In-game settings panels, diff review overlays, AI task detail views. The game world dims slightly behind the sheet.

### Popovers & Menus
Context menus and dropdown-style overlays.

**Properties:**
- Can extend beyond window boundaries
- No arrows/tips needed (unlike iOS)
- Triggering button shows selected state (white background)
- Centered by default

**Q3T Mapping:** Right-click context menus on window content. File actions, git actions, copy/paste.

---

## 3. Spatial Design Principles

### Depth & Z-Axis
Everything in visionOS communicates hierarchy through depth.

**Rules:**
- Foreground content is closer to the viewer
- Background/deprioritized content recedes along z-axis
- Buttons and interactive elements advance toward the viewer
- Never add depth to text (it hurts legibility)
- Shadows reinforce spatial position

**Q3T Implementation:**
- Floating panels have a configurable z-offset from walls
- Ornaments float ~20 units in front of their parent window
- Active/focused window moves slightly toward the player
- Inactive windows recede slightly
- Drop shadows project onto walls/floor based on panel position

### Ergonomics
Comfort-first design for extended use.

**visionOS Rules:**
- Horizontal head movement preferred over vertical
- Content within natural field of view
- No extreme vertical positions
- Wider aspect ratios for large canvases
- Minimize jarring/fast motion

**Q3T Rules:**
- Default panel placement: eye level, within 60° horizontal FOV
- Wall screens prefer wider aspect ratios
- Panel spawn animation: smooth ease-in, not instant pop
- Panel close animation: smooth fade + shrink, not instant vanish
- Auto-arrange command to reset all panels to ergonomic positions

### Dimming
Used to focus attention.

**Q3T Mapping:** When reviewing a diff or interacting with a modal, the game world can subtly dim (reduce brightness by 20-30%). Optional, configurable.

---

## 4. Visual Design

### Typography
- System fonts slightly heavier than iOS for legibility at distance
- Regular weight → Medium for body text
- Semibold → Bold for titles
- Two extra-large title styles for editorial layouts
- White text by default on glass
- Emphasize via **Vibrancy** (dynamic contrast adjustment), not font size changes

**Q3T Mapping:**
- Q3 font system renders at heavier weight for in-world text
- Window titles: bold, high contrast
- Body text in ornaments: medium weight
- Status text: use color/brightness variation, not size

### Color
- Prefer transparency over solid colors
- Solid colors feel "heavy and out of place"
- White backgrounds ONLY for selected/active states
- Glass material cells use subtle shade differences
- Vibrancy for dynamic contrast adjustment

**Q3T Mapping:**
- Panel backgrounds: always glass (semi-transparent), never solid
- Active ornament button: bright/white
- Inactive ornament button: dim/glass-blended
- Status colors: green/amber/red as subtle tints on glass, not solid blocks

### App Icons
Three-layer 3D icons with glass effect.
- 1 background layer + up to 2 foreground layers
- 1024×1024 per layer, clipped to circle
- Glass layer auto-applied for depth + specular

**Q3T Mapping:** Each room/module could have a 3D icon at its entrance — layered, with glass depth. The `auth/` room has a shield icon. The `api/` room has a lightning bolt. Visual wayfinding.

---

## 5. Interaction Model

### visionOS Input Hierarchy
1. **Eyes + indirect pinch** (look at it, pinch to tap) — primary
2. **Direct touch** (reach out and poke it) — when close
3. **Trackpad/keyboard** — paired external devices
4. **Voice** — Siri integration
5. **Accessibility** — VoiceOver, Switch Control

### Q3T Input Hierarchy
1. **Crosshair + fire** (look at it, click to interact) — primary
2. **+use key** (context action: grab, toggle, activate) — secondary
3. **Console commands** (`/desktop_*`) — power user
4. **Keyboard passthrough** (future: type into focused window) — post-MVP

### Focus System
In visionOS, you look at something to focus it. In Q3T, you aim your crosshair at it.

| visionOS | Q3T Equivalent |
|----------|---------------|
| Eye gaze → focus | Crosshair aim → focus |
| Pinch → tap | Fire button → tap |
| Long look → expand (tab bar) | Sustained crosshair hover → expand ornament labels |
| Drag (pinch + move) | +use key + move → grab/drag |
| Digital Crown → immersion level | Console cvar → immersion level (dim world vs. full bright) |

---

## 6. The Q3T Design Token Sheet

Translated from visionOS spatial specs to Quake 3 engine units:

```c
// ═══════════════════════════════════════════════════════
// Q3T SPATIAL DESIGN TOKENS
// Adapted from Apple VisionOS Human Interface Guidelines
// ═══════════════════════════════════════════════════════

// --- Glass Material ---
#define Q3T_GLASS_ALPHA           0.72f    // Base transparency
#define Q3T_GLASS_BLUR_PASSES     3        // Multi-pass blur simulation
#define Q3T_GLASS_TINT_R          0.95f
#define Q3T_GLASS_TINT_G          0.95f
#define Q3T_GLASS_TINT_B          0.97f
#define Q3T_GLASS_SPECULAR        0.04f    // Top-edge highlight opacity
#define Q3T_GLASS_SHADOW_ALPHA    0.25f

// --- Window Geometry ---
#define Q3T_CORNER_RADIUS         12.0f    // In reference points
#define Q3T_WINDOW_BAR_HEIGHT     32.0f    // Bottom bar
#define Q3T_WINDOW_BAR_OVERLAP    20.0f    // How much bar overlaps window

// --- Ornaments ---
#define Q3T_ORNAMENT_Z_OFFSET     8.0f     // Float distance in front of window
#define Q3T_ORNAMENT_OVERLAP      20.0f    // Edge overlap with parent window
#define Q3T_ORNAMENT_CORNER_RAD   16.0f    // Slightly more rounded than window
#define Q3T_ORNAMENT_ALPHA        0.80f    // Slightly more opaque than window

// --- Hover / Focus ---
#define Q3T_HOVER_GLOW_MULT       1.15f    // Brightness multiplier on hover
#define Q3T_HOVER_LIFT_Z          4.0f     // Z-axis advance on hover
#define Q3T_HOVER_FADE_IN         0.15f    // Seconds to hover state
#define Q3T_HOVER_FADE_OUT        0.25f    // Seconds from hover state
#define Q3T_FOCUS_BRIGHTNESS      1.08f    // Subtle brighten on focus
#define Q3T_MIN_TAP_TARGET        60.0f    // Minimum interaction area

// --- Animation ---
#define Q3T_ANIM_SPAWN_DURATION   0.35f    // Window appear animation
#define Q3T_ANIM_CLOSE_DURATION   0.25f    // Window close animation
#define Q3T_ANIM_GRAB_SCALE       1.02f    // Slight enlarge when grabbed
#define Q3T_ANIM_LERP_SPEED       8.0f     // Position interpolation per second
#define Q3T_ANIM_DEPTH_SHIFT      12.0f    // Z-shift for active/inactive

// --- Depth & Hierarchy ---
#define Q3T_ACTIVE_Z_ADVANCE      6.0f     // Active window moves toward player
#define Q3T_INACTIVE_Z_RECEDE     4.0f     // Inactive window moves away
#define Q3T_MODAL_DIM_FACTOR      0.7f     // World brightness during modals
#define Q3T_SHADOW_OFFSET_Y       8.0f     // Drop shadow distance
#define Q3T_SHADOW_BLUR           12.0f    // Drop shadow softness

// --- Spacing ---
#define Q3T_ELEMENT_MIN_SPACING   4.0f     // Between hoverable elements
#define Q3T_ORNAMENT_PADDING      12.0f    // Internal padding
#define Q3T_WINDOW_CONTENT_PAD    16.0f    // Content inset from glass edge

// --- Typography (Q3 font system) ---
#define Q3T_FONT_TITLE_SCALE      1.4f     // Title relative to base
#define Q3T_FONT_BODY_SCALE       1.0f     // Base size
#define Q3T_FONT_CAPTION_SCALE    0.8f     // Small labels
#define Q3T_FONT_WEIGHT_BODY      0.6f     // Medium (heavier than normal)
#define Q3T_FONT_WEIGHT_TITLE     0.8f     // Bold
#define Q3T_FONT_COLOR_PRIMARY    {1.0, 1.0, 1.0, 1.0}   // White
#define Q3T_FONT_COLOR_SECONDARY  {0.8, 0.8, 0.82, 0.9}  // Dim white
#define Q3T_FONT_COLOR_ACCENT     {0.4, 0.7, 1.0, 1.0}   // Blue accent

// --- Status Colors (as glass tints, not solid) ---
#define Q3T_STATUS_PASS_TINT      {0.2, 0.9, 0.4, 0.15}  // Green, subtle
#define Q3T_STATUS_FAIL_TINT      {1.0, 0.3, 0.3, 0.15}  // Red, subtle
#define Q3T_STATUS_BUILD_TINT     {1.0, 0.8, 0.2, 0.15}  // Amber, subtle
#define Q3T_STATUS_IDLE_TINT      {0.5, 0.5, 0.55, 0.10}  // Neutral
```

---

## 7. Component Anatomy Cheat Sheet

### Floating Desktop Panel (VisionOS style)

```
                    ┌─ Top Ornament (file path / breadcrumb)
                    │  z-offset: +8 from window
                    │  overlaps top edge by 20pt
                    │
    ╭───────────────┴──────────────────╮  ◄── Glass corner radius: 12pt
    │                                  │
    │   ┌──────────────────────────┐   │  ◄── Content area
    │   │                          │   │      (desktop capture texture)
    │   │   Live Terminal Window   │   │
    │   │                          │   │
    │   │   $ cargo build          │   │
    │   │   Compiling q3t v0.1.0   │   │
    │   │                          │   │
    │   └──────────────────────────┘   │
    │                                  │
    ╰───────────────┬──────────────────╯
                    │
                    └─ Bottom Ornament (window bar + controls)
                       [≡ Title] [📌 Pin] [👁 Vis] [✕ Close]
                       z-offset: +8 from window
                       overlaps bottom edge by 20pt
                       
                    ░░░░░░░░░░░░░░░  ◄── Drop shadow on floor/wall
```

### Wall-Mounted Screen

```
    ╔══════════════════════════════════╗  ◄── Bevel/frame (configurable style)
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
    
    ◉ ◉ ◉ ─── Status ornament (flush below frame)
    [●] Tests passing  |  main  |  2m ago
    
    💡 Dynamic light emission from screen content
```

---

## 8. Interaction States

| State | Visual Treatment |
|-------|-----------------|
| **Idle** | Glass at base alpha, no glow, shadow at rest |
| **Hover** (crosshair on panel) | +15% brightness, +4 z-lift, ornament labels expand |
| **Focused** (selected as active) | +8% brightness, slight z-advance toward player |
| **Grabbed** (+use on window bar) | +2% scale, moves with player view, shadow stretches |
| **Dimmed** (behind modal/sheet) | -30% brightness, recedes on z-axis |
| **Notification** (task complete) | Pulse glow animation (2 cycles), ornament status changes |
| **Error** (build failed) | Red tint on glass (subtle, 15% opacity), status ornament red |

---

## 9. Summary: VisionOS → Q3T Translation Table

| VisionOS Concept | Q3T Equivalent |
|-----------------|----------------|
| Window | Floating Desktop Panel |
| Volume | 3D Data Visualization (git graph, schema model) |
| Full Space / Immersive Space | The Quake 3 map itself |
| Shared Space | Multiplayer server (everyone's windows coexist) |
| Glass Material | Semi-transparent frosted panel shader |
| Window Bar | Bottom grab bar on floating panels |
| Ornament | Floating control strips attached to panels |
| Tab Bar | Module/file tabs on code windows |
| Hover Effect | Crosshair-aim glow + lift |
| Pinch gesture | Fire button |
| +use (grab/drag) | +use key |
| Eye tracking → focus | Crosshair → focus |
| Digital Crown (immersion) | Console cvar (world dimming level) |
| Spatial Audio | Distance-based audio (build sounds, voice chat) |
| Passthrough | The game world IS the passthrough |
| Dimming | World brightness reduction during modals |
| Vibrancy | Dynamic contrast on glass based on world behind |
| Portal | Portal windows — see into another room's screens |

---

*"The game world is not the background. The game world is the operating system."*
