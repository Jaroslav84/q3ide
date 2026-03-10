# Q3IDE — Window Placement System

**This document replaces ALL existing placement logic.** The current implementation (rules 1-13 in the codebase) is garbage except for the performance optimizations around window reuse. Rewrite from scratch using these rules.

**Part 1** is the spec — HOW the system works (data structures, algorithms, edge cases).
**Part 2** is the stages — WHEN to build each piece (10 stages, one Claude Code session each).

---

## ⚠️ LLM Implementation Instructions

### What to KEEP (across all stages)
- Window entity reuse — windows are moved (x,y,z,angle update), never destroyed and recreated. The SCStream tunnel stays alive. This is the single best optimization in the current code. Do not touch it.
- The existing area detector code.

### What to DESTROY (Stage 4 onwards)
- All 13 current placement rules. Rip them out.
- The wall-facing dot product sort as the primary placement strategy.
- The 30m radius hardcode.
- The 16u step width measurement.
- The 32° deduplication.
- The geometry clamp margin approach.
- The 1-per-frame drain rate.
- Everything in the placement code except the window entity reuse and texture pipeline.

### Code rules (all stages)
- 200 lines per file. Max 400. Split if exceeded.
- `Q3IDE_` prefix on all constants in `q3ide_params.h`.
- Commit after completing each stage with a descriptive message.
- Implementation order is strict. Do not skip stages. Do not combine stages.
- Owner (Istvan) manages git and testing between stages.

---

## Constants

All constants go in `q3ide_params.h`. Use `#define` with `Q3IDE_` prefix. Quake units = inches.

```c
// ── Window sizing ──────────────────────────────────────────────
#define Q3IDE_IDEAL_WINDOW_SIZE       100    // 100u diagonal (aspect-fit target)
#define Q3IDE_MIN_WINDOW_SIZE          66    // 66u diagonal (absolute floor)
#define Q3IDE_MAX_WINDOW_SIZE         200    // 200u diagonal (exceptions: billboards, theater)
#define Q3IDE_WINDOW_WALL_RATIO      0.85f  // 85% of wall height

// ── Wall qualification ─────────────────────────────────────────
#define Q3IDE_MIN_WALL_HEIGHT          79    // 2m — walls shorter than this are ignored
#define Q3IDE_MAX_WALL_HEIGHT         394    // 10m — walls taller than this get capped sizing
#define Q3IDE_MIN_WALL_WIDTH          114    // min_window_width(66) + 2 × margin(24) = 114u
#define Q3IDE_WALL_MARGIN              24    // 24u (~60cm) reserved on each side of wall
#define Q3IDE_WINDOW_GAP               16    // 16u (~40cm) between windows on same wall

// ── Wall angle thresholds ──────────────────────────────────────
#define Q3IDE_WALL_MAX_ANGLE          5.0f   // ±5° from vertical for normal walls
#define Q3IDE_WALL_BIG_MAX_ANGLE     30.0f   // ±30° for billboards/monitors in the map

// ── Placement geometry ─────────────────────────────────────────
#define Q3IDE_WINDOW_WALL_OFFSET       3     // 3u offset from wall surface (anti-clipping)
#define Q3IDE_PLACEMENT_RADIUS       2400    // 60m scan radius

// ── Placement throttle ─────────────────────────────────────────
#define Q3IDE_PLACEMENT_QUEUE_CAP      32    // max queued placements
#define Q3IDE_PLACEMENT_FPS_GATE       30    // only place if last frame was above this FPS

// ── Leapfrog behavior ──────────────────────────────────────────
#define Q3IDE_LEAPFROG_DISTANCE       280    // 7m — min player movement before leapfrog triggers
#define Q3IDE_LEAPFROG_CHECK_INTERVAL  30    // check every 30 frames (~0.5s at 60fps)

// ── Limits ─────────────────────────────────────────────────────
#define Q3IDE_MAX_WINDOWS              64    // max simultaneous window entities
#define Q3IDE_MAX_CACHED_WALLS        128    // max walls in area cache
#define Q3IDE_MAX_WALL_SLOTS           8     // max window slots per wall

// ── Texture update throttle ────────────────────────────────────
#define Q3IDE_TEXTURE_FPS_NORMAL       25    // max texture update rate per window (normal)
#define Q3IDE_TEXTURE_FPS_PLACING       2    // throttled rate while placement queue is draining

// ── Adaptive resolution ────────────────────────────────────────
#define Q3IDE_RES_TIER_COUNT            8    // number of resolution tiers
#define Q3IDE_AIM_FULL_RES_DISTANCE   120    // 3m — within this distance, always full res
#define Q3IDE_AIM_FULL_RES_DOT       0.95f  // dot product threshold for "aiming directly at"

// ── Static content detection ───────────────────────────────────
#define Q3IDE_STATIC_THRESHOLD          2    // <2 dirty frames in 1 second → static
#define Q3IDE_STATIC_CAPTURE_FPS        1    // capture at 1fps when classified static
```

