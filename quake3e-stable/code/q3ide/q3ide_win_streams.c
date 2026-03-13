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
