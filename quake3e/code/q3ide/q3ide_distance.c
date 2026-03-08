/*
 * q3ide_distance.c — Distance-based frame rate control and idle detection (Batch 1).
 *
 * Tracks player position and manages per-window frame skipping based on distance.
 * Also handles idle status when windows stop receiving frames.
 */

#include "q3ide_wm.h"
#include "q3ide_wm_internal.h"
#include "../qcommon/qcommon.h"
#include <math.h>

#define Q3IDE_IDLE_TIMEOUT_MS 5000

/* Player position (updated by Q3IDE_WM_UpdatePlayerPos) */
static vec3_t q3ide_player_pos = {0, 0, 0};

void Q3IDE_WM_UpdatePlayerPos(float px, float py, float pz)
{
	int i;
	unsigned long long now_ms;
	VectorSet(q3ide_player_pos, px, py, pz);
	VectorSet(q3ide_wm.player_eye, px, py, pz);
	now_ms = Sys_Milliseconds();
	for (i = 0; i < Q3IDE_MAX_WIN; i++) {
		q3ide_win_t *win = &q3ide_wm.wins[i];
		vec3_t diff;
		if (!win->active)
			continue;
		/* Compute distance from player to window origin */
		VectorSubtract(win->origin, q3ide_player_pos, diff);
		win->player_dist = VectorLength(diff);
		/* Determine target FPS based on distance */
		if (win->player_dist < Q3IDE_DIST_FULL)
			win->fps_target = Q3IDE_FPS_FULL;
		else if (win->player_dist < Q3IDE_DIST_HALF)
			win->fps_target = Q3IDE_FPS_HALF;
		else if (win->player_dist < Q3IDE_DIST_LOW)
			win->fps_target = Q3IDE_FPS_LOW;
		else
			win->fps_target = Q3IDE_FPS_MIN;
		/* Update idle status */
		if (win->last_frame_ms > 0 && now_ms - win->last_frame_ms > Q3IDE_IDLE_TIMEOUT_MS)
			win->status = Q3IDE_WIN_STATUS_IDLE;
	}
}

void Q3IDE_WM_SetHover(int idx, float hover_t)
{
	if (idx < 0 || idx >= Q3IDE_MAX_WIN)
		return;
	if (!q3ide_wm.wins[idx].active)
		return;
	q3ide_wm.wins[idx].hover_t = hover_t;
}
