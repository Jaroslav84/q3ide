/*
 * q3ide_view_modes_arc.c — Seamless edge-touching arc placement helper.
 * Used by both Focus3 (I) and Overview (O) row layout.
 */

#include "q3ide_params.h"
#include "q3ide_win_mngr.h"
#include "q3ide_win_mngr_internal.h"
#include "../qcommon/qcommon.h"
#include <math.h>

/* ── Forward trace: available depth before hitting a wall ───────────── */

static float trace_arc_dist(vec3_t eye, vec3_t fwd, float ideal_dist)
{
	vec3_t end, mins, maxs;
	trace_t tr;
	float d;

	end[0] = eye[0] + fwd[0] * ideal_dist;
	end[1] = eye[1] + fwd[1] * ideal_dist;
	end[2] = eye[2]; /* horizontal trace — ignore vertical pitch */

	VectorSet(mins, -Q3IDE_TRACE_BOX_HALF, -Q3IDE_TRACE_BOX_HALF, -Q3IDE_TRACE_BOX_HALF);
	VectorSet(maxs, Q3IDE_TRACE_BOX_HALF, Q3IDE_TRACE_BOX_HALF, Q3IDE_TRACE_BOX_HALF);
	CM_BoxTrace(&tr, eye, end, mins, maxs, 0, CONTENTS_SOLID, qfalse);

	d = tr.fraction * ideal_dist - Q3IDE_WALL_OFFSET;
	if (d < Q3IDE_FOCUS3_MIN_DIST)
		d = Q3IDE_FOCUS3_MIN_DIST;
	return d;
}

/* ── Focus3 arc placement ────────────────────────────────────────────── */

/*
 * Place up to 3 windows as a seamless edge-touching arc.
 * Traces forward to find available depth; scales window sizes proportionally
 * if a wall is closer than Q3IDE_VIEWMODE_ARC_DIST (aspect ratio preserved).
 * pitch_rad: vertical tilt (0=flat row). idxs[0]=center, [1]=left, [2]=right.
 */
void q3ide_focus3_place_arc(vec3_t eye, vec3_t fwd, int *idxs, int n, float pitch_rad)
{
	float R, Rh, Rv, cp, sp, scale;
	float a, b, theta, rad, c, s;
	vec3_t dir, pos, norm;
	int k;

	if (n == 0)
		return;

	/* Shrink arc radius to fit available space (sizes already scaled by caller) */
	R = trace_arc_dist(eye, fwd, Q3IDE_VIEWMODE_ARC_DIST);
	scale = R / Q3IDE_VIEWMODE_ARC_DIST;
	if (scale > 1.0f)
		scale = 1.0f;

	cp = cosf(pitch_rad);
	sp = sinf(pitch_rad);
	Rh = R * cp;
	Rv = R * sp;

	/* k=0: center panel straight ahead */
	pos[0] = eye[0] + fwd[0] * Rh;
	pos[1] = eye[1] + fwd[1] * Rh;
	pos[2] = eye[2] + Rv;
	norm[0] = -fwd[0] * cp;
	norm[1] = -fwd[1] * cp;
	norm[2] = -sp;
	Q3IDE_WM_MoveWindow(idxs[0], pos, norm, qtrue);

	a = q3ide_wm.wins[idxs[0]].world_w * 0.5f;

	/* k=1 left (−θ), k=2 right (+θ) — same pitch, rotated horizontally */
	for (k = 1; k < n && k < 3; k++) {
		b = q3ide_wm.wins[idxs[k]].world_w * 0.5f;
		if (Rh > 0.001f)
			theta = atan2f(b, Rh) + asinf(a / sqrtf(Rh * Rh + b * b));
		else
			theta = (float) M_PI * 0.5f;
		rad = (k == 1) ? -theta : theta;
		c = cosf(rad);
		s = sinf(rad);
		dir[0] = fwd[0] * c - fwd[1] * s;
		dir[1] = fwd[0] * s + fwd[1] * c;
		pos[0] = eye[0] + dir[0] * Rh;
		pos[1] = eye[1] + dir[1] * Rh;
		pos[2] = eye[2] + Rv;
		norm[0] = -dir[0] * cp;
		norm[1] = -dir[1] * cp;
		norm[2] = -sp;
		Q3IDE_WM_MoveWindow(idxs[k], pos, norm, qtrue);
	}
}