---

## General Window Constraints

These are always true, regardless of stage or placement mode.

1. **Minimum size = 100u diagonal.** Exception: user manually resizes.
2. **Aspect ratio is sacred.** Never stretched, never cropped.
3. **Double-sided display.** Render from behind with flipped image. Exception: flush against wall.
   Single polygon + `cull disable` shader. GPU mirrors UVs on back face via winding reversal — free, zero extra geometry.
4. **Single horizontal row per wall.** Never stacked vertically.
5. **Vertically centered on wall.** Middle. Like a projector screen.
6. **Vertical walls only.** ±5° threshold. Exception: in-map monitors/billboards ±30°.
7. **Never on ceilings or floors.**
8. **Never intersect each other.** Exception: user manually moves.
9. **Never placed outside the player's area/room.**
10. **3u wall offset.**
11. **85% wall height** (aspect-fit). Exception: < min or > max size, or wall > max height.
12. **Trace everything for collision.** BSP, entities, models, players. No shortcuts.

---

# PART 1 — SPEC

How the placement system works. Reference for all stages.

---

## Placement Architecture

### Two Modes

The placement system operates in two modes that switch automatically:

#### Mode 1: Area Transition

Triggered when: the area detector reports the player entered a new area.

```
Area transition detected
  → Throttle ALL window texture updates to Q3IDE_TEXTURE_FPS_PLACING (2fps)
  → Check for trained positions in this area (place those FIRST)
  → Pre-scan ALL walls in new area (expensive, once)
  → Build wall_cache[] (position, normal, width, height, pre-computed slots)
  → Sort walls by distance to player (closest first)
  → Queue ALL untrained windows for migration
  → Drain queue with FPS-adaptive throttle:
      → If last_frame_fps > Q3IDE_PLACEMENT_FPS_GATE (30fps):
          → Pick closest wall with a free slot
          → Move window entity to slot (update x,y,z,angle only)
      → If last_frame_fps ≤ 30: skip this frame, try next frame
  → When queue is empty:
      → Restore ALL windows to Q3IDE_TEXTURE_FPS_NORMAL (25fps)
```

Visual metaphor: 10 dogs follow you through a doorway. Dogs that know their spot (trained) go there first. The rest find a spot and sit down, one at a time. Closest spots fill first. If the room is lagging, dogs wait patiently until FPS recovers. All TVs freeze while dogs are moving — unfreeze when everyone's settled.

#### Mode 2: Within-Area Leapfrog

Triggered when: player moves > Q3IDE_LEAPFROG_DISTANCE (7m) from last placement check position AND player is NOT in area transition AND placement queue is empty.

Checked every Q3IDE_LEAPFROG_CHECK_INTERVAL frames (not every frame).

```
Player moved >7m from last check position
  → Find the window furthest from player (exclude trained windows)
  → Find the closest wall to player with a free slot (from cache)
  → If furthest_window_distance > closest_free_slot_distance:
      → Move window to new slot (update x,y,z,angle only)
  → Update last check position
```

The furthest dog gets up and runs to the closest open spot near you. One dog per check. Natural, smooth, no frame spike. Trained dogs never leapfrog.

---

## Wall Pre-Scanner

Runs once on area entry. Results cached until next area transition.

