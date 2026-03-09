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
	/* Forward pop: 1 unit base + up to 4 units on full hover (toward player) */
	float pop = 1.0f + win->hover_t * 4.0f;

	if (!win->shader || !re.AddPolyToScene)
		return;

	q3ide_win_basis(win, right, up);

	/* Front face */
	for (i = 0; i < 4; i++) {
		float sx = (i == 0 || i == 3) ? -1.0f : 1.0f;
		float sy = (i == 0 || i == 1) ? -1.0f : 1.0f;
		verts[i].xyz[0] = win->origin[0] + right[0] * sx * hw + up[0] * sy * hh + win->normal[0] * pop;
		verts[i].xyz[1] = win->origin[1] + right[1] * sx * hw + up[1] * sy * hh + win->normal[1] * pop;
		verts[i].xyz[2] = win->origin[2] + right[2] * sx * hw + up[2] * sy * hh + win->normal[2] * pop;
		verts[i].st[0] = 1.0f - (sx + 1.0f) * 0.5f;
		verts[i].st[1] = 1.0f - (sy + 1.0f) * 0.5f;
		verts[i].modulate.rgba[0] = 255;
		verts[i].modulate.rgba[1] = (byte) (255 - 80.0f * win->hover_t);
		verts[i].modulate.rgba[2] = (byte) (255 - 80.0f * win->hover_t);
		verts[i].modulate.rgba[3] = 255;
	}
	re.AddPolyToScene(win->shader, 4, verts, 1);

	/* Back face: wall-mounted windows skip this — they're flush against geometry.
	 * Free-floating windows render the back face so they're visible from both sides. */
	if (!win->wall_mounted) {
		for (i = 0; i < 4; i++) {
			float sx = (i == 0 || i == 3) ? -1.0f : 1.0f;
			float sy = (i == 0 || i == 1) ? -1.0f : 1.0f;
			verts[i].xyz[0] = win->origin[0] - right[0] * sx * hw + up[0] * sy * hh - win->normal[0];
			verts[i].xyz[1] = win->origin[1] - right[1] * sx * hw + up[1] * sy * hh - win->normal[1];
			verts[i].xyz[2] = win->origin[2] - right[2] * sx * hw + up[2] * sy * hh - win->normal[2];
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

/*
 * q3ide_add_portal_frame — teleporter energy overlay.
 *
 * Placed 2 units in front of the window (along win->normal) to sit just
 * above the texture quad without z-fighting. When wins[0] is snapped into
 * the real q3dm0 teleporter arch the energy glow covers it exactly.
 */
void q3ide_add_portal_frame(q3ide_win_t *win, qhandle_t shader)
{
	polyVert_t verts[4];
	vec3_t right, up;
	float hw = win->world_w * 0.5f;
	float hh = win->world_h * 0.5f;
	float fwd = 2.0f; /* in front of window — no z-fight */
	int i;

	if (!shader || !re.AddPolyToScene)
		return;

	q3ide_win_basis(win, right, up);

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
	/* Back face: reversed winding so energy is visible from behind the arch */
	{
		polyVert_t rv[4];
		rv[0] = verts[3];
		rv[1] = verts[2];
		rv[2] = verts[1];
		rv[3] = verts[0];
		re.AddPolyToScene(shader, 4, rv, 1);
	}
}

/*
 * q3ide_add_blood_splat — render a blood mark at the bullet hit point.
 *
 * A cross of two red rectangles + 4 scattered drip squares. Lives for
 * Q3IDE_SPLAT_LIFE_MS ms then vanishes.  All geometry lies in the
 * window's local right/up plane so it sits flush on the window surface.
 */
#define Q3IDE_SPLAT_LIFE_MS 700ULL

static void q3ide_splat_quad(vec3_t center, vec3_t right, vec3_t up, vec3_t normal, float hw, float hh, byte r, byte g,
                             byte b, qhandle_t shader)
{
	polyVert_t v[4];
	int i;
	const float xs[4] = {-hw, hw, hw, -hw};
	const float ys[4] = {-hh, -hh, hh, hh};
	for (i = 0; i < 4; i++) {
		v[i].xyz[0] = center[0] + right[0] * xs[i] + up[0] * ys[i] + normal[0];
		v[i].xyz[1] = center[1] + right[1] * xs[i] + up[1] * ys[i] + normal[1];
		v[i].xyz[2] = center[2] + right[2] * xs[i] + up[2] * ys[i] + normal[2];
		v[i].st[0] = 0.5f;
		v[i].st[1] = 0.5f;
		v[i].modulate.rgba[0] = r;
		v[i].modulate.rgba[1] = g;
		v[i].modulate.rgba[2] = b;
		v[i].modulate.rgba[3] = 255;
	}
	re.AddPolyToScene(shader, 4, v, 1);
}

void q3ide_add_blood_splat(q3ide_win_t *win)
{
	unsigned long long now_ms, elapsed;
	vec3_t right, up;
	/* Drip offsets in local (right, up) units */
	static const float drip_r[4] = {28.f, -26.f, 10.f, -18.f};
	static const float drip_u[4] = {20.f, -22.f, -28.f, 24.f};
	int i;

	if (!win->hit_time_ms || !q3ide_wm.border_shader || !re.AddPolyToScene)
		return;

	now_ms = (unsigned long long) Sys_Milliseconds();
	elapsed = now_ms - win->hit_time_ms;
	if (elapsed >= Q3IDE_SPLAT_LIFE_MS) {
		win->hit_time_ms = 0;
		return;
	}

	q3ide_win_basis(win, right, up);

	/* Central cross — two rectangles at 90° */
	q3ide_splat_quad(win->hit_pos, right, up, win->normal, 14.f, 4.f, 220, 0, 0, q3ide_wm.border_shader);
	q3ide_splat_quad(win->hit_pos, right, up, win->normal, 4.f, 14.f, 180, 0, 0, q3ide_wm.border_shader);

	/* Scattered drip squares */
	for (i = 0; i < 4; i++) {
		vec3_t drip;
		drip[0] = win->hit_pos[0] + right[0] * drip_r[i] + up[0] * drip_u[i];
		drip[1] = win->hit_pos[1] + right[1] * drip_r[i] + up[1] * drip_u[i];
		drip[2] = win->hit_pos[2] + right[2] * drip_r[i] + up[2] * drip_u[i];
		q3ide_splat_quad(drip, right, up, win->normal, 5.f, 5.f, 200, 10, 10, q3ide_wm.border_shader);
	}
}

/*
 * q3ide_clamp_window_size -- shrink world_w/world_h to fit within adjacent
 * walls/ceiling/floor, leaving a 1.5-unit gap on each side.
 * Aspect ratio is always preserved via uniform scaling.
 */
void q3ide_clamp_window_size(q3ide_win_t *win)
{
	vec3_t right, up, start, end;
	trace_t tr;
	static vec3_t mins = {0, 0, 0}, maxs = {0, 0, 0};
	float orig_hw, orig_hh, hw, hh, lim, rw, rh, r;

	q3ide_win_basis(win, right, up);

	/* Capture original sizes before any modification */
	orig_hw = hw = win->world_w * 0.5f;
	orig_hh = hh = win->world_h * 0.5f;

	/* Nudge start off the placement wall */
	start[0] = win->origin[0] + win->normal[0] * 2.0f;
	start[1] = win->origin[1] + win->normal[1] * 2.0f;
	start[2] = win->origin[2] + win->normal[2] * 2.0f;

	/* +right */
	end[0] = start[0] + right[0] * hw;
	end[1] = start[1] + right[1] * hw;
	end[2] = start[2] + right[2] * hw;
	CM_BoxTrace(&tr, start, end, mins, maxs, 0, CONTENTS_SOLID, qfalse);
	if (tr.fraction < 1.0f) {
		lim = tr.fraction * hw - 1.5f;
		if (lim < 1.5f)
			lim = 1.5f;
		if (lim < hw)
			hw = lim;
	}

	/* -right */
	end[0] = start[0] - right[0] * hw;
	end[1] = start[1] - right[1] * hw;
	end[2] = start[2] - right[2] * hw;
	CM_BoxTrace(&tr, start, end, mins, maxs, 0, CONTENTS_SOLID, qfalse);
	if (tr.fraction < 1.0f) {
		lim = tr.fraction * hw - 1.5f;
		if (lim < 1.5f)
			lim = 1.5f;
		if (lim < hw)
			hw = lim;
	}

	/* +up */
	end[0] = start[0] + up[0] * hh;
	end[1] = start[1] + up[1] * hh;
	end[2] = start[2] + up[2] * hh;
	CM_BoxTrace(&tr, start, end, mins, maxs, 0, CONTENTS_SOLID, qfalse);
	if (tr.fraction < 1.0f) {
		lim = tr.fraction * hh - 1.5f;
		if (lim < 1.5f)
			lim = 1.5f;
		if (lim < hh)
			hh = lim;
	}

	/* -up */
	end[0] = start[0] - up[0] * hh;
	end[1] = start[1] - up[1] * hh;
	end[2] = start[2] - up[2] * hh;
	CM_BoxTrace(&tr, start, end, mins, maxs, 0, CONTENTS_SOLID, qfalse);
	if (tr.fraction < 1.0f) {
		lim = tr.fraction * hh - 1.5f;
		if (lim < 1.5f)
			lim = 1.5f;
		if (lim < hh)
			hh = lim;
	}

	/* Uniform scale from original: preserve aspect ratio always */
	rw = (orig_hw > 0.0f) ? (hw / orig_hw) : 1.0f;
	rh = (orig_hh > 0.0f) ? (hh / orig_hh) : 1.0f;
	r = (rw < rh) ? rw : rh;
	if (r > 1.0f)
		r = 1.0f;

	win->world_w = orig_hw * 2.0f * r;
	win->world_h = orig_hh * 2.0f * r;

	if (win->world_w < 32.0f)
		win->world_w = 32.0f;
	if (win->world_h < 32.0f)
		win->world_h = 32.0f;
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
