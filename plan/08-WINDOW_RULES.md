# Window constraints & rules

These rules are design decisions that must be always true for tunneled windows inside the game

## Constants

Ofcourse they used inches...

- ideal_window_size = 100u inch diagonally where windows "aspect fit" this
- max_window_size = 200u inch (exception: billboards and theater mode, huge walls)
- min_window_size = 66u inch
- window_wall_ratio = 85% of the wall height (with exceptions)
- window_placement_radius = 2400u (Quake units = inch = 60m)
- max_wall_height = 394u (10m) walls
- min_wall_height = 79u (2m)
- min_wall_width = min_window_width + 2 × margin(24u)
- wall_margin = 24u each side
- window_gap = 16u between windows on same wall
- window_wall_offset = 3u gap between window and wall (less clipping)
- window_wall_max_angle = 5 degree +/-
- window_wall_big_max_angle = 30 degree +/-
- placement_fps_gate = 30fps (only place when above this)
- placement_queue_cap = 32
- leapfrog_distance = 280u (7m player movement before leapfrog triggers)
- leapfrog_check_interval = 30 frames (~0.5s at 60fps)
- max_windows = 64
- max_cached_walls = 128
- max_wall_slots = 8 per wall
- window_texture_fps_normal = 25fps (max texture update rate per window)
- window_texture_fps_placing = 2fps (throttled rate while dogs are sitting down)
- static_threshold = 2 dirty frames (less than this in 1 second → classified static)
- static_capture_fps = 1fps (capture rate when window is static)
- aim_full_res_distance = 120u (3m, within this → always full res)
- aim_full_res_dot = 0.95 (dot product threshold for aiming directly at window)
- res_tier_count = 8
- res_tier_distances = [120, 240, 480, 720, 960, 1200, 1800, max]
- res_tier_scales = [1.0, 0.75, 0.5, 0.375, 0.25, 0.1875, 0.125, 0.0625]

## General window constraints

- Windows never get smaller than ideal_window_size (100u). Like a 100" TV.
    - exceptions:
        - user manually resizes it
- Windows always keep the same Aspect Ratio
- Windows can display picture and have same interactions from behind too with the image flipped horizontally
    - exceptions:
        - window is 100% on the wall and none can see it from behind
    - **implementation:** single polygon with `cull disable` shader — GPU renders both faces for free.
      Back-face winding reversal automatically flips UVs horizontally (correct physical behaviour).
      Never submit a second explicit back-face quad — redundant and wasteful.
- Single horizontal row per wall. Never stacked vertically.
- Vertically centered on wall. Middle. Like a projector screen.
- All windows are equal priority. FIFO order. No VIP treatment.
- Window moves = just update x,y,z,angle. Never destroy/recreate the entity or SCStream tunnel.

## Automatic window placement on walls

### Two modes

#### Mode 1: Area transition
- Player enters a new area (area detector fires)
- Pre-scan ALL walls in the area within window_placement_radius (2400u / 60m). Expensive, once. Cache the results.
- Pre-compute window slots on each wall (position, size, occupancy)
- Queue ALL windows for migration to new area (queue cap: placement_queue_cap = 32)
- Drain queue with FPS-adaptive throttle: only place 1 window per frame IF last frame was above placement_fps_gate (30fps). If FPS is below 30 → skip, wait for recovery.
- Fill order: closest walls to player first. Spread windows evenly across walls (round-robin: 1 per wall, then cycle back for seconds).
- While queue is draining (dogs still sitting): ALL window texture updates throttle down to window_texture_fps_placing (2fps). Frees GPU bandwidth for placement. Windows look frozen but placement is silky smooth.
- When queue is empty (all dogs seated): texture updates restore to window_texture_fps_normal (25fps). TVs come alive again.
- Visual: 10 dogs follow you through a doorway, each finding a spot and sitting down one at a time. Closest spots first. If lagging, dogs wait. All TVs freeze while dogs are moving, unfreeze when everyone's settled.

#### Mode 2: Within-area leapfrog
- Checked every leapfrog_check_interval (30 frames, not every frame)
- Triggers when player moves > leapfrog_distance (280u / 7m) from last check position
- Find the furthest window from player
- Find the closest wall with a free slot (from cache)
- If furthest window is further than closest free slot → move it
- One window per check. The furthest dog gets up and runs to the nearest open spot.

### Wall scanning & caching
- Pre-scan runs once per area entry. Results cached until next area transition.
- Wall qualification: height ≥ min_wall_height (79u), usable width ≥ min_wall_width. Walls below these are ignored.
- Only vertical walls: within ±window_wall_max_angle (5°) of vertical
    - exceptions:
        - in-map monitors, billboards: up to ±window_wall_big_max_angle (30°). In that case max_window_size is not enforced.
- Slots pre-computed per wall: count = floor((width - 2 × wall_margin) / (window_width + window_gap)), capped at max_wall_slots (8)

### Placement rules
- Windows are placed within window_placement_radius (2400u / 60m)
- Windows are 'reused/moved' instead of recreating the tunnel pipe. Way more efficient.
- Placement traces against EVERYTHING for collision: BSP geometry, entity models, item pickups, weapon spawns, other windows, players. No shortcuts.
- Windows never ever intersect each other. If detected → window must find a better slot or wall
    - exceptions:
        - user manually moves window