### What it does
1. Cast rays outward from player position in a sphere pattern (or use BSP area data)
2. Find all surfaces within Q3IDE_PLACEMENT_RADIUS (2400u / 60m) that are in the current area
3. Filter: only surfaces within ±Q3IDE_WALL_MAX_ANGLE (5°) of vertical
    - Exception: in-map monitors/billboards up to ±Q3IDE_WALL_BIG_MAX_ANGLE (30°)
4. Filter: height ≥ Q3IDE_MIN_WALL_HEIGHT (79u), width ≥ Q3IDE_MIN_WALL_WIDTH (114u)
5. Deduplicate: merge surfaces that are part of the same physical wall
6. For each qualifying wall, pre-compute window slots:
   - Number of slots = floor((wall_width - 2 × Q3IDE_WALL_MARGIN) / (window_width + Q3IDE_WINDOW_GAP))
   - Slot positions = evenly spaced along wall, centered
   - Capped at Q3IDE_MAX_WALL_SLOTS (8) per wall
   - Each slot: {position, normal, width, height, occupied flag}

### Data structures

```c
typedef struct {
    vec3_t      position;        // slot center in world space
    vec3_t      normal;          // facing direction
    float       width;           // available width for this slot
    float       height;          // available height for this slot
    int         windowId;        // -1 if free, window entity ID if occupied
} WallSlot_t;

typedef struct {
    vec3_t      center;          // wall center position
    vec3_t      normal;          // wall facing direction
    float       width;           // usable width
    float       height;          // floor-to-ceiling height
    float       distToPlayer;    // cached distance for sorting
    int         slotCount;       // how many windows fit
    int         slotsUsed;       // how many slots are occupied
    WallSlot_t  slots[Q3IDE_MAX_WALL_SLOTS];
} CachedWall_t;

typedef struct {
    int         windowId;        // which window
    int         areaId;          // which area this position belongs to
    vec3_t      position;        // saved world position
    vec3_t      normal;          // saved facing direction
    qboolean    trained;         // true = user placed it here manually
} TrainedPosition_t;
```

---

## Spread-Even Distribution

When placing N windows across M walls:

1. Sort walls by distance to player (closest first)
2. Place trained windows at their saved slots FIRST (they get priority)
3. If a trained slot is occupied by an untrained window → bump the untrained window
4. Round-robin remaining untrained windows: assign 1 per wall, cycling through walls
5. Continue cycling until all windows are placed or all slots are full
6. If more windows than slots: remaining stay in queue (leapfrog places them later as player moves)

---

## Window Sizing Algorithm

For each wall slot:

```
raw_height = wall_height × Q3IDE_WINDOW_WALL_RATIO (0.85)

if raw_height > Q3IDE_MAX_WINDOW_SIZE:
    raw_height = Q3IDE_MAX_WINDOW_SIZE

if raw_height < Q3IDE_MIN_WINDOW_SIZE:
    skip this wall (too small)

window_height = raw_height
window_width = window_height × captured_aspect_ratio

if window_width > slot_width:
    window_width = slot_width
    window_height = window_width / captured_aspect_ratio

// Final diagonal check
diagonal = sqrt(window_width² + window_height²)
if diagonal < Q3IDE_MIN_WINDOW_SIZE:
    skip this slot
if diagonal > Q3IDE_MAX_WINDOW_SIZE:
    scale down proportionally to fit
```

Always aspect-fit. Never stretch. Never crop.

---

## Collision Tracing

Before finalizing any window position, verify it doesn't clip into anything:

```
For each corner of the window quad (4 corners):
    trace from window center to corner + Q3IDE_WINDOW_WALL_OFFSET
    if trace hits ANYTHING (BSP, entity, model, player):
        shrink window or move to next slot

Trace against:
    - BSP world geometry (walls, floors, ceilings, pillars)
    - Entity bounding boxes (weapon pickups, health, armor)
    - Map models (decorative geometry, doors, platforms)
    - Other window entities (no overlap allowed)
```

If a window can't fit anywhere after collision checks: leave it in the queue, try again on next leapfrog cycle.

---

## Resolution Tiers

