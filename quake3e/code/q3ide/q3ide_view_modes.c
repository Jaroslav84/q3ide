/*
 * q3ide_view_modes.c — Overview (O) and Focus3 (I) view layouts.
 *
 * Overview: hold O to arrange all attached windows into a 3×N grid in
 *   front of the player; release O to snap every window back exactly
 *   where it was.
 *
 * Focus3: press I to keep the first 3 windows and lay them out as a
 *   connected triple-monitor strip (touching edges, no gap) floating
 *   in front of the player.
 */

#include "q3ide_view_modes.h"
#include "q3ide_engine_hooks.h"
#include "q3ide_win_mngr.h"
#include "q3ide_win_mngr_internal.h"
#include "q3ide_params.h"
#include "q3ide_log.h"
#include "../qcommon/qcommon.h"
#include "../client/client.h"
#include <math.h>
#include <string.h>

/* ── Saved snapshot for overview restore ────────────────────────────── */

static struct {
	qboolean active;
	vec3_t   origin[Q3IDE_MAX_WIN];
	vec3_t   normal[Q3IDE_MAX_WIN];
	float    world_w[Q3IDE_MAX_WIN];
	float    world_h[Q3IDE_MAX_WIN];
	qboolean wall_mounted[Q3IDE_MAX_WIN];
} g_ov;

qboolean Q3IDE_ViewModes_OverviewActive(void)
{
	return g_ov.active;
}

/* ── Shared helper: player eye, flat-forward, right vectors ─────────── */

static qboolean player_axes(vec3_t eye, vec3_t fwd, vec3_t right)
{
	float p, y, hlen;

	if (cls.state != CA_ACTIVE)
		return qfalse;

	VectorCopy(cl.snap.ps.origin, eye);
	eye[2] += cl.snap.ps.viewheight;

	p      = cl.snap.ps.viewangles[PITCH] * ((float)M_PI / 180.0f);
	y      = cl.snap.ps.viewangles[YAW] * ((float)M_PI / 180.0f);
	fwd[0] = cosf(p) * cosf(y);
	fwd[1] = cosf(p) * sinf(y);
	fwd[2] = 0.0f; /* flatten — panels sit on the horizontal plane */

	hlen = sqrtf(fwd[0] * fwd[0] + fwd[1] * fwd[1]);
	if (hlen < 0.001f)
		return qfalse;
	fwd[0] /= hlen;
	fwd[1] /= hlen;

	/* right = (fwd.y, -fwd.x, 0) — player's rightward direction */
	right[0] = fwd[1];
	right[1] = -fwd[0];
	right[2] = 0.0f;

	return qtrue;
}

/* ── Overview ────────────────────────────────────────────────────────── */

static void overview_enter(void)
{
	int    i, k, n, col, row, cols;
	int    active_idx[Q3IDE_MAX_WIN];
	float  cell_w, cell_h, gap, dist, ox, oy;
	vec3_t eye, fwd, right, center, norm, pos;

	if (g_ov.active)
		return;

	/* Save current layout of every slot (active or not) */
	for (i = 0; i < Q3IDE_MAX_WIN; i++) {
		VectorCopy(q3ide_wm.wins[i].origin, g_ov.origin[i]);
		VectorCopy(q3ide_wm.wins[i].normal, g_ov.normal[i]);
		g_ov.world_w[i]      = q3ide_wm.wins[i].world_w;
		g_ov.world_h[i]      = q3ide_wm.wins[i].world_h;
		g_ov.wall_mounted[i] = q3ide_wm.wins[i].wall_mounted;
	}
	g_ov.active = qtrue;

	n = 0;
	for (i = 0; i < Q3IDE_MAX_WIN; i++)
		if (q3ide_wm.wins[i].active)
			active_idx[n++] = i;

	if (n == 0 || !player_axes(eye, fwd, right)) {
		Q3IDE_SetHudMsg("OVERVIEW — no windows", 1500);
		return;
	}

	cols   = Q3IDE_OVERVIEW_COLS;
	cell_w = Q3IDE_OVERVIEW_CELL_W;
	cell_h = Q3IDE_OVERVIEW_CELL_H;
	gap    = Q3IDE_OVERVIEW_GAP;
	dist   = Q3IDE_OVERVIEW_DIST;

	/* Wall anchored at floor level — row 0 at bottom, rows ascend upward */
	center[0] = eye[0] + fwd[0] * dist;
	center[1] = eye[1] + fwd[1] * dist;

	norm[0] = -fwd[0];
	norm[1] = -fwd[1];
	norm[2] = 0.0f;

	for (k = 0; k < n; k++) {
		col = k % cols;
		row = k / cols;
		ox = ((float)col - (float)(cols - 1) * 0.5f) * (cell_w + gap);
		oy = (float)row * (cell_h + gap) + cell_h * 0.5f;

		pos[0] = center[0] + right[0] * ox;
		pos[1] = center[1] + right[1] * ox;
		pos[2] = cl.snap.ps.origin[2] + Q3IDE_OVERVIEW_FLOOR_LIFT + oy;

		q3ide_wm.wins[active_idx[k]].world_w     = cell_w;
		q3ide_wm.wins[active_idx[k]].world_h     = cell_h;
		q3ide_wm.wins[active_idx[k]].wall_mounted = qfalse;
		Q3IDE_WM_MoveWindow(active_idx[k], pos, norm, qtrue);
	}

	Q3IDE_SetHudMsg("OVERVIEW", 1500);
	Q3IDE_LOGI("overview: %d wins %d cols dist=%.0f", n, cols, dist);
}

