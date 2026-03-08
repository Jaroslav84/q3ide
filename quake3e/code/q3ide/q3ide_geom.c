/*
 * q3ide_geom.c — Geometry and rendering helpers for Q3IDE.
 *
 * Window quad polygon generation with hover border effect, and ray tracing for window hit detection.
 */

#include "q3ide_wm.h"
#include "q3ide_wm_internal.h"
#include "../../../q3ide_design.h"
#include "../qcommon/qcommon.h"
#include "../client/client.h"
#include <math.h>

/* Compute the right/up basis for a window (shared by depth prepass and poly render). */
static void q3ide_win_basis(q3ide_win_t *win, vec3_t right, vec3_t up)
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
 * q3ide_add_depth_quad — depth prepass for occlusion.
 *
 * Q3 sorts polys by shader index internally, so windows with different scratch
 * shaders don't respect painter's algorithm. Fix: submit all windows first as
 * black opaque quads using the shared *white shader (low shader index = renders
 * before all *scratch shaders). This writes correct depth values so the texture
 * pass renders with proper occlusion regardless of Q3's internal sort.
 */
void q3ide_add_depth_quad(q3ide_win_t *win)
{
	polyVert_t verts[4];
	vec3_t right, up;
	float hw = win->world_w * 0.5f, hh = win->world_h * 0.5f;
	float hover_lift = win->hover_t > 0.0f ? win->hover_t * 18.0f : 0.0f;
	int i;

	if (!q3ide_wm.border_shader || !re.AddPolyToScene)
		return;

	q3ide_win_basis(win, right, up);

	/* Front face only — back face depth prepass not needed and may cause artifacts. */
	for (i = 0; i < 4; i++) {
		float sx = (i == 0 || i == 3) ? -1.0f : 1.0f;
		float sy = (i == 0 || i == 1) ? -1.0f : 1.0f;
		verts[i].xyz[0] = win->origin[0] + right[0] * sx * hw + up[0] * sy * hh + win->normal[0] * hover_lift;
		verts[i].xyz[1] = win->origin[1] + right[1] * sx * hw + up[1] * sy * hh + win->normal[1] * hover_lift;
		verts[i].xyz[2] = win->origin[2] + right[2] * sx * hw + up[2] * sy * hh + win->normal[2] * hover_lift;
		verts[i].st[0] = 0.5f;
		verts[i].st[1] = 0.5f;
		verts[i].modulate.rgba[0] = 0; /* black — texture pass overwrites */
		verts[i].modulate.rgba[1] = 0;
		verts[i].modulate.rgba[2] = 0;
		verts[i].modulate.rgba[3] = 255;
	}
	re.AddPolyToScene(q3ide_wm.border_shader, 4, verts, 1);
}

void q3ide_add_poly(q3ide_win_t *win)
{
	polyVert_t verts[4];
	vec3_t right, up;
	float hw = win->world_w * 0.5f, hh = win->world_h * 0.5f;
	int i;
	float hover_lift = 0.0f;

	if (!win->shader || !re.AddPolyToScene)
		return;

	q3ide_win_basis(win, right, up);

	if (win->hover_t > 0.0f)
		hover_lift = win->hover_t * 18.0f; /* pop window toward viewer */

	/* Front and back faces — double-sided so windows are visible from any angle.
	 * Back face flips the right vector (sign=-1) giving a mirrored but visible texture. */
	{
		int face;
		for (face = 0; face < 2; face++) {
			float sign = face ? -1.0f : 1.0f;
			for (i = 0; i < 4; i++) {
				float sx = (i == 0 || i == 3) ? -1.0f : 1.0f;
				float sy = (i == 0 || i == 1) ? -1.0f : 1.0f;
				verts[i].xyz[0] =
				    win->origin[0] + sign * right[0] * sx * hw + up[0] * sy * hh + win->normal[0] * hover_lift;
				verts[i].xyz[1] =
				    win->origin[1] + sign * right[1] * sx * hw + up[1] * sy * hh + win->normal[1] * hover_lift;
				verts[i].xyz[2] =
				    win->origin[2] + sign * right[2] * sx * hw + up[2] * sy * hh + win->normal[2] * hover_lift;
				verts[i].st[0] = 1.0f - (sx + 1.0f) * 0.5f;
				verts[i].st[1] = 1.0f - (sy + 1.0f) * 0.5f;
				verts[i].modulate.rgba[0] = 255;
				verts[i].modulate.rgba[1] = 255;
				verts[i].modulate.rgba[2] = 255;
				verts[i].modulate.rgba[3] = 255;
			}
			re.AddPolyToScene(win->shader, 4, verts, 1);
		}
	}

	/* 2-unit red border on the front face when hovering. */
	if (win->hover_t > 0.0f && q3ide_wm.border_shader) {
		float thick = 2.0f;
		byte br = (byte) (220.0f * win->hover_t);
		/* (lx_lo, lx_hi, ly_lo, ly_hi) in local window space */
		float strips[4][4] = {
		    {-hw, hw, hh, hh + thick},                   /* top    */
		    {-hw, hw, -hh - thick, -hh},                 /* bottom */
		    {-hw - thick, -hw, -hh - thick, hh + thick}, /* left   */
		    {hw, hw + thick, -hh - thick, hh + thick},   /* right  */
		};
		int s;
		for (s = 0; s < 4; s++) {
			float lx0 = strips[s][0], lx1 = strips[s][1];
			float ly0 = strips[s][2], ly1 = strips[s][3];
			float qx[4] = {lx0, lx1, lx1, lx0};
			float qy[4] = {ly0, ly0, ly1, ly1};
			for (i = 0; i < 4; i++) {
				verts[i].xyz[0] = win->origin[0] + right[0] * qx[i] + up[0] * qy[i] + win->normal[0] * hover_lift;
				verts[i].xyz[1] = win->origin[1] + right[1] * qx[i] + up[1] * qy[i] + win->normal[1] * hover_lift;
				verts[i].xyz[2] = win->origin[2] + right[2] * qx[i] + up[2] * qy[i] + win->normal[2] * hover_lift;
				verts[i].st[0] = 0.5f;
				verts[i].st[1] = 0.5f;
				verts[i].modulate.rgba[0] = br;
				verts[i].modulate.rgba[1] = 0;
				verts[i].modulate.rgba[2] = 0;
				verts[i].modulate.rgba[3] = 255;
			}
			re.AddPolyToScene(q3ide_wm.border_shader, 4, verts, 1);
		}
	}
}