8 tiers, SCK source-side downscale. 1 SCStream per window, resolution changed dynamically. Stale texture shown during reconfiguration.

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

**Aim override:** dot product > Q3IDE_AIM_FULL_RES_DOT (0.95) → force tier 0 regardless of distance. Sniper zoom on a terminal across the map.

**Distance override:** within Q3IDE_AIM_FULL_RES_DISTANCE (120u / 3m) → force tier 0.

**SCK frame interval per tier:** tier 0 = 1/25, tier 1-2 = 1/15, tier 3-4 = 1/10, tier 5-6 = 1/5, tier 7 = 1/2. SCK never generates frames faster than needed.

---

## Edge Cases

### More windows than wall space
Windows stay queued. As the player moves (leapfrog mode), new walls become available and queued windows get placed.

### Tiny rooms with no qualifying walls
Windows float unplaced. They'll attach when the player moves to an area with walls. No error, no crash, just waiting dogs.

### Player teleports (Shift+1-8)
Same as area transition. Pre-scan new area, queue all windows for migration. Trained windows go to their remembered spots first.

### Player dies
Windows stay exactly where they are. They don't move on death. Player respawns and walks back to them (or they leapfrog to the new spawn area).

### Windows stay put within an area
Once windows are placed inside an area, they DO NOT reposition while the player stays in that area. No unnecessary shuffling. Dogs sit, dogs stay.
- Exception: area is bigger than Q3IDE_PLACEMENT_RADIUS (60m) — leapfrog rules apply within the 60m bubble only.
- Exception: user manually moves a window.

### Window manually repositioned by user (trained positions)
That window is flagged as `manually_placed = true` with its position saved **per area**. It is excluded from leapfrog and automatic area transition placement.

**Trained position memory:** When the user returns to this area later, the window goes back to its trained spot automatically. We teach our dogs where their place is.

- Untrained windows (never manually moved) use automatic placement as normal.
- If a trained window's saved wall slot is occupied by another window → trained window gets priority, the other window gets bumped to nearest free slot.
- Trained positions persist across area transitions. The wall_cache stores them.
- Trained position conflicts: if two trained windows want the same spot → first one placed wins, second finds nearest free slot.

---

## Stream Freeze Pattern (✅ IMPLEMENTED — use this everywhere)

`Q3IDE_WM_PauseStreams()` / `Q3IDE_WM_ResumeStreams()` — implemented in `q3ide_win_mngr.c`.

Calls `q3ide_pause_all_streams(cap)` / `q3ide_resume_all_streams(cap)` in the Rust dylib. Sets `STREAMS_PAUSED` atomic bool — `get_frame()` returns `None` → zero texture uploads → 100% FPS restoration. SCStreams stay warm. Last frame frozen on GPU (windows look alive, content is static).

**Use this in Stage 4** instead of manually throttling to 2fps per-window. The implementation is:
```c
// Area transition begins:
Q3IDE_WM_PauseStreams();   // freeze all — last frames stay on GPU
// ... drain placement queue with FPS gate ...
Q3IDE_WM_ResumeStreams();  // unfreeze when queue is empty
```
This is far simpler than a 2fps-per-window throttle AND gives better FPS headroom during placement.

---

## Performance Rules

**Current baseline:** 0 windows = 40-50fps. 31 windows = 23fps. Each window ~0.5-0.7fps cost (texture uploads at 25fps/window).

**Current bottleneck:** CMSampleBuffer → CVPixelBuffer → CPU BGRA→RGBA swizzle → glTexSubImage2D (GL_RGBA). All 31 windows upload unconditionally at full 1920×1080. 31 × 8MB × 25fps = ~6.4 GB/sec CPU→GPU bandwidth.

