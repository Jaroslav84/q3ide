/*
 * q3ide_view_modes_overview.c — Overview (O key) commands + public API.
 * Layout algorithm lives in q3ide_view_modes_grid.c.
 */

#include "q3ide_view_modes.h"
#include "../client/client.h"
#include "../qcommon/qcommon.h"
#include "q3ide_engine_hooks.h"
#include "q3ide_hotkey.h"
#include "q3ide_log.h"
#include "q3ide_params.h"
#include "q3ide_win_mngr.h"
#include "q3ide_win_mngr_internal.h"
#include <string.h>

/* Shared state — also extern'd in monitors.c */
extern qboolean g_f3_active;
extern void q3ide_focus3_hide(void);

/* ── Overview state ─────────────────────────────────────────────────── */

qboolean g_ov_active = qfalse;    /* true while overview is showing */
qboolean g_ov_placed = qfalse;    /* true once layout has run */
float g_ov_scroll_z = 0.0f;       /* world-unit vertical shift for the O grid */
qboolean g_ov_scrolling = qfalse; /* true shortly after a scroll tick */
unsigned long long g_ov_last_scroll_ms = 0ULL;
static q3ide_hotkey_t s_ov_hk = Q3IDE_HOTKEY_INIT;
static qboolean g_ov_held = qfalse;

/* From layout TU */
extern void q3ide_overview_layout(void);
extern qboolean q3ide_player_axes(vec3_t eye, vec3_t fwd, vec3_t right);

/* ── Public API ──────────────────────────────────────────────────────── */

qboolean Q3IDE_ViewModes_OverviewActive(void)
{
	return g_ov_active;
}

void q3ide_overview_detach_all(void)
{
	int i;
	/* Wall-placed windows survive: clear in_overview flag, leave them on walls.
	 * Arc-only windows (wall_placed=false) are torn down — stop stream, free slot. */
	for (i = 0; i < Q3IDE_MAX_WIN; i++) {
		q3ide_win_t *w = &q3ide_wm.wins[i];
		if (!w->active)
			continue;
		w->in_overview = qfalse;
		if (!w->wall_placed) {
			if (q3ide_wm.cap_stop && w->owns_stream)
				q3ide_wm.cap_stop(q3ide_wm.cap, w->capture_id);
			memset(w, 0, sizeof(q3ide_win_t));
			q3ide_wm.num_active--;
		}
	}
	g_ov_active = qfalse;
	g_ov_placed = qfalse;
	g_ov_scroll_z = 0.0f;
	g_ov_scrolling = qfalse;
	s_ov_hk.locked = qfalse;
	Q3IDE_SetHudMsg("OVERVIEW off", Q3IDE_HUD_CONFIRM_MS);
}

/* ── Show / hide ─────────────────────────────────────────────────────── */

