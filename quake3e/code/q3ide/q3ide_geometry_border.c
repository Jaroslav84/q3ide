/* q3ide_geometry_border.c — Window background quad and border frame rendering. */

#include "q3ide_win_mngr.h"
#include "q3ide_params.h"
#include "q3ide_win_mngr_internal.h"
#include "../qcommon/qcommon.h"
#include "../client/client.h"
#include <math.h>

/* Shared basis — defined in q3ide_geometry.c */
extern void q3ide_win_basis(q3ide_win_t *win, vec3_t right, vec3_t up);

/* Compute right/up basis from an explicit normal (same math as q3ide_win_basis). */
static void normal_basis(vec3_t normal, vec3_t right, vec3_t up)
{
	float nx = normal[0], ny = normal[1];
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
 * q3ide_add_bg — black+logo backdrop sandwiched around the tunnel face.
 * Caller passes explicit origin/normal (wall or arc position).
 *
 * Two quads submitted as one poly call (numPolys=2):
 *   front bg: depth = 1.0 - BG_DEPTH_OFFSET — behind front face, CCW from front
 *   back  bg: depth = 1.0 + BG_DEPTH_OFFSET — behind back  face, CCW from back
 */
void q3ide_add_bg(q3ide_win_t *win, vec3_t origin, vec3_t normal)
{
	static const float sx[4] = {-1, 1, 1, -1};
	static const float sy[4] = {-1, -1, 1, 1};
	static const int back_idx[4] = {3, 2, 1, 0};
	polyVert_t verts[8];
	vec3_t right, up;
	float hw = win->world_w * 0.5f, hh = win->world_h * 0.5f;
	float fd = 1.0f - Q3IDE_BG_DEPTH_OFFSET;
	float bd = 1.0f + Q3IDE_BG_DEPTH_OFFSET;
	int i, bi;

	if (!q3ide_wm.bg_shader || !re.AddPolyToScene)
		return;

	normal_basis(normal, right, up);
	for (i = 0; i < 4; i++) {
		float bx = right[0] * sx[i] * hw + up[0] * sy[i] * hh;
		float by = right[1] * sx[i] * hw + up[1] * sy[i] * hh;
		float bz = right[2] * sx[i] * hw + up[2] * sy[i] * hh;
		float u = (sx[i] + 1.0f) * 0.5f;
		float v = (1.0f - sy[i]) * 0.5f;

		/* Front bg — CCW from front, at depth fd */
		verts[i].xyz[0] = origin[0] + bx + normal[0] * fd;
		verts[i].xyz[1] = origin[1] + by + normal[1] * fd;
		verts[i].xyz[2] = origin[2] + bz + normal[2] * fd;
		verts[i].st[0] = u;
		verts[i].st[1] = v;
		verts[i].modulate.rgba[0] = 255;
		verts[i].modulate.rgba[1] = 255;
		verts[i].modulate.rgba[2] = 255;
		verts[i].modulate.rgba[3] = 255;

		/* Back bg — reversed winding (CCW from back), at depth bd */
		bi = 4 + back_idx[i];
		verts[bi].xyz[0] = origin[0] + bx + normal[0] * bd;
		verts[bi].xyz[1] = origin[1] + by + normal[1] * bd;
		verts[bi].xyz[2] = origin[2] + bz + normal[2] * bd;
		verts[bi].st[0] = u;
		verts[bi].st[1] = v;
		verts[bi].modulate.rgba[0] = 255;
		verts[bi].modulate.rgba[1] = 255;
		verts[bi].modulate.rgba[2] = 255;
		verts[bi].modulate.rgba[3] = 255;
	}
	re.AddPolyToScene(q3ide_wm.bg_shader, 4, verts, 2);
}

/*
 * Fill 4 verts of one border strip into buf[base..base+3].
 * xs/ys are the 4 corner offsets in right/up space; depth is normal scale.
 */
static void q3ide_fill_strip(polyVert_t *buf, vec3_t origin, vec3_t normal, vec3_t right, vec3_t up, float x0, float y0,
                             float x1, float y1, float depth)
{
	int i;
	const float xs[4] = {x0, x1, x1, x0};
	const float ys[4] = {y0, y0, y1, y1};
	for (i = 0; i < 4; i++) {
		buf[i].xyz[0] = origin[0] + right[0] * xs[i] + up[0] * ys[i] + normal[0] * depth;
		buf[i].xyz[1] = origin[1] + right[1] * xs[i] + up[1] * ys[i] + normal[1] * depth;
		buf[i].xyz[2] = origin[2] + right[2] * xs[i] + up[2] * ys[i] + normal[2] * depth;
		buf[i].st[0] = 0.5f;
		buf[i].st[1] = 0.5f;
		buf[i].modulate.rgba[0] = 255;
		buf[i].modulate.rgba[1] = 255;
		buf[i].modulate.rgba[2] = 255;
		buf[i].modulate.rgba[3] = 255;
	}
}

/* Fill 4 verts of one edge quad into buf. Corners span fz→bz along normal. */
static void q3ide_fill_edge(polyVert_t *buf, vec3_t origin, vec3_t normal, vec3_t right, vec3_t up, float rx, float uy,
                            float rx2, float uy2, float fz, float bz)
{
	int i;
	const float rxs[4] = {rx, rx2, rx2, rx};
	const float uys[4] = {uy, uy2, uy2, uy};
	const float nzs[4] = {fz, fz, bz, bz};
	for (i = 0; i < 4; i++) {
		buf[i].xyz[0] = origin[0] + right[0] * rxs[i] + up[0] * uys[i] + normal[0] * nzs[i];
		buf[i].xyz[1] = origin[1] + right[1] * rxs[i] + up[1] * uys[i] + normal[1] * nzs[i];
		buf[i].xyz[2] = origin[2] + right[2] * rxs[i] + up[2] * uys[i] + normal[2] * nzs[i];
		buf[i].st[0] = 0.5f;
		buf[i].st[1] = 0.5f;
		buf[i].modulate.rgba[0] = 255;
		buf[i].modulate.rgba[1] = 255;
		buf[i].modulate.rgba[2] = 255;
		buf[i].modulate.rgba[3] = 255;
	}
}

/*
 * q3ide_add_frame — render the TV chassis and optional highlight border.
 * border_mode: 0 = normal (black edges), 1 = highlighted (red), 2 = green (wall-placed in arc)
 * Caller passes explicit origin/normal (wall or arc position).
 */
void q3ide_add_frame(q3ide_win_t *win, int border_mode, vec3_t origin, vec3_t normal)
{
	/*
	 * Unified TV chassis + face border — 1 AddPolyToScene call always.
	 *
	 * Normal:      4 side quads (edge_shader, black).
	 * Highlighted/Green: 4 side quads + 8 face-border strips = 12 quads,
	 *              all in border_shader or ov_green_shader — single call, 48 verts.
	 *
	 * win62/win63 both have cull disable — visible from any angle.
	 */
	polyVert_t verts[48];
	vec3_t right, up;
	float hw, hh, bw, fz, bz;
	qhandle_t frame_shader;

	if (!q3ide_wm.edge_shader || !q3ide_wm.border_shader || !re.AddPolyToScene)
		return;

	normal_basis(normal, right, up);
	hw = win->world_w * 0.5f;
	hh = win->world_h * 0.5f;
	fz = 1.0f + q3ide_params.windowDepth * 0.5f;
	bz = 1.0f - q3ide_params.windowDepth * 0.5f;

	/* 4 side quads — always first in the array. */
	q3ide_fill_edge(verts + 0, origin, normal, right, up, -hw, -hh, -hw, hh, fz, bz);
	q3ide_fill_edge(verts + 4, origin, normal, right, up, hw, -hh, hw, hh, fz, bz);
	q3ide_fill_edge(verts + 8, origin, normal, right, up, -hw, hh, hw, hh, fz, bz);
	q3ide_fill_edge(verts + 12, origin, normal, right, up, -hw, -hh, hw, -hh, fz, bz);

	if (border_mode == 0) {
		re.AddPolyToScene(q3ide_wm.edge_shader, 4, verts, 4);
		return;
	}

	/* Highlighted or green: append 8 face-border strips (4 front @ fz, 4 back @ bz). */
	frame_shader = (border_mode == 2 && q3ide_wm.ov_green_shader) ? q3ide_wm.ov_green_shader : q3ide_wm.border_shader;
	bw = q3ide_params.borderThickness;
	q3ide_fill_strip(verts + 16, origin, normal, right, up, -hw, -hh, -hw + bw, hh, fz);
	q3ide_fill_strip(verts + 20, origin, normal, right, up, hw - bw, -hh, hw, hh, fz);
	q3ide_fill_strip(verts + 24, origin, normal, right, up, -hw + bw, hh - bw, hw - bw, hh, fz);
	q3ide_fill_strip(verts + 28, origin, normal, right, up, -hw + bw, -hh, hw - bw, -hh + bw, fz);
	q3ide_fill_strip(verts + 32, origin, normal, right, up, -hw, -hh, -hw + bw, hh, bz);
	q3ide_fill_strip(verts + 36, origin, normal, right, up, hw - bw, -hh, hw, hh, bz);
	q3ide_fill_strip(verts + 40, origin, normal, right, up, -hw + bw, hh - bw, hw - bw, hh, bz);
	q3ide_fill_strip(verts + 44, origin, normal, right, up, -hw + bw, -hh, hw - bw, -hh + bw, bz);
	re.AddPolyToScene(frame_shader, 4, verts, 12);
}
