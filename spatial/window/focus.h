/*
 * spatial/window/focus.h — Crosshair focus and interaction modes for q3ide.
 *
 * The crosshair is the eye-tracking equivalent. This module tracks:
 *   - Which window is under the crosshair
 *   - Dwell time for hover detection
 *   - Interaction mode transitions (FPS, Pointer, Keyboard)
 *   - Ray-window intersection and UV mapping
 *
 * Batch 2.1–2.2: Core focus tracking and dwell.
 * Engine-agnostic, uses only math.h.
 */

#ifndef Q3IDE_SPATIAL_WINDOW_FOCUS_H
#define Q3IDE_SPATIAL_WINDOW_FOCUS_H

#include "entity.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ─── Interaction Modes ───────────────────────────────────────────────────── */

typedef enum {
	MODE_FPS = 0,           /* Normal FPS camera control (default) */
	MODE_POINTER,           /* Pointer mode — mouse cursor on window */
	MODE_KEYBOARD,          /* Keyboard passthrough mode */
} interaction_mode_t;

/* ─── Focus State ─────────────────────────────────────────────────────────── */

typedef struct {
	/* ─── Mode ─── */
	interaction_mode_t       mode;               /* Current interaction mode */

	/* ─── Focus Target ─── */
	int                      focused_win;        /* Index of focused window, -1=none */
	float                    hit_uv[2];          /* UV coords on window [0..1] */
	float                    hit_point[3];       /* World-space hit point */
	float                    hit_dist;           /* Distance to focused window (units) */

	/* ─── Dwell Timer ─── */
	float                    dwell_start_ms;     /* Sys_Milliseconds when dwell began, -1=no dwell */
	float                    hover_progress;     /* [0..1] eased dwell progress */

	/* ─── Mode State ─── */
	int                      pointer_active;     /* 1 = Pointer Mode is active */
	int                      keyboard_active;    /* 1 = Keyboard Passthrough is active */

	/* ─── Pointer Position ─── */
	float                    pointer_uv[2];      /* Current pointer UV in window space */
} focus_state_t;

/* ─── Design Tokens ───────────────────────────────────────────────────────── */

#define Q3IDE_DWELL_MS              150.0f   /* ms to hover before activation */
#define Q3IDE_POINTER_DIST_MAX      600.0f   /* max distance for Pointer Mode */
#define Q3IDE_EDGE_ZONE_PX          20.0f    /* edge zone width in virtual pixels */

/* ─── Initialization ──────────────────────────────────────────────────────── */

/*
 * Initialize focus state to default (no focus, FPS mode, no dwell).
 */
void focus_init(focus_state_t *fs);

/* ─── Focus Update ────────────────────────────────────────────────────────── */

/*
 * Update focus state each frame: ray-cast all windows, track dwell.
 *
 * Args:
 *   fs:      focus state to update
 *   wins:    window array
 *   n:       number of windows
 *   eye:     player eye position [3]
 *   fwd:     normalized view direction [3]
 *   dt_ms:   frame delta time in milliseconds
 *
 * Updates:
 *   focused_win, hit_uv, hit_point, hit_dist
 *   dwell_start_ms and hover_progress
 */
void focus_update(focus_state_t *fs, const window_entity_t *wins, int n,
                  const float *eye, const float *fwd, float dt_ms);

/* ─── Ray-Window Intersection ─────────────────────────────────────────────── */

/*
 * Test ray (eye + t*fwd) against an oriented window.
 *
 * The window is an axis-aligned rectangle in 3D space:
 *   - Plane: (hit - origin) · normal = 0
 *   - Local basis: right = perp to normal in XY, up = (0, 0, 1)
 *   - Rect bounds: |local_x| <= world_w/2, |local_y| <= world_h/2
 *
 * Args:
 *   w:        window entity
 *   eye:      ray origin [3]
 *   fwd:      ray direction [3], must be normalized
 *   out_uv:   UV coordinates [2], output (0..1) if hit
 *   out_dist: distance along ray if hit, output
 *
 * Returns: 1 if hit, 0 if miss.
 */
int focus_ray_hit_window(const window_entity_t *w, const float *eye,
                         const float *fwd, float *out_uv, float *out_dist);

/* ─── Interaction Mode Transitions ────────────────────────────────────────── */

/*
 * Enter Pointer Mode: crosshair → mouse cursor control.
 * Sets mode=MODE_POINTER, pointer_active=1.
 */
void focus_enter_pointer(focus_state_t *fs);

/*
 * Exit Pointer Mode: return to FPS camera control.
 * Sets mode=MODE_FPS, pointer_active=0.
 */
void focus_exit_pointer(focus_state_t *fs);

/*
 * Enter Keyboard Passthrough Mode: keys sent to focused window.
 * Sets mode=MODE_KEYBOARD, keyboard_active=1.
 */
void focus_enter_keyboard(focus_state_t *fs);

/*
 * Exit Keyboard Passthrough Mode: return to FPS control.
 * Sets mode=MODE_FPS, keyboard_active=0.
 */
void focus_exit_keyboard(focus_state_t *fs);

/* ─── Pointer Movement ────────────────────────────────────────────────────── */

/*
 * Move the pointer by dx/dy screen pixels within the focused window.
 *
 * Args:
 *   fs:           focus state
 *   dx, dy:       pixel deltas
 *   window_w_px:  window width in virtual pixels
 *   window_h_px:  window height in virtual pixels
 *
 * Updates: pointer_uv[2]
 * Returns: 1 if pointer stayed in bounds, 0 if at edge zone (exit trigger).
 */
int focus_pointer_move(focus_state_t *fs, float dx, float dy,
                       float window_w_px, float window_h_px);

#ifdef __cplusplus
}
#endif

#endif /* Q3IDE_SPATIAL_WINDOW_FOCUS_H */
