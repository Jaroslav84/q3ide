/*
 * q3ide_spawn.c — On-respawn terminal focus: float nearest terminal window
 * in front of the player after respawn.
 * Per-frame tick: q3ide_frame.c.
 */

#include "q3ide_log.h"
#include "q3ide_wm.h"
#include "q3ide_wm_internal.h"
#include "../qcommon/qcommon.h"
#include "../client/client.h"
#include <math.h>

/* Defined in q3ide_cmd_query.c */
extern const char *q3ide_terminal_apps[];
extern qboolean q3ide_match(const char *app, const char **list);

#define Q3IDE_SPAWN_WIN_W    100.0f /* world width of spawn-focus window, units */
#define Q3IDE_SPAWN_WIN_DIST 200.0f /* distance ahead of player eye */

void q3ide_spawn_focus_terminal(const vec3_t eye)
{
	int i, found = -1;
	float yaw_rad, aspect;
	vec3_t fwd, pos, norm;
	q3ide_win_t *win;

	if (!q3ide_wm.num_active)
		return;

	/* Prefer a terminal window; fall back to first active tunnel window. */
	for (i = 0; i < Q3IDE_MAX_WIN; i++) {
		if (!q3ide_wm.wins[i].active || !q3ide_wm.wins[i].is_tunnel)
			continue;
		if (found < 0)
			found = i;
		if (q3ide_match(q3ide_wm.wins[i].label, q3ide_terminal_apps)) {
			found = i;
			break;
		}
	}
	if (found < 0)
		return;

	win = &q3ide_wm.wins[found];

	if (win->tex_w > 0 && win->tex_h > 0)
		aspect = (float) win->tex_w / (float) win->tex_h;
	else if (win->world_h > 0.001f)
		aspect = win->world_w / win->world_h;
	else
		aspect = 16.0f / 9.0f;

	yaw_rad = cl.snap.ps.viewangles[YAW] * (float) M_PI / 180.0f;
	fwd[0] = cosf(yaw_rad);
	fwd[1] = sinf(yaw_rad);
	fwd[2] = 0.0f;

	pos[0] = eye[0] + fwd[0] * Q3IDE_SPAWN_WIN_DIST;
	pos[1] = eye[1] + fwd[1] * Q3IDE_SPAWN_WIN_DIST;
	pos[2] = eye[2];
	norm[0] = -fwd[0];
	norm[1] = -fwd[1];
	norm[2] = 0.0f;

	win->world_w = Q3IDE_SPAWN_WIN_W;
	win->world_h = Q3IDE_SPAWN_WIN_W / aspect;
	win->wall_mounted = qfalse;
	Q3IDE_WM_MoveWindow(found, pos, norm, qtrue);
	Q3IDE_LOGI("spawn focus: id=%u size=%.0fx%.0f", win->capture_id, win->world_w, win->world_h);
}