- Windows are never placed on ceilings/floors
- Windows are never placed outside of the room/area where user can't see it
- Windows are sized max window_wall_ratio (85%) of the wall height (aspectFit mode resize)
    - exceptions:
        - window_size < min_window_size (66u) OR window_size > max_window_size (200u)
        - wall is taller than max_wall_height (394u)
- Windows have window_wall_offset (3u) offset away from wall surface to avoid clipping into wall objects
- Windows like to be as much as possible on the walls, only vertical walls are candidates within window_wall_max_angle (±5°)
    - exceptions:
        - big enough monitors, billboards inside the game. In that case max_window_size is not enforced.
        - big but not vertical walls, up to window_wall_big_max_angle (±30°) are accepted. In that case max_window_size is not enforced.
- Windows always know which direction to face based on map data
- Placement should not break any existing features (highlighting, interacting, resize, reposition, etc)
- Once windows are placed inside an area, they STAY where they are. No repositioning while the user is in the same area.
    - exceptions:
        - area is bigger than window_placement_radius (2400u / 60m) → leapfrog rules apply within the 60m bubble only
        - user manually moves a window
- Windows remember their trained position per area. If user repositioned a window in an area, that position is saved. When user returns to that area later, that window goes back to its trained spot. We teach our dogs where their place is.
    - trained positions stored in wall_cache per area (persist across area transitions)
    - untrained windows (never manually moved) use automatic placement as normal
    - if the trained wall slot is occupied by another window → trained window gets priority, bump the other

