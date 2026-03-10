/*
 * q3ide_geometry.c — Window polygon generation (border, strips, main quad).
 * Effects (portal frame, blood splat): q3ide_effects.c.
 * Clamping + ray-hit: q3ide_geometry_clamp.c.
 */

#include "q3ide_win_mngr.h"
#include "q3ide_params.h"
#include "q3ide_win_mngr_internal.h"
#include "../qcommon/qcommon.h"
#include "../client/client.h"
#include <math.h>

/* Compute right/up basis for a window quad. */
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
 * Fill 4 verts of one border strip into buf[base..base+3].
 * xs/ys are the 4 corner offsets in right/up space; depth is normal scale.
 */
static void q3ide_fill_strip(polyVert_t *buf, q3ide_win_t *win, vec3_t right, vec3_t up, float x0, float y0,
                              float x1, float y1, float depth)
{
	int i;
	const float xs[4] = {x0, x1, x1, x0};
	const float ys[4] = {y0, y0, y1, y1};
	for (i = 0; i < 4; i++) {
		buf[i].xyz[0]           = win->origin[0] + right[0] * xs[i] + up[0] * ys[i] + win->normal[0] * depth;
		buf[i].xyz[1]           = win->origin[1] + right[1] * xs[i] + up[1] * ys[i] + win->normal[1] * depth;
		buf[i].xyz[2]           = win->origin[2] + right[2] * xs[i] + up[2] * ys[i] + win->normal[2] * depth;
		buf[i].st[0]            = 0.5f;
		buf[i].st[1]            = 0.5f;
		buf[i].modulate.rgba[0] = 255;
		buf[i].modulate.rgba[1] = 255;
		buf[i].modulate.rgba[2] = 255;
		buf[i].modulate.rgba[3] = 255;
	}
}


void q3ide_add_poly(q3ide_win_t *win)
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
	float hw = win->world_w * 0.5f, hh = win->world_h * 0.5f;
	float uv_w = win->uv_x1 - win->uv_x0;
	int i, bi;

	if (!win->shader || !re.AddPolyToScene)
		return;

	q3ide_win_basis(win, right, up);
	for (i = 0; i < 4; i++) {
		float wx = win->origin[0] + right[0] * sx[i] * hw + up[0] * sy[i] * hh + win->normal[0];
		float wy = win->origin[1] + right[1] * sx[i] * hw + up[1] * sy[i] * hh + win->normal[1];
		float wz = win->origin[2] + right[2] * sx[i] * hw + up[2] * sy[i] * hh + win->normal[2];
		/* Front quad [0..3]: CCW from front. U flipped (uv_x1→uv_x0) because
		 * the GL texture origin is bottom-left while SCK pixels are top-down,
		 * causing an inherent horizontal mirror that we correct here. */
		verts[i].xyz[0]           = wx;
		verts[i].xyz[1]           = wy;
		verts[i].xyz[2]           = wz;
		verts[i].st[0]            = win->uv_x1 - (sx[i] + 1.0f) * 0.5f * uv_w;
		verts[i].st[1]            = (1.0f - sy[i]) * 0.5f;
		verts[i].modulate.rgba[0] = 255;
		verts[i].modulate.rgba[1] = 255;
		verts[i].modulate.rgba[2] = 255;
		verts[i].modulate.rgba[3] = 255;
		/* Back quad [4..7]: reversed winding (CCW from behind). Swapping U back
		 * gives correct image from behind — the winding reversal handles mirroring. */
		bi                            = 4 + back_idx[i];
		verts[bi].xyz[0]              = wx;
		verts[bi].xyz[1]              = wy;
		verts[bi].xyz[2]              = wz;
		verts[bi].st[0]               = win->uv_x0 + (sx[i] + 1.0f) * 0.5f * uv_w;
		verts[bi].st[1]               = (1.0f - sy[i]) * 0.5f;
		verts[bi].modulate.rgba[0]    = 255;
		verts[bi].modulate.rgba[1]    = 255;
		verts[bi].modulate.rgba[2]    = 255;
		verts[bi].modulate.rgba[3]    = 255;
	}
	/* 1 texture, 1 call, 2 quads — front+back share the same scratch slot. */
	re.AddPolyToScene(win->shader, 4, verts, 2);
}

