/*
 * q3ide_distance.c — Player position tracking and idle detection.
 */

#include "q3ide_win_mngr.h"
#include "q3ide_win_mngr_internal.h"
#include "../qcommon/qcommon.h"
#include <math.h>

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
		VectorSubtract(win->origin, q3ide_player_pos, diff);
		win->player_dist = VectorLength(diff);
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
