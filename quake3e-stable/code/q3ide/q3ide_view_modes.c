/*
 * q3ide_view_modes.c — Shared state and lifecycle for Overview (O) and Focus3 (I).
 *
 * Overview layout: q3ide_view_modes_overview.c
 * Monitor layout:  q3ide_view_modes_monitors.c
 */

#include "q3ide_view_modes.h"
#include "../client/client.h"
#include "../qcommon/qcommon.h"
#include "q3ide_log.h"
#include "q3ide_params.h"
#include "q3ide_win_mngr.h"
#include "q3ide_win_mngr_internal.h"
#include <math.h>
#include <string.h>

/* ── Shared state (extern in focus3.c and overview.c) ───────────────── */

qboolean g_f3_active; /* Monitors active — extern in monitors.c */

/* ── Shared helper: player eye, flat-forward, right vectors ─────────── */

qboolean q3ide_player_axes(vec3_t eye, vec3_t fwd, vec3_t right)
{
	float p, y, hlen;

	if (cls.state != CA_ACTIVE)
		return qfalse;

	VectorCopy(cl.snap.ps.origin, eye);
	eye[2] += cl.snap.ps.viewheight;

	p = cl.snap.ps.viewangles[PITCH] * ((float) M_PI / 180.0f);
	y = cl.snap.ps.viewangles[YAW] * ((float) M_PI / 180.0f);
	fwd[0] = cosf(p) * cosf(y);
	fwd[1] = cosf(p) * sinf(y);
	fwd[2] = 0.0f;

	hlen = sqrtf(fwd[0] * fwd[0] + fwd[1] * fwd[1]);
	if (hlen < 0.001f)
		return qfalse;
	fwd[0] /= hlen;
	fwd[1] /= hlen;

	right[0] = fwd[1];
	right[1] = -fwd[0];
	right[2] = 0.0f;

	return qtrue;
}

/* ── Public state queries ────────────────────────────────────────────── */

qboolean Q3IDE_ViewModes_Focus3Active(void)
{
	return g_f3_active;
}

/* ── Lifecycle ───────────────────────────────────────────────────────── */

/* Overview — q3ide_view_modes_overview.c */
extern void Q3IDE_ViewModesOverview_Init(void);
extern void Q3IDE_ViewModesOverview_Shutdown(void);
extern void Q3IDE_Overview_Tick(void);

/* Monitors — q3ide_view_modes_monitors.c */
extern void q3ide_cmd_focus3_down(void);
extern void q3ide_cmd_focus3_up(void);
extern void q3ide_focus3_show(void);
extern void q3ide_focus3_tick(void);
extern unsigned long long g_f3_retry_at;

void Q3IDE_ViewModes_Init(void)
{
	g_f3_active = qfalse;
	Q3IDE_ViewModesOverview_Init();
	Cmd_AddCommand("+q3ide_focus3", q3ide_cmd_focus3_down);
	Cmd_AddCommand("-q3ide_focus3", q3ide_cmd_focus3_up);
}

void Q3IDE_ViewModes_Shutdown(void)
{
	g_f3_active = qfalse;
	Q3IDE_ViewModesOverview_Shutdown();
	Cmd_RemoveCommand("+q3ide_focus3");
	Cmd_RemoveCommand("-q3ide_focus3");
}

void Q3IDE_ViewModes_Tick(void)
{
	Q3IDE_Overview_Tick();
	q3ide_focus3_tick();
	if (g_f3_retry_at && !g_f3_active && Sys_Milliseconds() >= g_f3_retry_at) {
		g_f3_retry_at = 0;
		q3ide_focus3_show();
	}
}