/* Fill 4 verts of one edge quad into buf. Corners span fz→bz along normal. */
static void q3ide_fill_edge(polyVert_t *buf, q3ide_win_t *win, vec3_t right, vec3_t up, float rx, float uy,
                             float rx2, float uy2, float fz, float bz)
{
	int i;
	const float rxs[4] = {rx, rx2, rx2, rx};
	const float uys[4] = {uy, uy2, uy2, uy};
	const float nzs[4] = {fz, fz, bz, bz};
	for (i = 0; i < 4; i++) {
		buf[i].xyz[0]           = win->origin[0] + right[0] * rxs[i] + up[0] * uys[i] + win->normal[0] * nzs[i];
		buf[i].xyz[1]           = win->origin[1] + right[1] * rxs[i] + up[1] * uys[i] + win->normal[1] * nzs[i];
		buf[i].xyz[2]           = win->origin[2] + right[2] * rxs[i] + up[2] * uys[i] + win->normal[2] * nzs[i];
		buf[i].st[0]            = 0.5f;
		buf[i].st[1]            = 0.5f;
		buf[i].modulate.rgba[0] = 255;
		buf[i].modulate.rgba[1] = 255;
		buf[i].modulate.rgba[2] = 255;
		buf[i].modulate.rgba[3] = 255;
	}
}

void q3ide_add_frame(q3ide_win_t *win, qboolean highlighted)
{
	/*
	 * Unified TV chassis + face border — 1 AddPolyToScene call always.
	 *
	 * Not highlighted: 4 side quads (edge_shader, black).
	 * Highlighted:     4 side quads + 8 face-border strips = 12 quads,
	 *                  all in border_shader (red) — single call, 48 verts.
	 *
	 * win62/win63 both have cull disable — visible from any angle.
	 */
	/* Worst case: 12 quads × 4 verts = 48. Stack allocation — no heap. */
	polyVert_t verts[48];
	vec3_t right, up;
	float hw, hh, bw, fz, bz;

	if (!q3ide_wm.edge_shader || !q3ide_wm.border_shader || !re.AddPolyToScene)
		return;

	q3ide_win_basis(win, right, up);
	hw = win->world_w * 0.5f;
	hh = win->world_h * 0.5f;
	fz = 1.0f + q3ide_params.windowDepth * 0.5f;
	bz = 1.0f - q3ide_params.windowDepth * 0.5f;

	/* 4 side quads — always first in the array. */
	q3ide_fill_edge(verts + 0,  win, right, up, -hw, -hh, -hw, hh,  fz, bz);
	q3ide_fill_edge(verts + 4,  win, right, up,  hw, -hh,  hw, hh,  fz, bz);
	q3ide_fill_edge(verts + 8,  win, right, up, -hw,  hh,  hw, hh,  fz, bz);
	q3ide_fill_edge(verts + 12, win, right, up, -hw, -hh,  hw, -hh, fz, bz);

	if (!highlighted) {
		re.AddPolyToScene(q3ide_wm.edge_shader, 4, verts, 4);
		return;
	}

	/* Highlighted: append 8 face-border strips (4 front @ fz, 4 back @ bz).
	 * Same shader as sides → everything batched into one call. */
	bw = q3ide_params.borderThickness;
	q3ide_fill_strip(verts + 16, win, right, up, -hw,      -hh,      -hw + bw, hh,       fz);
	q3ide_fill_strip(verts + 20, win, right, up,  hw - bw, -hh,       hw,      hh,       fz);
	q3ide_fill_strip(verts + 24, win, right, up, -hw + bw,  hh - bw,  hw - bw, hh,       fz);
	q3ide_fill_strip(verts + 28, win, right, up, -hw + bw, -hh,       hw - bw, -hh + bw, fz);
	q3ide_fill_strip(verts + 32, win, right, up, -hw,      -hh,      -hw + bw, hh,       bz);
	q3ide_fill_strip(verts + 36, win, right, up,  hw - bw, -hh,       hw,      hh,       bz);
	q3ide_fill_strip(verts + 40, win, right, up, -hw + bw,  hh - bw,  hw - bw, hh,       bz);
	q3ide_fill_strip(verts + 44, win, right, up, -hw + bw, -hh,       hw - bw, -hh + bw, bz);
	re.AddPolyToScene(q3ide_wm.border_shader, 4, verts, 12);
}

/* Portal frame and blood splat — q3ide_effects.c */
