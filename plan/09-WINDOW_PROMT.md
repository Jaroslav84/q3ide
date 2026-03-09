# Q3IDE — Window Placement System

**This document replaces ALL existing placement logic.** The current implementation (rules 1-13 in the codebase) is garbage except for the performance optimizations around window reuse. Rewrite the placement system from scratch using these rules.

---

## ⚠️ LLM Implementation Instructions

### What to KEEP
- Window entity reuse — windows are moved (x,y,z,angle update), never destroyed and recreated. The SCStream tunnel stays alive. This is the single best optimization in the current code. Do not touch it.
- The placement queue concept — staggered placement to avoid frame spikes. But the throttle changes (see below).

### What to DESTROY
- All 13 current placement rules. Rip them out.
- The wall-facing dot product sort as the primary placement strategy.
- The 30m radius hardcode.
- The 16u step width measurement.
- The 32° deduplication.
- The geometry clamp margin approach.
- The 1-per-frame drain rate.
- Everything in the placement code except the window entity reuse and texture pipeline.

### What to BUILD (from this document)
- New constant definitions (see Constants below) — put these in `q3ide_params.h`
- Wall pre-scanner with area-based caching
- Two-mode placement: area transition + within-area leapfrog
- FPS-adaptive throttle
- Spread-even distribution across walls
- Single horizontal row, vertically centered on wall
- Full collision tracing (BSP, entities, models, everything)

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
#define Q3IDE_PLACEMENT_RADIUS       2400    // 60m scan radius (was 1200u/30m, doubled)

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

These are always true, regardless of placement mode.

1. **Minimum size = 100u diagonal.** Windows never get smaller than a 100" TV equivalent. Exception: user manually resizes.
2. **Aspect ratio is sacred.** Windows always maintain the captured window's aspect ratio. Never stretched, never cropped.
3. **Double-sided display.** Windows render from behind too (image flipped horizontally) so they're useful from both sides. Exception: window is flush against a wall with no back visibility.
4. **Single horizontal row per wall.** Windows are never stacked vertically. One row, evenly spaced.
5. **Vertically centered on wall.** Windows sit at the vertical center of the wall. Like a projector screen — middle of the wall, period.
6. **Vertical walls only.** Only surfaces within ±5° of vertical qualify. Exception: in-map monitors/billboards up to ±30°.
7. **Never on ceilings or floors.**
8. **Never intersect each other.** If two windows would overlap, the later one must find a different slot or wall. Exception: user manually moves a window.
9. **Never placed outside the player's area/room.** Windows stay in the area the player can see.
10. **3u wall offset.** Windows float 3u off the wall surface to avoid z-fighting.
11. **85% wall height.** Windows are sized to 85% of wall height (aspect-fit). Exceptions: result < min_window_size or > max_window_size, or wall > max_wall_height.
12. **Trace everything for collision.** BSP geometry, entity models, item pickups, weapon spawns, other players — trace against ALL of it when determining window placement and sizing. No shortcuts.

---

## Placement Architecture

### Two Modes

The placement system operates in two modes that switch automatically:

#### Mode 1: Area Transition

Triggered when: the area detector reports the player entered a new area.

```
Area transition detected
  → Throttle ALL window texture updates to Q3IDE_TEXTURE_FPS_PLACING (2fps)
  → Pre-scan ALL walls in new area (expensive, once)
  → Build wall_cache[] (position, normal, width, height, pre-computed slots)
  → Sort walls by distance to player (closest first)
  → Queue ALL windows for migration
  → Drain queue with FPS-adaptive throttle:
      → If last_frame_fps > Q3IDE_PLACEMENT_FPS_GATE (30fps):
          → Pick closest wall with a free slot
          → Move window entity to slot (update x,y,z,angle only)
      → If last_frame_fps ≤ 30: skip this frame, try next frame
  → When queue is empty:
      → Restore ALL windows to Q3IDE_TEXTURE_FPS_NORMAL (25fps)
```

Visual metaphor: 10 dogs follow you through a doorway. Each dog finds a spot and sits down, one at a time. Closest spots fill first. If the room is lagging, dogs wait patiently until FPS recovers. All TVs freeze while dogs are moving — unfreezes when everyone's settled.

#### Mode 2: Within-Area Leapfrog

Triggered when: player moves > Q3IDE_LEAPFROG_DISTANCE (7m) from last placement check position AND player is NOT in area transition.

Checked every Q3IDE_LEAPFROG_CHECK_INTERVAL frames (not every frame).

```
Player moved >7m from last check position
  → Find the window furthest from player
  → Find the closest wall to player with a free slot (from cache)
  → If furthest_window_distance > closest_free_slot_distance:
      → Move window to new slot (update x,y,z,angle only)
  → Update last check position
```

The furthest dog gets up and runs to the closest open spot near you. One dog per check. Natural, smooth, no frame spike.

---

## Wall Pre-Scanner

Runs once on area entry. Results cached until next area transition.

