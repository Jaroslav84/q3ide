/*
 * q3ide_geometry.c — Window polygon generation (main quad).
 * Frame/border geometry: q3ide_geometry_border.c.
 * Clamping + ray-hit: q3ide_geometry_clamp.c / q3ide_window_trace.c.
 */

#include "q3ide_win_mngr.h"
#include "q3ide_params.h"
#include "q3ide_win_mngr_internal.h"
#include "../qcommon/qcommon.h"
#include "../client/client.h"
#include <math.h>

/* Compute right/up basis for a window quad from win->normal. */
void q3ide_win_basis(q3ide_win_t *win, vec3_t right, vec3_t up)
{
	float nx = win->normal[0], ny = win->normal[1];
	float horiz_len = sqrtf(nx * nx + ny * ny);
	if (horiz_len > 0.01f) {
		right[0] = -ny / horiz_len;
		right[1] = nx / horiz_len;
		right[2] = 0.0f;
	} else {
		right[0] = 1.0f;
		right[1] = 0.0f;
		right[2] = 0.0f;
	}
	up[0] = 0.0f;
	up[1] = 0.0f;
	up[2] = 1.0f;
}

/*
 * q3ide_add_poly — render the window texture quad at the given origin/normal.
 * Caller passes explicit origin/normal so this works for both wall and arc positions.
 */
void q3ide_add_poly(q3ide_win_t *win, vec3_t origin, vec3_t normal)
{
	/*
	 * Two quads — same vertices, same shader (no cull disable on win0-61).
	 * Front quad: CCW from front, U left→right.
	 * Back quad:  reversed winding (CCW from behind), U right→left so the
	 * image reads correctly when viewed through the screen from behind.
	 * Multiplayer: both sides see an un-mirrored image. Cost: 4 extra verts.
	 */
	static const float sx[4] = {-1, 1, 1, -1};
	static const float sy[4] = {-1, -1, 1, 1};
	/* Back quad: reverse vertex order → TL, TR, BR, BL (CCW from behind). */
	static const int back_idx[4] = {3, 2, 1, 0};
	/* 8 verts: front[0..3] + back[4..7] — single API call, numPolys=2. */
	polyVert_t verts[8];
	vec3_t right, up;
	float nx = normal[0], ny = normal[1];
	float horiz_len = sqrtf(nx * nx + ny * ny);
	float hw = win->world_w * 0.5f, hh = win->world_h * 0.5f;
	float uv_w = win->uv_x1 - win->uv_x0;
	int i, bi;

	if (!win->shader || !re.AddPolyToScene)
		return;

	/* Compute basis from passed normal */
	if (horiz_len > 0.01f) {
		right[0] = -ny / horiz_len;
		right[1] = nx / horiz_len;
		right[2] = 0.0f;
	} else {
		right[0] = 1.0f;
		right[1] = 0.0f;
		right[2] = 0.0f;
	}
	up[0] = 0.0f;
	up[1] = 0.0f;
	up[2] = 1.0f;

	for (i = 0; i < 4; i++) {
		float wx = origin[0] + right[0] * sx[i] * hw + up[0] * sy[i] * hh + normal[0];
		float wy = origin[1] + right[1] * sx[i] * hw + up[1] * sy[i] * hh + normal[1];
		float wz = origin[2] + right[2] * sx[i] * hw + up[2] * sy[i] * hh + normal[2];
		/* Front quad [0..3]: CCW from front. U flipped (uv_x1→uv_x0) because
		 * the GL texture origin is bottom-left while SCK pixels are top-down,
		 * causing an inherent horizontal mirror that we correct here. */
		verts[i].xyz[0] = wx;
		verts[i].xyz[1] = wy;
		verts[i].xyz[2] = wz;
		verts[i].st[0] = win->uv_x1 - (sx[i] + 1.0f) * 0.5f * uv_w;
		verts[i].st[1] = (1.0f - sy[i]) * 0.5f;
		verts[i].modulate.rgba[0] = 255;
		verts[i].modulate.rgba[1] = 255;
		verts[i].modulate.rgba[2] = 255;
		verts[i].modulate.rgba[3] = 255;
		/* Back quad [4..7]: reversed winding (CCW from behind). Swapping U back
		 * gives correct image from behind — the winding reversal handles mirroring. */
		bi = 4 + back_idx[i];
		verts[bi].xyz[0] = wx;
		verts[bi].xyz[1] = wy;
		verts[bi].xyz[2] = wz;
		verts[bi].st[0] = win->uv_x0 + (sx[i] + 1.0f) * 0.5f * uv_w;
		verts[bi].st[1] = (1.0f - sy[i]) * 0.5f;
		verts[bi].modulate.rgba[0] = 255;
		verts[bi].modulate.rgba[1] = 255;
		verts[bi].modulate.rgba[2] = 255;
		verts[bi].modulate.rgba[3] = 255;
	}
	/* 1 texture, 1 call, 2 quads — front+back share the same scratch slot. */
	re.AddPolyToScene(win->shader, 4, verts, 2);
}
