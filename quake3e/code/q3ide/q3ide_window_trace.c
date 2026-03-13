/* q3ide_window_trace.c — Ray-window intersection and wall trace for placement. */

#include "q3ide_params.h"
#include "q3ide_win_mngr.h"
#include "q3ide_win_mngr_internal.h"
#include "../qcommon/qcommon.h"
#include <math.h>

/* Window basis — q3ide_geometry.c */
extern void q3ide_win_basis(q3ide_win_t *win, vec3_t right, vec3_t up);

int Q3IDE_WM_TraceWindowHit(const vec3_t start, const vec3_t dir, int skip_idx)
{
	/*
	 * Ray-plane intersection using the same right/up basis as q3ide_win_basis()
	 * in q3ide_geometry.c.  Using identical axes makes hit detection pixel-perfect
	 * and consistent from both sides of a double-sided window.
	 *
	 * Basis: right = XY-perp of normal, up = world Z.
	 * Works for walls (normal mostly horizontal) and is intentionally simple.
	 */
	int i, best = -1;
	float best_t = 1e9f; /* no distance limit — highlight works from any range */

	for (i = 0; i < Q3IDE_MAX_WIN; i++) {
		q3ide_win_t *win = &q3ide_wm.wins[i];
		vec3_t diff, hit;
		float denom, t, hw, hh, lx, ly, nx, ny, horiz_len;
		float right[3], up[3];

		if (!win->active)
			continue;
		if (i == skip_idx)
			continue;
		/* Only hit windows that are actually rendered — same gate as AddPolys.
		 * Prevents aiming at invisible (LOS-occluded) windows. */
		if (!win->los_visible && !win->in_overview)
			continue;

		/* Use arc position when in overview, wall position otherwise. */
		{
			const float *o = win->in_overview ? win->ov_origin : win->origin;
			const float *n = win->in_overview ? win->ov_normal : win->normal;

			denom = DotProduct(dir, n);
			/* Double-sided: accept ray from either side (|denom| check only). */
			if (fabsf(denom) < Q3IDE_TRACE_PLANE_EPS)
				continue;

			VectorSubtract(o, start, diff);
			t = DotProduct(diff, n) / denom;
			if (t < 0.0f || t >= best_t)
				continue;

			hit[0] = start[0] + dir[0] * t;
			hit[1] = start[1] + dir[1] * t;
			hit[2] = start[2] + dir[2] * t;

			/* Same basis as q3ide_win_basis() — XY projection of normal. */
			nx = n[0];
			ny = n[1];
			horiz_len = sqrtf(nx * nx + ny * ny);
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

			VectorSubtract(hit, o, diff);
			lx = DotProduct(diff, right);
			ly = DotProduct(diff, up);
			hw = (win->in_overview ? win->base_world_w : win->world_w) * 0.5f;
			hh = (win->in_overview ? win->base_world_h : win->world_h) * 0.5f;

			if (fabsf(lx) <= hw && fabsf(ly) <= hh) {
				best_t = t;
				best = i;
			}
		}
	}
	return best;
}


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
	/* Reject floors, ceilings, and surfaces tilted more than 30° from vertical */
	if (fabsf(tr.plane.normal[2]) > Q3IDE_WALL_MAX_TILT_Z)
		return qfalse;
	out_pos[0] = tr.endpos[0] + tr.plane.normal[0] * Q3IDE_WALL_OFFSET;
	out_pos[1] = tr.endpos[1] + tr.plane.normal[1] * Q3IDE_WALL_OFFSET;
	out_pos[2] = tr.endpos[2] + tr.plane.normal[2] * Q3IDE_WALL_OFFSET;
	VectorCopy(tr.plane.normal, out_normal);
	return qtrue;
}


/*
 * q3ide_clamp_corners_to_walls — shift win->origin in the wall plane so no
 * corner clips into solid geometry.
 *
 * For each of the 4 corners: if the corner starts inside solid, trace from
 * win->origin (guaranteed clear — it was just placed on a wall surface + offset)
 * toward that corner to find the exact wall face it crossed.  This keeps the
 * probe short and local — no risk of hitting a distant wall on the other side
 * of the map.  The largest-magnitude shift per axis is applied to win->origin.
 */
void q3ide_clamp_corners_to_walls(q3ide_win_t *win)
{
	/* BL, BR, TR, TL — right/up multipliers for each corner */
	static const float sx[4] = {-1.0f, 1.0f, 1.0f, -1.0f};
	static const float sy[4] = {-1.0f, -1.0f, 1.0f, 1.0f};
	vec3_t right, up;
	float hw, hh, shift_r, shift_u;
	int i;

	q3ide_win_basis(win, right, up);
	hw = win->world_w * 0.5f;
	hh = win->world_h * 0.5f;
	shift_r = 0.0f;
	shift_u = 0.0f;

	for (i = 0; i < 4; i++) {
		trace_t chk, probe;
		vec3_t corner, end, mins, maxs, delta;
		float dr, du;

		VectorSet(mins, 0, 0, 0);
		VectorSet(maxs, 0, 0, 0);
		corner[0] = win->origin[0] + right[0] * sx[i] * hw + up[0] * sy[i] * hh;
		corner[1] = win->origin[1] + right[1] * sx[i] * hw + up[1] * sy[i] * hh;
		corner[2] = win->origin[2] + right[2] * sx[i] * hw + up[2] * sy[i] * hh;

		CM_BoxTrace(&chk, corner, corner, mins, maxs, 0, CONTENTS_SOLID, qfalse);
		if (!chk.startsolid)
			continue;

		/*
		 * Trace from the clear origin toward the corner (10% past it so the
		 * trace endpoint is clearly behind the wall face).  This finds the
		 * exact local wall face the corner crossed — never a distant wall.
		 */
		end[0] = corner[0] + (corner[0] - win->origin[0]) * 0.1f;
		end[1] = corner[1] + (corner[1] - win->origin[1]) * 0.1f;
		end[2] = corner[2] + (corner[2] - win->origin[2]) * 0.1f;

		CM_BoxTrace(&probe, win->origin, end, mins, maxs, 0, CONTENTS_SOLID, qfalse);
		if (probe.fraction >= 1.0f || probe.startsolid)
			continue;

		/* Move corner to just outside the wall face, keep shift largest per axis */
		delta[0] = probe.endpos[0] + probe.plane.normal[0] * Q3IDE_WALL_OFFSET - corner[0];
		delta[1] = probe.endpos[1] + probe.plane.normal[1] * Q3IDE_WALL_OFFSET - corner[1];
		delta[2] = probe.endpos[2] + probe.plane.normal[2] * Q3IDE_WALL_OFFSET - corner[2];
		dr = DotProduct(delta, right);
		du = DotProduct(delta, up);
		if (fabsf(dr) > fabsf(shift_r))
			shift_r = dr;
		if (fabsf(du) > fabsf(shift_u))
			shift_u = du;
	}

	win->origin[0] += right[0] * shift_r + up[0] * shift_u;
	win->origin[1] += right[1] * shift_r + up[1] * shift_u;
	win->origin[2] += right[2] * shift_r + up[2] * shift_u;
}