static void overview_show(void)
{
	int i, n;
	qboolean was_active[Q3IDE_MAX_WIN];

	if (g_ov_active)
		return;
	if (!q3ide_wm.cap) {
		Q3IDE_LOGE("overview: capture dylib not loaded");
		Q3IDE_SetHudMsg("OVERVIEW: no capture dylib", Q3IDE_HUD_ERROR_MS);
		return;
	}
	if (g_f3_active)
		q3ide_focus3_hide();

	/* Snapshot which windows already exist before we add overview-only ones. */
	for (i = 0; i < Q3IDE_MAX_WIN; i++)
		was_active[i] = q3ide_wm.wins[i].active;

	/* Attach every macOS window not already active — overview shows everything. */
	Q3IDE_WM_PopulateQueue(qfalse);
	if (Q3IDE_WM_PendingCount() > 0) {
		vec3_t eye, fwd, dummy_pos, dummy_norm;
		if (q3ide_player_axes(eye, fwd, dummy_norm)) {
			dummy_pos[0] = eye[0] + fwd[0] * Q3IDE_SPAWN_WIN_DIST;
			dummy_pos[1] = eye[1] + fwd[1] * Q3IDE_SPAWN_WIN_DIST;
			dummy_pos[2] = eye[2];
			dummy_norm[0] = -fwd[0];
			dummy_norm[1] = -fwd[1];
			dummy_norm[2] = 0.0f;
			Q3IDE_WM_FlushAllPending(dummy_pos, dummy_norm);
		}
	}

	/* Windows added by the flush above are arc-only: clear their wall_placed flag.
	 * Pre-existing wall-placed windows keep wall_placed=true set by Q3IDE_WM_Attach. */
	for (i = 0; i < Q3IDE_MAX_WIN; i++) {
		if (q3ide_wm.wins[i].active && !was_active[i])
			q3ide_wm.wins[i].wall_placed = qfalse;
	}

	n = 0;
	for (i = 0; i < Q3IDE_MAX_WIN; i++)
		if (q3ide_wm.wins[i].active)
			n++;
	if (n == 0) {
		Q3IDE_SetHudMsg("OVERVIEW: no windows", Q3IDE_HUD_STATUS_MS);
		return;
	}

	/* Mark all active windows as in_overview so layout places them in the arc. */
	for (i = 0; i < Q3IDE_MAX_WIN; i++)
		if (q3ide_wm.wins[i].active)
			q3ide_wm.wins[i].in_overview = qtrue;

	g_ov_active = qtrue;
	q3ide_overview_layout();
	g_ov_placed = qtrue;
	Q3IDE_SetHudMsg("OVERVIEW", Q3IDE_HUD_STATUS_MS);
	Q3IDE_LOGI("overview: shown, %d windows", n);
}

/* ── Command callbacks ───────────────────────────────────────────────── */

static void cmd_overview_down(void)
{
	qboolean active;
	q3ide_hk_result_t res;
	if (g_ov_held)
		return; /* key repeat — ignore */
	active = g_ov_active || g_ov_placed || s_ov_hk.locked;
	if (cls.state != CA_ACTIVE) {
		Q3IDE_SetHudMsg("OVERVIEW: not in game", Q3IDE_HUD_STATUS_MS);
		return;
	}
	g_ov_held = qtrue;
	res = q3ide_hk_down(&s_ov_hk, active);
	if (res == Q3IDE_HK_ACTIVATE) {
		overview_show();
		q3ide_hk_rearm(&s_ov_hk);
	} else {
		q3ide_overview_detach_all();
	}
}

static void cmd_overview_up(void)
{
	qboolean is_on;
	q3ide_hk_result_t res;
	g_ov_held = qfalse;
	is_on = g_ov_active || g_ov_placed;
	res = q3ide_hk_up(&s_ov_hk, is_on);
	if (res == Q3IDE_HK_DEACTIVATE)
		q3ide_overview_detach_all();
}

void Q3IDE_ViewModesOverview_Init(void)
{
	g_ov_active = qfalse;
	g_ov_placed = qfalse;
	Cmd_AddCommand("+q3ide_overview", cmd_overview_down);
	Cmd_AddCommand("-q3ide_overview", cmd_overview_up);
}

void Q3IDE_ViewModesOverview_Shutdown(void)
{
	g_ov_active = qfalse;
	g_ov_placed = qfalse;
	Cmd_RemoveCommand("+q3ide_overview");
	Cmd_RemoveCommand("-q3ide_overview");
}

void Q3IDE_Overview_Tick(void)
{
	unsigned long long now;
	if (!g_ov_active)
		return;
	if (g_ov_scrolling) {
		now = (unsigned long long) Sys_Milliseconds();
		if (now - g_ov_last_scroll_ms < Q3IDE_OVERVIEW_SCROLL_IDLE_MS)
			return; /* user still scrolling — don't readjust */
		g_ov_scrolling = qfalse;
	}
	if (g_ov_held)
		q3ide_overview_layout();
}
