/*
 * spatial/window/pointer.c — Pointer interaction mapping implementation.
 *
 * Pure math, no engine dependencies.
 */

#include "pointer.h"
#include <string.h>

/* ─── UV to Pixel Mapping ─────────────────────────────────────────────────── */

int pointer_uv_to_pixel(const window_entity_t *w, float uv[2], int *px, int *py) {
	if (!w || !uv || !px || !py) {
		return 0;
	}

	/* Check UV bounds */
	if (uv[0] < 0.0f || uv[0] > 1.0f || uv[1] < 0.0f || uv[1] > 1.0f) {
		return 0;
	}

	/* Map UV to pixel coordinates */
	*px = (int)(uv[0] * w->tex_w);
	*py = (int)(uv[1] * w->tex_h);

	/* Clamp to valid range */
	if (*px < 0) *px = 0;
	if (*px >= w->tex_w) *px = w->tex_w - 1;
	if (*py < 0) *py = 0;
	if (*py >= w->tex_h) *py = w->tex_h - 1;

	return 1;
}

/* ─── Edge Zone Detection ─────────────────────────────────────────────────── */

int pointer_in_edge_zone(float uv[2], float edge_px, float win_w_px, float win_h_px) {
	if (!uv || win_w_px <= 0.0f || win_h_px <= 0.0f) {
		return 0;
	}

	/* Convert edge_px to UV units */
	float edge_u = edge_px / win_w_px;
	float edge_v = edge_px / win_h_px;

	/* Check if UV is in any edge zone */
	int at_left = uv[0] < edge_u;
	int at_right = uv[0] > (1.0f - edge_u);
	int at_top = uv[1] < edge_v;
	int at_bottom = uv[1] > (1.0f - edge_v);

	return (at_left || at_right || at_top || at_bottom);
}

/* ─── Click Injection ─────────────────────────────────────────────────────── */

pointer_click_result_t pointer_get_click(const focus_state_t *fs,
                                         const window_entity_t *wins) {
	pointer_click_result_t result;
	memset(&result, 0, sizeof(result));

	/* No focused window */
	if (!fs || fs->focused_win < 0 || fs->focused_win >= Q3IDE_MAX_WINDOWS) {
		return result;
	}

	const window_entity_t *w = &wins[fs->focused_win];
	if (!w->active) {
		return result;
	}

	/* Map pointer UV to pixel coordinates */
	if (!pointer_uv_to_pixel(w, fs->pointer_uv, &result.pixel_x, &result.pixel_y)) {
		return result;
	}

	/* Set target window ID */
	result.window_id = (int)w->capture_id;

	return result;
}