static void overview_exit(void)
{
	int i;

	if (!g_ov.active)
		return;

	/* Restore only slots that had a saved size (were active at entry) */
	for (i = 0; i < Q3IDE_MAX_WIN; i++) {
		if (!q3ide_wm.wins[i].active || g_ov.world_w[i] <= 0.0f)
			continue;
		q3ide_wm.wins[i].world_w     = g_ov.world_w[i];
		q3ide_wm.wins[i].world_h     = g_ov.world_h[i];
		q3ide_wm.wins[i].wall_mounted = g_ov.wall_mounted[i];
		Q3IDE_WM_MoveWindow(i, g_ov.origin[i], g_ov.normal[i], qtrue);
	}
	g_ov.active = qfalse;
	Q3IDE_LOGI("overview: restored");
}

/* ── Focus3 ──────────────────────────────────────────────────────────── */

static void focus3_enter(void)
{
	int    active_idx[Q3IDE_MAX_WIN];
	int    n, keep, k, i;
	float  h, widths[3], total_w, cur_x, asp;
	vec3_t eye, fwd, right, center, norm, pos;

	if (g_ov.active)
		overview_exit();

	n = 0;
	for (i = 0; i < Q3IDE_MAX_WIN; i++)
		if (q3ide_wm.wins[i].active)
			active_idx[n++] = i;

	if (n == 0 || !player_axes(eye, fwd, right)) {
		Q3IDE_SetHudMsg("FOCUS3 — no windows", 1500);
		return;
	}

	keep = (n < 3) ? n : 3;

	/* Detach all windows beyond the first three */
	for (i = keep; i < n; i++)
		Q3IDE_WM_DetachById(q3ide_wm.wins[active_idx[i]].capture_id);

	/* Panel heights are uniform; widths derived from each window's aspect ratio */
	h       = Q3IDE_FOCUS3_HEIGHT;
	total_w = 0.0f;
	for (k = 0; k < keep; k++) {
		asp       = (q3ide_wm.wins[active_idx[k]].world_h > 1.0f)
		                ? q3ide_wm.wins[active_idx[k]].world_w / q3ide_wm.wins[active_idx[k]].world_h
		                : (16.0f / 9.0f);
		widths[k] = h * asp;
		total_w += widths[k];
	}

	center[0] = eye[0] + fwd[0] * Q3IDE_FOCUS3_DIST;
	center[1] = eye[1] + fwd[1] * Q3IDE_FOCUS3_DIST;
	center[2] = eye[2];

	norm[0] = -fwd[0];
	norm[1] = -fwd[1];
	norm[2] = 0.0f;

	/* Lay panels left-to-right, sharing edges (no gap = connected monitors) */
	cur_x = -total_w * 0.5f;
	for (k = 0; k < keep; k++) {
		float cx = cur_x + widths[k] * 0.5f;

		pos[0] = center[0] + right[0] * cx;
		pos[1] = center[1] + right[1] * cx;
		pos[2] = center[2];

		q3ide_wm.wins[active_idx[k]].world_w     = widths[k];
		q3ide_wm.wins[active_idx[k]].world_h     = h;
		q3ide_wm.wins[active_idx[k]].wall_mounted = qfalse;
		Q3IDE_WM_MoveWindow(active_idx[k], pos, norm, qtrue);

		cur_x += widths[k]; /* next panel shares this panel's right edge */
	}

	Q3IDE_SetHudMsg("FOCUS 3", 1500);
	Q3IDE_LOGI("focus3: %d panels w=%.0f h=%.0f dist=%.0f", keep, total_w, h, Q3IDE_FOCUS3_DIST);
}

/* ── Command callbacks (bound via autoexec: bind o "+q3ide_overview") ── */

static void cmd_overview_down(void)
{
	overview_enter();
}

static void cmd_overview_up(void)
{
	overview_exit();
}

static void cmd_focus3(void)
{
	focus3_enter();
}

/* ── Public API ──────────────────────────────────────────────────────── */

void Q3IDE_ViewModes_Init(void)
{
	memset(&g_ov, 0, sizeof(g_ov));
	Cmd_AddCommand("+q3ide_overview", cmd_overview_down);
	Cmd_AddCommand("-q3ide_overview", cmd_overview_up);
	Cmd_AddCommand("q3ide_focus3", cmd_focus3);
}

void Q3IDE_ViewModes_Shutdown(void)
{
	g_ov.active = qfalse;
	Cmd_RemoveCommand("+q3ide_overview");
	Cmd_RemoveCommand("-q3ide_overview");
	Cmd_RemoveCommand("q3ide_focus3");
}