1. **Never recreate window entities.** Move them. The SCStream tunnel, texture, and render state persist.
2. **FPS-gated placement.** Only place/move a window if last frame was >30fps.
3. **Texture throttle during placement.** While placement queue is draining, ALL windows throttle to 2fps texture updates. 31 × 25fps = crushing. 31 × 2fps = breathing room. Restore to 25fps when queue is empty.
4. **Pre-scan once per area.** Wall cache is built on area entry, not per-placement.
5. **Leapfrog checks every 30 frames**, not every frame. ~2 checks/second at 60fps. No global texture throttle for leapfrog — single window moves are cheap.
6. **Queue cap = 32.** Excess stays in waiting list until slots free up.
7. **No animation on placement.** Just update position/angle. Cheapest possible operation.
8. **Distance-based render priority is separate.** Placement doesn't care about texture resolution. That's the renderer's job.

---

# PART 2 — IMPLEMENTATION STAGES

10 stages. Each stage = one Claude Code session. Owner (Istvan) manages git and testing between stages.

---

## STAGE 1 — Kill the BGRA→RGBA swizzle

Smallest change, biggest instant win.

### What to do
- Add a `format` parameter to `RE_UploadCinematic` (or add `RE_UploadCinematicBGRA` entry point)
- Pass `GL_BGRA` directly to `glTexSubImage2D` from q3ide code
- Delete the BGRA→RGBA swizzle loop in the frame polling path
- Existing video playback continues to pass `GL_RGBA` — no breakage

### Why
Eliminates 64 million per-pixel CPU operations per frame at 31 windows.

### 🧪 Test
- Measure FPS with 31 windows BEFORE change
- Apply change
- Measure FPS with 31 windows AFTER change
- Windows should look identical (no color shift)
- Video playback (/cinematic) should still work

### 📋 Istvan: git commit, verify FPS delta, verify no color issues

---

## STAGE 2 — Visibility-gated texture uploads

Skip UploadCinematic for windows you can't see.

### What to do
- Before calling UploadCinematic for each window in Q3IDE_WM_PollFrames():
    1. **Dot product:** player view direction · (window_pos - player_pos). If < 0 → behind player → skip
    2. **BSP trace:** ray from player eye to window center. If hits anything before window → occluded → skip
- Skipped windows keep their last texture on the GPU (stale but invisible)
- When window becomes visible again, next PollFrames upload refreshes it

### Why
One dot product + one ray trace is dirt cheap compared to an 8MB texture upload. Expected savings: 50-70% of uploads eliminated.

### 🧪 Test
- Face a wall with no windows behind you → FPS should be near baseline
- Spin 180° → windows appear with stale content, then refresh
- `/q3ide_debug_vis` should show skip counts per window

### 📋 Istvan: git commit, verify FPS improvement when facing away from windows

---

## STAGE 3 — Wall scanner + cache

Foundation for the new placement system. No placement changes yet — just scanning.

### What to do
- Create `wall_scanner.h/.c` and `wall_cache.h/.c`
- On area entry (hook into existing area detector), scan all walls within Q3IDE_PLACEMENT_RADIUS (2400u / 60m)
- Filter, deduplicate, qualify walls per the Wall Pre-Scanner spec above
- Pre-compute window slots per wall
- Cache results — persist until next area transition
- See **Wall Pre-Scanner** and **Data structures** sections in Part 1

### 🧪 Test
- Console command `/q3ide_walls` dumps all cached walls with slot counts, positions, normals
- Walk between areas → cache rebuilds (verify via console)
- No placement changes yet — current placement still active

### 📋 Istvan: git commit, verify wall scanning looks sane in different map areas

---

## STAGE 4 — Area transition placement

Replace current placement with cache-based placement. **This is where the old 13 rules get destroyed.**

### What to do
- Rip out all old placement code (keep window entity reuse only)
- Implement area transition mode per the **Placement Architecture** spec above
- Implement spread-even distribution per the **Spread-Even Distribution** spec
- Implement window sizing per the **Window Sizing Algorithm** spec
- Implement collision tracing per the **Collision Tracing** spec
- FPS-gated drain + texture throttle (2fps during placement, 25fps when done)

### 🧪 Test
- Walk between areas → windows migrate smoothly to new walls
- FPS stays above 30 during migration
- Windows spread evenly across walls (not all crammed on one wall)
- Single horizontal row per wall, vertically centered
- No overlap, no clipping into geometry

### 📋 Istvan: git commit, test across multiple map areas, verify visual quality

---

## STAGE 5 — Within-area leapfrog

