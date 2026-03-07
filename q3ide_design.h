/*
 * q3ide_design.h — Spatial design tokens derived from Apple VisionOS HIG.
 *
 * These constants define the visual language of q3ide.
 * All values use VisionOS terminology:
 *   - Window (not panel)
 *   - Ornament (not toolbar)
 *   - Glass Material (not transparent background)
 *   - Hover Effect (not highlight)
 *
 * MVP uses only a subset (marked with [MVP]).
 * Post-MVP tokens are included for reference but not yet used.
 */

#ifndef Q3IDE_DESIGN_H
#define Q3IDE_DESIGN_H

/* ─── Glass Material ──────────────────────────────────────────────────── */

#define Q3IDE_GLASS_ALPHA           0.72f
#define Q3IDE_GLASS_TINT_R          0.95f
#define Q3IDE_GLASS_TINT_G          0.95f
#define Q3IDE_GLASS_TINT_B          0.97f
#define Q3IDE_GLASS_BLUR_PASSES     3
#define Q3IDE_GLASS_SPECULAR        0.04f

/* ─── Window Geometry [MVP subset] ────────────────────────────────────── */

#define Q3IDE_CORNER_RADIUS         12.0f    /* Post-MVP: rounded corners */
#define Q3IDE_WINDOW_BAR_HEIGHT     32.0f    /* Post-MVP: Window bar */
#define Q3IDE_WINDOW_BAR_OVERLAP    20.0f    /* Post-MVP: bar overlap */

/* [MVP] Default Window size in game units */
#define Q3IDE_WINDOW_DEFAULT_WIDTH  256.0f
#define Q3IDE_WINDOW_DEFAULT_HEIGHT 192.0f
#define Q3IDE_WINDOW_MIN_WIDTH      64.0f
#define Q3IDE_WINDOW_MAX_WIDTH      512.0f

/* [MVP] Wall offset — how far the quad floats from the wall surface */
#define Q3IDE_WALL_OFFSET           2.0f

/* ─── Ornaments (Post-MVP) ────────────────────────────────────────────── */

#define Q3IDE_ORNAMENT_Z_OFFSET     8.0f
#define Q3IDE_ORNAMENT_OVERLAP      20.0f

/* ─── Hover / Focus (Post-MVP) ────────────────────────────────────────── */

#define Q3IDE_HOVER_GLOW            1.15f
#define Q3IDE_HOVER_LIFT_Z          4.0f
#define Q3IDE_HOVER_FADE_IN_SEC     0.15f
#define Q3IDE_MIN_TAP_TARGET        60.0f

/* ─── Animation (Post-MVP) ────────────────────────────────────────────── */

#define Q3IDE_ANIM_SPAWN_SEC        0.35f
#define Q3IDE_ANIM_CLOSE_SEC        0.25f
#define Q3IDE_ANIM_GRAB_SCALE       1.02f
#define Q3IDE_ANIM_LERP_SPEED       8.0f

/* ─── Status Tints (Post-MVP) ─────────────────────────────────────────── */

#define Q3IDE_STATUS_PASS_R         0.2f
#define Q3IDE_STATUS_PASS_G         0.9f
#define Q3IDE_STATUS_PASS_B         0.4f
#define Q3IDE_STATUS_PASS_A         0.15f

#define Q3IDE_STATUS_FAIL_R         1.0f
#define Q3IDE_STATUS_FAIL_G         0.3f
#define Q3IDE_STATUS_FAIL_B         0.3f
#define Q3IDE_STATUS_FAIL_A         0.15f

#define Q3IDE_STATUS_BUILD_R        1.0f
#define Q3IDE_STATUS_BUILD_G        0.8f
#define Q3IDE_STATUS_BUILD_B        0.2f
#define Q3IDE_STATUS_BUILD_A        0.15f

/* ─── Capture defaults [MVP] ──────────────────────────────────────────── */

#define Q3IDE_CAPTURE_TARGET_FPS    60
#define Q3IDE_CAPTURE_RING_BUF_SIZE 3

#endif /* Q3IDE_DESIGN_H */