### Edge cases
- More windows than wall slots → extras stay queued, placed on leapfrog as new walls become available
- Tiny room with no qualifying walls → windows float unplaced, attach when player moves to a better area
- Player teleports (Shift+1-8) → same as area transition, pre-scan new area
- Player dies → windows stay exactly where they are
- Window manually repositioned by user → flagged manually_placed, position saved per area. Excluded from leapfrog. On return to this area, window goes back to its trained spot.
- Trained position conflicts → if two manually-placed windows want the same spot (shouldn't happen, but edge case) → first one placed wins, second finds nearest free slot

### Performance baseline (current measurements)
- 0 windows in game → 40-50 fps
- 31 windows in game → 23 fps
- Each window costs roughly 0.5-0.7 fps (texture upload cost)
- window_texture_fps_normal = 25fps per window. This is the main GPU cost.
- During placement (queue draining): throttle ALL windows to window_texture_fps_placing (2fps). This turns 31 × 25fps texture uploads into 31 × 2fps = massive GPU headroom for placement work.
- During leapfrog (single window move): no global throttle needed. One window repositioning is cheap.

## Rendering optimizations

### WIN #0: Cull-disable single-quad trick (IMPLEMENTED ✅)

**Problem:** Early implementation submitted TWO quads per window (front + explicit back face = 2× vertex data, 2× draw calls per window). Border strips had the same problem — 8 strips instead of 4.

**Fix:** All `q3ide/win*` and `q3ide/win63` (border) shaders declare `cull disable`. A single polygon is rendered from both sides by the GPU at zero extra cost:
- Front face: winding CCW → UVs read left-to-right → correct image
- Back face: same vertices, winding appears CW from that camera → UVs mirror horizontally → correct physical behaviour (like looking at a screen from behind)

**Result:** 50% fewer quads per window, 50% fewer border strips. No texture duplication, no second scratch slot, zero extra memory. The GPU's backface-culling disable is essentially free.

**Code:** `q3ide_geometry.c` — `q3ide_add_poly()` and `q3ide_frame_strips()`.

**Rule:** Never add a second back-face quad. The shader does it.

---

### Current bottleneck (the murder weapon)
- Capture: CMSampleBuffer → CVPixelBuffer.lock_read_only() → raw CPU pointer
- Per-frame BGRA→RGBA pixel swizzle loop (CPU) into staging buffer
- Upload: RE_UploadCinematic() → glTexSubImage2D → full CPU→GPU copy every frame
- ALL windows upload unconditionally, regardless of visibility or distance
- ALL windows upload at full source resolution (1920×1080 = ~8MB per upload)
- 31 windows × 8MB × 25fps = ~6.4 GB/sec of CPU→GPU bandwidth. Insane.

### WIN #1: Kill the BGRA→RGBA swizzle
- Add a format parameter to RE_UploadCinematic (or add RE_UploadCinematicBGRA entry point)
- Pass GL_BGRA directly to glTexSubImage2D. GPU handles the format natively.
- Eliminates 64 million per-pixel CPU operations per frame at 31 windows
- One-line change in the renderer, format flag in the API

### WIN #2: Visibility-gated texture uploads
- Before calling UploadCinematic for each window, run two checks:
    - Dot product: is the window behind the player? (>90° from view direction) → skip
    - BSP trace: cast ray from player eye to window center. If it hits anything → skip
- Skipped windows keep their last-uploaded texture on the GPU (stale but invisible, who cares)
- When window becomes visible again, next upload refreshes it instantly
- Cost: 1 dot product + 1 ray trace per window per frame. Dirt cheap vs 8MB upload.
- Expected savings: 50-70% of uploads eliminated depending on map layout

### WIN #3: Adaptive resolution (8 tiers)
- SCK captures at the resolution we tell it to. Source-side downscale = zero CPU cost.
- 1 SCStream per window. Resolution changed dynamically when tier changes.
- During reconfiguration: show stale texture at old resolution until new stream delivers. No pop, no stall.
- Full res override: if player is aiming directly at a window (tight dot product) OR within aim_full_res_distance (10 feet / 120u) → full resolution regardless of distance tier. Sniper zoom on a terminal across the map.

#### Resolution tiers

| Tier | Distance | Scale | Resolution (1080p source) | Upload size |
|------|----------|-------|--------------------------|-------------|
| 0 | 0-120u (0-3m) | 1.0 | 1920×1080 | ~8.3 MB |
| 1 | 120-240u (3-6m) | 0.75 | 1440×810 | ~4.7 MB |
| 2 | 240-480u (6-12m) | 0.5 | 960×540 | ~2.1 MB |
| 3 | 480-720u (12-18m) | 0.375 | 720×405 | ~1.2 MB |
| 4 | 720-960u (18-24m) | 0.25 | 480×270 | ~0.5 MB |
| 5 | 960-1200u (24-30m) | 0.1875 | 360×202 | ~0.3 MB |
| 6 | 1200-1800u (30-45m) | 0.125 | 240×135 | ~0.13 MB |
| 7 | >1800u (>45m) | 0.0625 | 120×67 | ~0.03 MB |

- aim_full_res_distance = 120u (10 feet / 3m). Within this distance → always tier 0.
- Aiming directly at any window (dot product > 0.95) → tier 0 regardless of distance.

#### Resolution tier constants
- res_tier_count = 8
- res_tier_distances = [120, 240, 480, 720, 960, 1200, 1800, max]
- res_tier_scales = [1.0, 0.75, 0.5, 0.375, 0.25, 0.1875, 0.125, 0.0625]
- aim_full_res_distance = 120u (3m)
- aim_full_res_dot = 0.95 (tight cone, almost dead-center aim)

#### Projected savings with all 3 wins
- 31 windows, average tier 4 (most windows far away): 31 × 0.5MB × 25fps = ~388 MB/sec
- Plus visibility culling skipping ~60%: effective ~155 MB/sec
- vs current: 6,400 MB/sec
- **~40x reduction in CPU→GPU bandwidth**

### WIN #4: Static content detection
- Count dirty frames over a rolling window per window
- If <2 dirty frames in last second → classify as "static" → drop to 1fps capture
- A terminal sitting idle with a blinking cursor shouldn't cost anything
- Stacks on top of adaptive resolution — static window at tier 5 costs almost nothing
- Detection resets instantly when content starts changing again

### WIN #5: SCK minimumFrameInterval per tier
- We change SCK resolution per tier. We should ALSO set the frame interval at the source.
- Tier 0 (close): minimumFrameInterval = 1/25 (25fps)
- Tier 4 (medium-far): minimumFrameInterval = 1/10 (10fps)
- Tier 7 (thumbnail): minimumFrameInterval = 1/2 (2fps)
- SCK never even generates frames faster than needed. Saves capture-side GPU/CPU work.
- Use stream.updateConfiguration() for on-the-fly changes without restarting the stream.

### WIN #6: Mipmap generation
- Call glGenerateMipmap after each texture upload
- Distant windows look smooth instead of aliased — visually better
- Almost free on modern GPUs. Only regenerate when texture is dirty.

### WIN #7: Texture Array (GL_TEXTURE_2D_ARRAY) — LAST STEP
- Current: 31 separate textures = 31 glBindTexture calls = 31 GPU pipeline stalls per frame
- Texture array: one single texture object with N layers. Bind once, draw all windows in one batched draw call. Shader picks layer by index.
- 31 bind+draw → 1 bind+draw. GPU stays in flow state.
- Constraint: all layers in an array must be same resolution. Solution: one array per resolution tier (8 arrays max, 8 binds — still way better than 31).
- This is a renderer refactor — shader changes, instanced rendering, UV remapping. Do it LAST after all other wins are proven.
- Implementation order: WIN #1 → #2 → #3 → #4 → #5 → #6 → #7

## Per-window performance metrics

Exposed via Performance Widget and console. Essential for debugging.

Per window, track:
- capture_fps — actual frames received from SCK
- upload_fps — actual texture uploads to GPU
- dirty_ratio — what % of frames were actually dirty
- bandwidth — MB/s for this window's texture data
- latency — ms from SCK callback to texture visible in-game
- vram — MB consumed by this window's texture + mipmaps
- skip_count — frames skipped due to visibility culling or budget
- current_tier — which resolution tier (0-7) this window is at
- static_flag — is this window classified as static content

NOTE: we are still experimenting, developing. Don't come up with hard_caps and efficiency increasing tweaks. The time is not there yet.