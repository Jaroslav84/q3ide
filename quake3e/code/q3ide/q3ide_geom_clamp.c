/*
 * q3ide_geom_clamp.c — Window size clamping and ray-window hit detection.
 */

#include "q3ide_wm.h"
#include "q3ide_wm_internal.h"
#include "../qcommon/qcommon.h"
#include <math.h>

/* Compute right/up basis — duplicated from geom.c (both static, no cross-TU dep) */
static void q3ide_win_basis_clamp(q3ide_win_t *win, vec3_t right, vec3_t up)
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
 * q3ide_clamp_window_size — shrink world_w/h to fit within adjacent walls,
 * preserving aspect ratio. 1.5u gap on each side.
 */
void q3ide_clamp_window_size(q3ide_win_t *win)
{
	vec3_t right, up, start, end;
	trace_t tr;
	static vec3_t mins = {0, 0, 0}, maxs = {0, 0, 0};
	float orig_hw, orig_hh, hw, hh, lim, rw, rh, r;

	q3ide_win_basis_clamp(win, right, up);

	orig_hw = hw = win->world_w * 0.5f;
	orig_hh = hh = win->world_h * 0.5f;

	start[0] = win->origin[0] + win->normal[0] * 3.0f;
	start[1] = win->origin[1] + win->normal[1] * 3.0f;
	start[2] = win->origin[2] + win->normal[2] * 3.0f;

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

	/* Uniform scale to preserve aspect ratio */
	rw = (orig_hw > 0.0f) ? (hw / orig_hw) : 1.0f;
	rh = (orig_hh > 0.0f) ? (hh / orig_hh) : 1.0f;
	r = (rw < rh) ? rw : rh;
	if (r > 1.0f)
		r = 1.0f;

	win->world_w = orig_hw * 2.0f * r;
	win->world_h = orig_hh * 2.0f * r;
	/* Minimum size: scale both dims uniformly so neither drops below 32. */
	{
		float min_scale = 1.0f;
		if (win->world_w > 0.0f && win->world_w < 32.0f)
			min_scale = 32.0f / win->world_w;
		if (win->world_h > 0.0f && win->world_h < 32.0f) {
			float ms = 32.0f / win->world_h;
			if (ms > min_scale)
				min_scale = ms;
		}
		if (min_scale > 1.0f) {
			win->world_w *= min_scale;
			win->world_h *= min_scale;
		}
	}
}

int Q3IDE_WM_TraceWindowHit(vec3_t start, vec3_t dir, int skip_idx)
{
	int i, best = -1;
	float best_t = 512.0f;

	for (i = 0; i < Q3IDE_MAX_WIN; i++) {
		q3ide_win_t *win = &q3ide_wm.wins[i];
		vec3_t right, up, diff, hit;
		float denom, t, hw, hh, lx, ly;

		if (!win->active)
			continue;
		if (i == skip_idx)
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

#define Q3IDE_WALL_DIST   512.0f
#define Q3IDE_WALL_OFFSET 3.0f

qboolean Q3IDE_WM_TraceWall(vec3_t start, vec3_t dir, vec3_t out_pos, vec3_t out_normal)
{
	trace_t tr;
	vec3_t end, mins = {0, 0, 0}, maxs = {0, 0, 0};
	end[0] = start[0] + dir[0] * Q3IDE_WALL_DIST;
	end[1] = start[1] + dir[1] * Q3IDE_WALL_DIST;
	end[2] = start[2] + dir[2] * Q3IDE_WALL_DIST;
	CM_BoxTrace(&tr, start, end, mins, maxs, 0, CONTENTS_SOLID, qfalse);
	if (tr.fraction >= 1.0f || tr.startsolid)
		return qfalse;
	out_pos[0] = tr.endpos[0] + tr.plane.normal[0] * Q3IDE_WALL_OFFSET;
	out_pos[1] = tr.endpos[1] + tr.plane.normal[1] * Q3IDE_WALL_OFFSET;
	out_pos[2] = tr.endpos[2] + tr.plane.normal[2] * Q3IDE_WALL_OFFSET;
	VectorCopy(tr.plane.normal, out_normal);
	return qtrue;
}
