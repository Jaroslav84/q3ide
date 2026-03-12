/*
 * q3ide_view_modes_overview.c — Overview (O key) commands + public API.
 * Layout algorithm lives in q3ide_view_modes_overview_layout.c.
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
extern qboolean q3ide_player_axes(vec3_t eye, vec3_t fwd, vec3_t right);

/* ── Overview state ─────────────────────────────────────────────────── */

typedef struct {
	qboolean active;
	vec3_t origin[Q3IDE_MAX_WIN];
	vec3_t normal[Q3IDE_MAX_WIN];
	float world_w[Q3IDE_MAX_WIN];
	float world_h[Q3IDE_MAX_WIN];
} win_snapshot_t;

win_snapshot_t g_ov;
qboolean g_ov_placed;
static q3ide_hotkey_t s_ov_hk = Q3IDE_HOTKEY_INIT;
static qboolean g_ov_held = qfalse; /* true while key is physically held */

/* From layout TU */
extern void q3ide_ov_snapshot_save(win_snapshot_t *s);
extern void q3ide_overview_layout(void);

/* ── Public API ──────────────────────────────────────────────────────── */

qboolean Q3IDE_ViewModes_OverviewActive(void)
{
	return g_ov.active;
}

void q3ide_overview_detach_all(void)
{
	g_ov.active = qfalse;
	g_ov_placed = qfalse;
	Q3IDE_WM_CmdDetachAll();
	Q3IDE_SetHudMsg("OVERVIEW off", Q3IDE_HUD_CONFIRM_MS);
}

/* ── Show / hide ─────────────────────────────────────────────────────── */

static void overview_show(void)
{
	int i, n;
	vec3_t eye, fwd, right, pos, norm;

	if (g_ov.active)
		return;
	if (!q3ide_wm.cap) {
		Q3IDE_LOGE("overview: capture dylib not loaded");
		Q3IDE_SetHudMsg("OVERVIEW: no capture dylib", Q3IDE_HUD_ERROR_MS);
		return;
	}
	if (g_f3_active)
		q3ide_focus3_hide();
	if (!q3ide_player_axes(eye, fwd, right))
		return;

	pos[0] = eye[0] + fwd[0] * Q3IDE_VIEWMODE_ARC_DIST;
	pos[1] = eye[1] + fwd[1] * Q3IDE_VIEWMODE_ARC_DIST;
	pos[2] = eye[2];
	norm[0] = -fwd[0];
	norm[1] = -fwd[1];
	norm[2] = 0.0f;

	Q3IDE_WM_PopulateQueue(qfalse);
	Q3IDE_WM_FlushAllPending(pos, norm);

	n = 0;
	for (i = 0; i < Q3IDE_MAX_WIN; i++)
		if (q3ide_wm.wins[i].active)
			n++;
	if (n == 0) {
		Q3IDE_SetHudMsg("OVERVIEW: no windows", Q3IDE_HUD_STATUS_MS);
		return;
	}

	q3ide_ov_snapshot_save(&g_ov);
	q3ide_overview_layout();
	g_ov_placed = qtrue;
	Q3IDE_SetHudMsg("OVERVIEW", Q3IDE_HUD_STATUS_MS);
	Q3IDE_LOGI("overview: shown, %d windows", n);
}

/* ── Command callbacks ───────────────────────────────────────────────── */

static void cmd_overview_down(void)
{
	qboolean active = g_ov.active || g_ov_placed;
	if (cls.state != CA_ACTIVE) {
		Q3IDE_SetHudMsg("OVERVIEW: not in game", Q3IDE_HUD_STATUS_MS);
		return;
	}
	g_ov_held = qtrue;
	if (q3ide_hk_down(&s_ov_hk, active) == Q3IDE_HK_ACTIVATE) {
		overview_show();
		q3ide_hk_rearm(&s_ov_hk); /* overview_show() does stream init — restart timer */
	} else {
		q3ide_overview_detach_all();
	}
}

static void cmd_overview_up(void)
{
	g_ov_held = qfalse;
	if (q3ide_hk_up(&s_ov_hk, g_ov.active || g_ov_placed) == Q3IDE_HK_DEACTIVATE)
		q3ide_overview_detach_all();
}

void Q3IDE_ViewModesOverview_Init(void)
{
	memset(&g_ov, 0, sizeof(g_ov));
	g_ov_placed = qfalse;
	Cmd_AddCommand("+q3ide_overview", cmd_overview_down);
	Cmd_AddCommand("-q3ide_overview", cmd_overview_up);
}

void Q3IDE_ViewModesOverview_Shutdown(void)
{
	g_ov.active = qfalse;
	g_ov_placed = qfalse;
	Cmd_RemoveCommand("+q3ide_overview");
	Cmd_RemoveCommand("-q3ide_overview");
}

void Q3IDE_Overview_Tick(void)
{
	if (g_ov.active && g_ov_held)
		q3ide_overview_layout();
}
