/*
 * q3ide_spawn.c — On-respawn focus: float most-active terminal in front of
 * player. All other windows stack at the same position.
 */

#include "q3ide_log.h"
#include "q3ide_win_mngr.h"
#include "q3ide_win_mngr_internal.h"
#include "../qcommon/qcommon.h"
#include "../client/client.h"
#include <math.h>

/* Defined in q3ide_commands_query.c */
extern const char *q3ide_terminal_apps[];
extern qboolean q3ide_match(const char *app, const char **list);

void q3ide_spawn_focus_terminal(const vec3_t eye)
{
	int i, terminal = -1;
	float yaw_rad, aspect;
	vec3_t fwd, pos, norm;

	if (!q3ide_wm.num_active)
		return;

	/* Compute spawn position: 200u in front of player. */
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

	/* Find most-recently-active terminal (highest frame count). */
	{
		unsigned long long best_frames = 0;
		for (i = 0; i < Q3IDE_MAX_WIN; i++) {
			q3ide_win_t *w = &q3ide_wm.wins[i];
			if (!w->active || !w->is_tunnel)
				continue;
			if (q3ide_match(w->label, q3ide_terminal_apps) && w->frames >= best_frames) {
				best_frames = w->frames;
				terminal = i;
			}
		}
		/* Fall back to first active tunnel if no terminal found. */
		if (terminal < 0) {
			for (i = 0; i < Q3IDE_MAX_WIN; i++) {
				if (q3ide_wm.wins[i].active && q3ide_wm.wins[i].is_tunnel) {
					terminal = i;
					break;
				}
			}
		}
	}

	/* Move terminal to spawn position at full size. */
	if (terminal >= 0) {
		q3ide_win_t *win = &q3ide_wm.wins[terminal];
		aspect = (win->tex_w > 0 && win->tex_h > 0) ? (float) win->tex_w / (float) win->tex_h
		         : (win->world_h > 0.001f)          ? win->world_w / win->world_h
		                                            : 16.0f / 9.0f;
		win->world_w = Q3IDE_SPAWN_WIN_W;
		win->world_h = Q3IDE_SPAWN_WIN_W / aspect;
		win->wall_mounted = qfalse;
		Q3IDE_WM_MoveWindow(terminal, pos, norm, qtrue);
		Q3IDE_LOGI("spawn: terminal id=%u at (%.0f,%.0f,%.0f)", win->capture_id, pos[0], pos[1], pos[2]);
	}

	/* Stack all other windows at the same position. */
	for (i = 0; i < Q3IDE_MAX_WIN; i++) {
		q3ide_win_t *win = &q3ide_wm.wins[i];
		if (!win->active || i == terminal)
			continue;
		win->wall_mounted = qfalse;
		Q3IDE_WM_MoveWindow(i, pos, norm, qtrue);
	}
	Q3IDE_LOGI("spawn: stacked %d window(s) at (%.0f,%.0f,%.0f)", q3ide_wm.num_active, pos[0], pos[1], pos[2]);
}
