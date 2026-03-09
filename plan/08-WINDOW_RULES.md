# Window constraints & rules

These rules are design decisions that must be always true for tunneled windows inside the game

## Constants

Ofcourse they used inches...

- ideal_window_size = 100u inch diagonally where windows "aspect fit" this 
- max_window_size = 200u inch (exception: billboards and theator mode, huge walls)
- window_wall_ratio = 85% percent of the wall height (with exceptions)
- window_placement_radius = 1200u (Quake units = inch = 30m)
- max_wall_height = 394u (10m) walls
- min_wall_height = 79u (2m)
- min_window_size = 66u inch
- min_wall_width = min_window_width + 2 × margin(24u)
- window_wall_offset = 3u gap between window and wall (less clipping)
- window_wall_max_angle = 5 dregree + / -
- window_wall_big_max_angle = 30 dregree + / -
- window_placement_queue_drain = 1 window at a time
- window_placement_queue_cap = 32

## General window constraints
- Windows never get smaller then a 100" TV 
    - exceptions:
        - user manually resizes it
- Windows always keep the same Aspect Ratio
- Windows can display picture and have same interactions from behind too with the image flipped horizontally
    - exceptions:
        - window is 100% on the wall and none can see it from behind

# Automatic window placement on walls
- Windows are placed in 1200u (30m) radius
- while player is moving in the game "wall sort algo" determines which wall gets first pick. 
    - Most face-on wall gets windows assigned first. 
    - Then comes walls on the side. This single sort controls which wall becomes your "main" screen. No other heuristics. One rule.
- Placement queue drains 1 window per frame (window_placement_queue_drain). Attach is staggered — avoids a single-frame spike when many windows are placed at once. Queue cap: window_placement_queue_cap
- Window placement should not break any existing features (highlighting windows, interacting, resize, reposition, etc)
- Wall qualification: height ≥ min_wall_height, usable width ≥ min_wall_width. Walls below these thresholds are ignored entirely for placement.
- Windows are 'reused/replaced' instead of recreating the whole tunneling pipe. It's way effecient.
- Windows placement happens according to map data and objects on the wall to avoid clipping
- Windows never ever intersect each other. If detected -> window must find a better place
- Windows are no placed vertically on top of each other stacked.
- Windows are never placed on ceilings/floors
- Windows tend to be placed horizontally the same ground level for easy readibility
- Windows are sized max 85% (window_wall_ratio) of the wall height (aspectFit mode resize)
    - exceptions:
        - window_size < min_window_size OR window_size > max_window_size
        - wall is higher than max_wall_size
- Windows on wall have a 3 inch offset (window_wall_offset) away from the wall towards the area (so that it avoids clipping into wall objects)
- Windows like to be as much as possible on the walls, only vertical walls are candidates window_max_angle= +/- 5 degree threshold
    - exceptions:
        - big enough minitors, billboards inside the game. In that case window_max_size is not valid.
        - big but not vertical walls, max 'window_wall_big_max_angle' degree are exections. In that case window_max_size is not valid
- Windows always know which direction to face based on map data
- Windows are never placed outside of the room/area where user can't see it
- Windows are never palced overlapping each other
    - exceptions:
        - user manually moves window

NOTE: we are still experimenting, developing. Don't come up with hard_caps and efficiecny increasing tweaks. The time is not there yet.



#  Full current rule list (after all removals today):

1. Max windows: 64 — How many windows can exist in the scene simultaneously. Raised from 16; you can attach up to
  64 windows before the engine runs out of texture slots.
2. Wall facing sort — Walls are ranked by how directly they face the player; the wall you're looking at gets
  windows assigned first. Pure dot product, no other factors.
3. TV height = 85% of wall height (min 64u) — Each window is sized to 85% of the wall's floor-to-ceiling height, so
   there's a small gap above and below. If the wall is very short, 64u (~1.6m) is the floor.
4. Wall margins 24u each side, 16u gap between windows — Windows never touch the wall edges; 24u (~60cm) is
  reserved on left and right. Multiple windows on the same wall are separated by 16u (~40cm) gaps.
5. Wall qualification: height ≥ 96u, width ≥ 32u — Walls shorter than ~2.4m or narrower than ~80cm are ignored
  entirely. Too small to meaningfully display anything.
6. Wall deduplication: normals within ~32° — If two raycasts hit walls facing nearly the same direction, only one
  wall is kept. Prevents the same physical wall from being counted multiple times.
7. Wall normal Z ≤ 0.4 — Surfaces angled more than ~24° from vertical (floors, ceilings, ramps) are rejected as
  walls. Only roughly-vertical surfaces get windows.
8. Back-trace gap check — During width measurement, a short probe is cast back into the wall; if it hits nothing,
  there's a gap or doorway and width sweep stops there. Prevents windows from spanning across open doorways.
9. Width measurement: 16u steps, 512u max, clamped 64u–2048u — The engine sweeps sideways in 16u increments to find
   where the wall ends, up to 512u (~13m) away. Result is clamped so no wall is reported narrower than 64u or wider
  than 2048u.
10. Placement radius: 1200u (~30m) — Only walls within 30m of the player at the moment attach all is run are
  scanned. Configurable via q3ide_placement_radius cvar.
11. Window wall offset: 3u — Windows float 3u (~7.5cm) off the wall surface to avoid z-fighting/clipping artifacts.
   Was inconsistent (2u vs 4u) across files; now unified.
12. Geometry clamp margin: 1.5u, min 32u×32u — After placement, corner-probing shrinks any window whose edges poke
  into adjacent geometry, keeping a 1.5u clearance. No window can shrink below 32u×32u through this process.
13. Placement queue: 1 window per frame, max 32 queued — Windows are attached one per game frame instead of all at
  once to avoid a single-frame performance spike. If more than 32 are pending, extras are dropped.
