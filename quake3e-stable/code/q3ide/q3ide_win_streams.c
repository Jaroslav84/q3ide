/* q3ide_win_streams.c — Window stream pause/resume and hide/show control. */

#include "q3ide_win_mngr.h"
#include "q3ide_log.h"
#include "q3ide_win_mngr_internal.h"
#include "../qcommon/qcommon.h"

void Q3IDE_WM_PauseStreams(void)
{
	if (q3ide_wm.streams_paused)
		return;
	q3ide_wm.streams_paused = qtrue;
	if (q3ide_wm.cap_pause_streams && q3ide_wm.cap)
		q3ide_wm.cap_pause_streams(q3ide_wm.cap);
	Q3IDE_LOGI("streams paused (;)");
}

void Q3IDE_WM_ResumeStreams(void)
{
	if (!q3ide_wm.streams_paused)
		return;
	if (q3ide_wm.streams_user_paused)
		return; /* ";" killswitch active — block all automatic resumes */
	q3ide_wm.streams_paused = qfalse;
	if (q3ide_wm.cap_resume_streams && q3ide_wm.cap)
		q3ide_wm.cap_resume_streams(q3ide_wm.cap);
	Q3IDE_LOGI("streams resumed");
}

void Q3IDE_WM_TickMovePause(const vec3_t origin)
{
	extern int q3ide_aimed_win;
	float dx, dy, dz, dist2;
	qboolean moving;

	/* First call: seed reference position; skip movement check to avoid false-positive on spawn. */
	if (!q3ide_wm.move_initialized) {
		VectorCopy(origin, q3ide_wm.move_origin);
		q3ide_wm.move_initialized = qtrue;
		return;
	}

	dx = origin[0] - q3ide_wm.move_origin[0];
	dy = origin[1] - q3ide_wm.move_origin[1];
	dz = origin[2] - q3ide_wm.move_origin[2];
	dist2 = dx * dx + dy * dy + dz * dz;
	VectorCopy(origin, q3ide_wm.move_origin); /* always advance for next-frame delta */

	moving = (dist2 > Q3IDE_MOVE_THRESHOLD * Q3IDE_MOVE_THRESHOLD);

	if (moving) {
		q3ide_wm.move_stop_ms = 0;
		if (q3ide_aimed_win >= 0) {
			/* Rule 1 (highest): aimed window — keep streams live; undo any auto-pause */
			if (q3ide_wm.streams_move_paused) {
				q3ide_wm.streams_move_paused = qfalse;
				Q3IDE_WM_ResumeStreams();
			}
		} else if (!q3ide_wm.streams_paused) {
			/* Rule 2: moving, no aimed window — pause all */
			q3ide_wm.streams_move_paused = qtrue;
			Q3IDE_WM_PauseStreams();
		}
	} else if (q3ide_wm.streams_move_paused) {
		/* Player stopped — wait for stop delay before resuming */
		unsigned long long now = (unsigned long long) Sys_Milliseconds();
		if (q3ide_wm.move_stop_ms == 0)
			q3ide_wm.move_stop_ms = now;
		if (now - q3ide_wm.move_stop_ms >= Q3IDE_MOVE_STOP_DELAY_MS) {
			q3ide_wm.streams_move_paused = qfalse;
			q3ide_wm.move_stop_ms = 0;
			Q3IDE_WM_ResumeStreams();
		}
	}
}

void Q3IDE_WM_HideWins(void)
{
	q3ide_wm.wins_hidden = qtrue;
	Q3IDE_LOGI("windows hidden (H)");
}

void Q3IDE_WM_ShowWins(void)
{
	q3ide_wm.wins_hidden = qfalse;
	Q3IDE_LOGI("windows shown (H)");
}

qboolean Q3IDE_WM_WinsHidden(void)
{
	return q3ide_wm.wins_hidden;
}