Windows follow you within large areas.

### What to do
- Implement leapfrog mode per the **Placement Architecture** spec (Mode 2)
- Area persistence: windows STAY PUT while player is in same area. Leapfrog ONLY triggers when player moves >7m AND area is large enough.
- One window per check. No global texture throttle for leapfrog.

### 🧪 Test
- Walk 10m in a large area → furthest window repositions ahead
- Walk 5m → nothing happens (below threshold)
- In a small room → windows never move regardless of player position
- Leapfrog is smooth, no FPS dip

### 📋 Istvan: git commit, test in both large and small areas

---

## STAGE 6 — Trained positions

Dogs remember their spot.

### What to do
- Implement TrainedPosition_t per the **Data structures** spec
- When user manually repositions a window → save position per area, flag trained=true
- On area entry (before automatic placement):
    1. Check for trained positions in this area
    2. Place trained windows at their saved spots FIRST (they get priority)
    3. If a trained spot is occupied → bump the occupier to nearest free slot
    4. Then auto-place remaining untrained windows as normal
- Trained windows are excluded from leapfrog

### 🧪 Test
- Manually move a window → leave area → return → window is back in its trained spot
- Untrained windows auto-place around the trained ones
- Trained window's spot takes priority over auto-placed window

### 📋 Istvan: git commit, test trained position persistence across multiple area transitions

---

## STAGE 7 — Adaptive resolution (8 tiers)

SCK source-side downscale. The biggest bandwidth win.

### What to do
- Calculate distance tier per window per frame (cheap — just distance + dot product)
- When tier changes: call SCStream updateConfiguration with new resolution
- Show stale texture at old resolution until new stream delivers
- Implement aim override and distance override per the **Resolution Tiers** spec
- Set SCK minimumFrameInterval per tier per the **Resolution Tiers** spec

### 🧪 Test
- Walk toward a window → resolution visibly increases
- Walk away → resolution decreases (stale frame shown during transition)
- Aim at distant window with crosshair dead-center → snaps to full res
- Measure FPS improvement vs Stage 6

### 📋 Istvan: git commit, verify visual quality at each tier, measure FPS

---

## STAGE 8 — Static detection + mipmaps

Two small wins, one session.

### Static content detection
- Per window: count dirty frames over rolling 1-second window
- If < Q3IDE_STATIC_THRESHOLD (2) dirty frames in last second → classify as static
- Static windows: capture at Q3IDE_STATIC_CAPTURE_FPS (1fps)
- Resets instantly when content starts changing again

### Mipmap generation
- Call `glGenerateMipmap` after each texture upload
- Distant windows look smooth instead of aliased
- Only regenerate when texture is dirty

### 🧪 Test
- Open idle terminal → verify capture drops to 1fps (check via `/q3ide_debug_vis`)
- Type in terminal → capture immediately returns to normal rate
- Distant windows look smooth (not pixelated/aliased)
- Measure FPS improvement vs Stage 7

### 📋 Istvan: git commit, verify static detection works, check visual quality

---

## STAGE 9 — Texture Array (GL_TEXTURE_2D_ARRAY)

Renderer refactor. Do this LAST (before metrics).

### What to do
- Replace per-window GL textures with GL_TEXTURE_2D_ARRAY
- One texture array per resolution tier (up to 8 arrays)
- Each window gets a layer index within its tier's array
- Shader change: sample from `texture(sampler2DArray, vec3(uv, layerIndex))`
- Instanced draw: all windows in same tier → one bind + one batched draw call
- UploadCinematic now uploads to a specific layer: `glTexSubImage3D(..., layer, ...)`

### Why
31 separate `glBindTexture` → 8 or fewer binds (one per tier). GPU stays in flow state.

### Constraint
All layers in an array must be same resolution → solved by per-tier arrays. When window changes tier → remove from old array, add to new array's layer.

### 🧪 Test
- 31 windows render correctly (no visual changes)
- Measure draw call count before/after (should drop from ~31 to ~8)
- Measure FPS improvement vs Stage 8

### 📋 Istvan: git commit, verify no visual regression, measure draw call reduction

