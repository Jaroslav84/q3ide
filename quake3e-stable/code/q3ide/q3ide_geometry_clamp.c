/*
 * q3ide_geometry_clamp.c — Window size clamping.
 * Ray-window hit + wall trace: q3ide_window_trace.c.
 */

#include "q3ide_params.h"
#include "q3ide_win_mngr.h"
#include "q3ide_win_mngr_internal.h"
#include "../qcommon/qcommon.h"
#include <math.h>

/* Shared basis — defined in q3ide_geometry.c */
extern void q3ide_win_basis(q3ide_win_t *win, vec3_t right, vec3_t up);

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

	q3ide_win_basis(win, right, up);

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
		lim = tr.fraction * hw - Q3IDE_CLAMP_WALL_GAP;
		if (lim < Q3IDE_CLAMP_WALL_GAP)
			lim = Q3IDE_CLAMP_WALL_GAP;
		if (lim < hw)
			hw = lim;
	}

	/* -right — always trace orig_hw so previous constraint doesn't compound */
	end[0] = start[0] - right[0] * orig_hw;
	end[1] = start[1] - right[1] * orig_hw;
	end[2] = start[2] - right[2] * orig_hw;
	CM_BoxTrace(&tr, start, end, mins, maxs, 0, CONTENTS_SOLID, qfalse);
	if (tr.fraction < 1.0f) {
		lim = tr.fraction * orig_hw - Q3IDE_CLAMP_WALL_GAP;
		if (lim < Q3IDE_CLAMP_WALL_GAP)
			lim = Q3IDE_CLAMP_WALL_GAP;
		if (lim < hw)
			hw = lim;
	}

	/* +up — always trace orig_hh */
	end[0] = start[0] + up[0] * orig_hh;
	end[1] = start[1] + up[1] * orig_hh;
	end[2] = start[2] + up[2] * orig_hh;
	CM_BoxTrace(&tr, start, end, mins, maxs, 0, CONTENTS_SOLID, qfalse);
	if (tr.fraction < 1.0f) {
		lim = tr.fraction * orig_hh - Q3IDE_CLAMP_WALL_GAP;
		if (lim < Q3IDE_CLAMP_WALL_GAP)
			lim = Q3IDE_CLAMP_WALL_GAP;
		if (lim < hh)
			hh = lim;
	}

	/* -up — always trace orig_hh */
	end[0] = start[0] - up[0] * orig_hh;
	end[1] = start[1] - up[1] * orig_hh;
	end[2] = start[2] - up[2] * orig_hh;
	CM_BoxTrace(&tr, start, end, mins, maxs, 0, CONTENTS_SOLID, qfalse);
	if (tr.fraction < 1.0f) {
		lim = tr.fraction * orig_hh - Q3IDE_CLAMP_WALL_GAP;
		if (lim < Q3IDE_CLAMP_WALL_GAP)
			lim = Q3IDE_CLAMP_WALL_GAP;
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
		if (win->world_w > 0.0f && win->world_w < Q3IDE_CLAMP_MIN_SIZE)
			min_scale = Q3IDE_CLAMP_MIN_SIZE / win->world_w;
		if (win->world_h > 0.0f && win->world_h < Q3IDE_CLAMP_MIN_SIZE) {
			float ms = Q3IDE_CLAMP_MIN_SIZE / win->world_h;
			if (ms > min_scale)
				min_scale = ms;
		}
		if (min_scale > 1.0f) {
			win->world_w *= min_scale;
			win->world_h *= min_scale;
		}
	}
}