/*
 * q3ide_add_portal_frame — teleporter energy portal over the first mirror.
 *
 * Same as the real Q3 teleporter: a flat quad with the additive energy
 * shader. Pushed 6 units forward (toward viewer) so it renders clearly
 * in front of the window. The shader is GL_ONE GL_ONE additive so you
 * see the window content through the glowing swirl — exactly like standing
 * in front of a teleporter portal.
 */
void q3ide_add_portal_frame(q3ide_win_t *win, qhandle_t shader)
{
	polyVert_t verts[4];
	vec3_t right, up;
	/* 1.3x window size: portal extends past the window edges as a glow ring */
	float hw = win->world_w * 0.5f * 1.3f;
	float hh = win->world_h * 0.5f * 1.3f;
	float fwd = 6.0f; /* forward from window toward viewer */
	float nx = win->normal[0], ny = win->normal[1];
	float horiz_len = sqrtf(nx * nx + ny * ny);
	int i;

	if (!shader || !re.AddPolyToScene)
		return;

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
		float sx = (i == 0 || i == 3) ? -1.0f : 1.0f;
		float sy = (i == 0 || i == 1) ? -1.0f : 1.0f;
		verts[i].xyz[0] = win->origin[0] + win->normal[0] * fwd + right[0] * sx * hw + up[0] * sy * hh;
		verts[i].xyz[1] = win->origin[1] + win->normal[1] * fwd + right[1] * sx * hw + up[1] * sy * hh;
		verts[i].xyz[2] = win->origin[2] + win->normal[2] * fwd + right[2] * sx * hw + up[2] * sy * hh;
		verts[i].st[0] = (sx + 1.0f) * 0.5f;
		verts[i].st[1] = (sy + 1.0f) * 0.5f;
		verts[i].modulate.rgba[0] = 255;
		verts[i].modulate.rgba[1] = 255;
		verts[i].modulate.rgba[2] = 255;
		verts[i].modulate.rgba[3] = 255;
	}
	re.AddPolyToScene(shader, 4, verts, 1);
}

int Q3IDE_WM_TraceWindowHit(vec3_t start, vec3_t dir)
{
	int i, best = -1;
	float best_t = 512.0f; /* Q3IDE_WALL_DIST */

	for (i = 0; i < Q3IDE_MAX_WIN; i++) {
		q3ide_win_t *win = &q3ide_wm.wins[i];
		vec3_t right, up, diff, hit;
		float denom, t, hw, hh, lx, ly;

		if (!win->active || !win->shader)
			continue;

		denom = DotProduct(dir, win->normal);
		if (fabsf(denom) < 0.001f)
			continue;

		VectorSubtract(win->origin, start, diff);
		t = DotProduct(diff, win->normal) / denom;
		if (t < 0 || t >= best_t)
			continue;

		hit[0] = start[0] + dir[0] * t;
		hit[1] = start[1] + dir[1] * t;
		hit[2] = start[2] + dir[2] * t;

		if (fabsf(win->normal[2]) > 0.99f) {
			vec3_t fwd = {1, 0, 0};
			CrossProduct(win->normal, fwd, right);
		} else {
			vec3_t wup = {0, 0, 1};
			CrossProduct(win->normal, wup, right);
		}
		VectorNormalize(right);
		CrossProduct(right, win->normal, up);
		VectorNormalize(up);

		VectorSubtract(hit, win->origin, diff);
		lx = DotProduct(diff, right);
		ly = DotProduct(diff, up);
		hw = win->world_w * 0.5f;
		hh = win->world_h * 0.5f;

		if (fabsf(lx) <= hw && fabsf(ly) <= hh) {
			best_t = t;
			best = i;
		}
	}
	return best;
}
