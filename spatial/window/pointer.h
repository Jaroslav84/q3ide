/*
 * spatial/window/pointer.h — Pointer interaction mapping for q3ide.
 *
 * Batch 2.3–2.5: Converting focus UV coordinates to pixel clicks,
 * edge zone detection, and click injection targeting.
 *
 * Engine-agnostic, uses only plain C types.
 */

#ifndef Q3IDE_SPATIAL_WINDOW_POINTER_H
#define Q3IDE_SPATIAL_WINDOW_POINTER_H

#include "focus.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ─── Click Result ────────────────────────────────────────────────────────── */

typedef struct {
	int                      pixel_x;    /* Window pixel coordinate X */
	int                      pixel_y;    /* Window pixel coordinate Y */
	int                      window_id;  /* capture_id to inject into */
} pointer_click_result_t;

/* ─── UV to Pixel Mapping ─────────────────────────────────────────────────── */

/*
 * Map UV coordinates [0..1] to window pixel coordinates.
 *
 * Args:
 *   w:      window entity (uses tex_w, tex_h)
 *   uv:     UV coordinates [2], range [0..1]
 *   px, py: output pixel coordinates
 *
 * Returns: 1 on success, 0 if UV out of bounds.
 */
int pointer_uv_to_pixel(const window_entity_t *w, float uv[2], int *px, int *py);

/* ─── Edge Zone Detection ─────────────────────────────────────────────────── */

/*
 * Test if pointer is in the edge zone of a window.
 *
 * The edge zone is a margin around the window that triggers mode exit
 * if the pointer drifts past it (e.g., Pointer Mode → FPS Mode).
 *
 * Args:
 *   uv:        pointer UV coordinates [2], range [0..1]
 *   edge_px:   edge zone width in pixels (window-relative)
 *   win_w_px:  window width in pixels
 *   win_h_px:  window height in pixels
 *
 * Returns: 1 if in edge zone, 0 if in safe zone.
 */
int pointer_in_edge_zone(float uv[2], float edge_px, float win_w_px, float win_h_px);

/* ─── Click Injection ─────────────────────────────────────────────────────── */

/*
 * Compute click target from focus state.
 *
 * Converts the focused window's UV coordinates and pointer position
 * to pixel coordinates for injection via the capture system.
 *
 * Args:
 *   fs:   focus state (must have valid focused_win and pointer_uv)
 *   wins: window array
 *
 * Returns: click_result with pixel_x, pixel_y, window_id set.
 *          If no focused window, result.window_id = 0 (invalid).
 */
pointer_click_result_t pointer_get_click(const focus_state_t *fs,
                                         const window_entity_t *wins);

#ifdef __cplusplus
}
#endif

#endif /* Q3IDE_SPATIAL_WINDOW_POINTER_H */