---

## STAGE 10 — Per-window performance metrics

Wire up the Performance Widget for debugging.

### What to track per window
- capture_fps — actual frames received from SCK
- upload_fps — actual texture uploads to GPU
- dirty_ratio — what % of frames were actually dirty
- bandwidth — MB/s for this window's texture data
- latency — ms from SCK callback to texture visible in-game
- vram — MB consumed by this window's texture + mipmaps
- skip_count — frames skipped due to visibility culling or budget
- current_tier — which resolution tier (0-7)
- static_flag — is this window classified as static content

### Display
- Performance Widget (HUD overlay): summary — total windows, total bandwidth, worst offender
- Console `/q3ide_perf`: per-window table with all 9 metrics
- Console `/q3ide_perf <windowId>`: detailed single-window view

### 🧪 Test
- `/q3ide_perf` shows live per-window stats
- Metrics update in real-time as you move around
- Static windows show low capture_fps and dirty_ratio
- Distant windows show lower tiers and smaller bandwidth

### 📋 Istvan: git commit, verify metrics are accurate, use them to find remaining bottlenecks

---

## File Structure

```
spatial/window/
├── placement.h          // Placement system public API
├── placement.c          // Core placement logic (area transition + leapfrog)
├── wall_scanner.h       // Wall pre-scanner
├── wall_scanner.c       // BSP ray casting, wall qualification, slot computation
├── wall_cache.h         // CachedWall_t, WallSlot_t, TrainedPosition_t
├── wall_cache.c         // Cache management (create, query, invalidate, trained positions)
├── placement_queue.h    // FPS-adaptive placement queue
├── placement_queue.c    // Queue drain logic with FPS gate + texture throttle
├── visibility.h         // Dot product + BSP trace visibility checks
├── visibility.c         // Pre-upload visibility gating
├── adaptive_res.h       // Resolution tier calculation + SCK reconfiguration
├── adaptive_res.c       // 8-tier system, aim override, static detection
├── perf_metrics.h       // Per-window performance tracking
├── perf_metrics.c       // Metric collection + console/widget display
```

Each file ≤ 200 lines. Max 400. Split if exceeded.

---

## Performance Baseline

| Metric | Before (current) | After all 10 stages (projected) |
|--------|------------------|-------------------------------|
| FPS (31 windows) | 23 fps | 40+ fps |
| CPU→GPU bandwidth | ~6,400 MB/sec | ~155 MB/sec |
| Texture uploads/frame | 31 (unconditional) | ~5-10 (visible only) |
| Draw calls (windows) | 31 | ~8 (per-tier batched) |
| Placement FPS dip | massive spike | smooth (FPS-gated + 2fps throttle) |

---

## Summary

```
SPEC:
  - Two modes: area transition (all dogs migrate) + leapfrog (furthest jumps forward)
  - Wall pre-scanner with cache (scan once per area, reuse)
  - Spread-even: round-robin across walls, closest first, trained get priority
  - Sizing: 85% wall height, aspect-fit, min/max clamp
  - Collision: trace everything, 4-corner check
  - Edge cases: tiny rooms, death, teleport, area persistence, trained positions

STAGES:
  1. Kill swizzle          → GL_BGRA native, delete CPU loop
  2. Visibility gate       → skip uploads for invisible windows
  3. Wall scanner + cache  → foundation for new placement
  4. Area transition       → destroy old rules, build new placement
  5. Leapfrog              → windows follow player within area
  6. Trained positions     → dogs remember their spot
  7. Adaptive resolution   → 8 tiers, SCK source-side downscale
  8. Static + mipmaps      → idle windows cost nothing
  9. Texture array         → batched draw calls
 10. Performance metrics   → see everything, debug anything

ALWAYS TRUE:
  - 100u minimum, 200u maximum, 85% wall height
  - single horizontal row, vertically centered on wall
  - trace everything for collision
  - never stack, never overlap, never ceiling/floor
  - reuse entities, never recreate
  - FPS-gated at 30fps
  - texture throttle: 2fps during placement, 25fps normal
  - dogs stay put in their area
  - trained dogs remember their spot across visits
```