### What it does
1. Cast rays outward from player position in a sphere pattern (or use BSP area data)
2. Find all surfaces within Q3IDE_PLACEMENT_RADIUS (60m) that are in the current area
3. Filter: only surfaces within ±Q3IDE_WALL_MAX_ANGLE (5°) of vertical
4. Filter: height ≥ Q3IDE_MIN_WALL_HEIGHT (79u), width ≥ Q3IDE_MIN_WALL_WIDTH (114u)
5. Deduplicate: merge surfaces that are part of the same physical wall
6. For each qualifying wall, pre-compute window slots:
   - Number of slots = floor((wall_width - 2 × margin) / (window_width + gap))
   - Slot positions = evenly spaced along wall, centered
   - Each slot: {position, normal, width, height, occupied flag}

### Wall cache structure
```c
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
    vec3_t      position;        // slot center in world space
    vec3_t      normal;          // facing direction
    float       width;           // available width for this slot
    float       height;          // available height for this slot
    int         windowId;        // -1 if free, window entity ID if occupied
} WallSlot_t;
```

### Spread-even distribution
When placing N windows across M walls, distribute evenly:
1. Sort walls by distance (closest first)
2. Round-robin: assign 1 slot per wall, cycling through walls
3. Continue until all windows are placed or all slots are full
4. If more windows than slots: remaining windows stay in queue (will leapfrog later as player moves)

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

## Edge Cases

### More windows than wall space
Windows stay queued. As the player moves (leapfrog mode), new walls become available and queued windows get placed.

### Tiny rooms with no qualifying walls
Windows float unplaced. They'll attach when the player moves to an area with walls. No error, no crash, just waiting dogs.

### Player teleports (Shift+1-8)
Same as area transition. Pre-scan new area, queue all windows for migration. Trained windows go to their remembered spots.

### Player dies
Windows stay exactly where they are. They don't move on death. Player respawns and walks back to them (or they leapfrog to the new spawn area).

### Windows stay put within an area
Once windows are placed inside an area, they DO NOT reposition while the player stays in that area. No unnecessary shuffling. Dogs sit, dogs stay.
- Exception: area is bigger than Q3IDE_PLACEMENT_RADIUS (60m) — leapfrog rules apply within the 60m bubble only.
- Exception: user manually moves a window.

### Window manually repositioned by user (trained positions)
That window is flagged as `manually_placed = true` with its position saved **per area**. It is excluded from leapfrog and automatic area transition placement.

**Trained position memory:** When the user returns to this area later, the window goes back to its trained spot automatically. We teach our dogs where their place is.

```c
typedef struct {
    int         windowId;        // which window
    int         areaId;          // which area this position belongs to
    vec3_t      position;        // saved world position
    vec3_t      normal;          // saved facing direction
    qboolean    trained;         // true = user placed it here manually
} TrainedPosition_t;
```

- Untrained windows (never manually moved) use automatic placement as normal.
- If a trained window's saved wall slot is occupied by another window → trained window gets priority, the other window gets bumped to nearest free slot.
- Trained positions persist across area transitions. The wall_cache stores them.

---

## Performance Rules

**Current baseline:** 0 windows = 40-50fps. 31 windows = 23fps. Each window ~0.5-0.7fps cost (texture uploads at 25fps/window).

1. **Never recreate window entities.** Move them. The SCStream tunnel, texture, and render state persist.
2. **FPS-gated placement.** Only place/move a window if last frame was >30fps.
3. **Texture throttle during placement.** While placement queue is draining, ALL windows throttle to 2fps texture updates. 31 windows × 25fps = crushing. 31 windows × 2fps = breathing room. Restore to 25fps when queue is empty.
4. **Pre-scan once per area.** Wall cache is built on area entry, not per-placement.
5. **Leapfrog checks every 30 frames**, not every frame. ~2 checks/second at 60fps. No global texture throttle for leapfrog — single window moves are cheap.
6. **Queue cap = 32.** If more than 32 windows are queued, excess stays in a waiting list until slots free up.
7. **No animation on placement.** Just update position/angle. Cheapest possible operation.
8. **Distance-based render priority is separate.** The placement system doesn't care about texture resolution. That's the renderer's job (see IDE_VISION.md Performance & Rendering section).

---

## File Structure

```
spatial/window/
├── placement.h          // Placement system public API
├── placement.c          // Core placement logic (area transition + leapfrog)
├── wall_scanner.h       // Wall pre-scanner and caching
├── wall_scanner.c       // BSP ray casting, wall qualification, slot computation
├── wall_cache.h         // CachedWall_t, WallSlot_t data structures
├── wall_cache.c         // Cache management (create, query, invalidate)
├── placement_queue.h    // FPS-adaptive placement queue
├── placement_queue.c    // Queue drain logic with FPS gate
```

Each file ≤ 200 lines. Max 400. Split if exceeded.

---

## Summary

```
AREA TRANSITION:
  area_detector fires → wall_scanner pre-scans → wall_cache built
  → all windows queued → drain 1/frame if FPS>30 → spread evenly
  → closest walls first → single row, centered vertically

WITHIN-AREA LEAPFROG:
  every 30 frames: player moved >7m?
  → find furthest window → find closest free slot
  → move window (x,y,z,angle) → done

ALWAYS TRUE:
  - 100u minimum, 200u maximum, 85% wall height
  - single horizontal row, vertically centered on wall
  - trace everything for collision
  - never stack, never overlap, never ceiling/floor
  - reuse entities, never recreate
  - FPS-gated at 30fps
  - texture throttle: 2fps during placement, 25fps normal
```