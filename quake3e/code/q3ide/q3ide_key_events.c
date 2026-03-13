/* q3ide_key_events.c — Keyboard event dispatch for Q3IDE hotkeys. */

#include "q3ide_engine_hooks.h"
#include "q3ide_hotkey.h"
#include "q3ide_log.h"
#include "q3ide_params.h"
#include "q3ide_win_mngr.h"
#include "q3ide_win_mngr_internal.h"
#include "q3ide_view_modes.h"
#include "q3ide_map_skin_browser.h"
#include "../qcommon/qcommon.h"
#include "../client/client.h"

extern q3ide_hooks_state_t q3ide_state;

extern void Q3IDE_DragResize_OnCmdKey(qboolean down);
extern qboolean Q3IDE_DragResize_OnScroll(int dir);

qboolean Q3IDE_OnKeyEvent(int key, qboolean down)
{
	if (!q3ide_state.initialized)
		return qfalse;
	/* CMD — enable drag+resize while held */
	if (key == K_COMMAND) {
		Q3IDE_DragResize_OnCmdKey(down);
		return qfalse; /* don't consume — Quake may need it too */
	}
	/* ";" — pause/resume streams (killswitch: blocks all automatic resumes while active) */
	if (key == ';') {
		static q3ide_hotkey_t s_pause_hk = Q3IDE_HOTKEY_INIT;
		if (down) {
			if (q3ide_hk_down(&s_pause_hk, s_pause_hk.locked) == Q3IDE_HK_ACTIVATE) {
				q3ide_wm.streams_user_paused = qtrue;
				Q3IDE_WM_PauseStreams();
			} else {
				q3ide_wm.streams_user_paused = qfalse;
				Q3IDE_WM_ResumeStreams();
			}
		} else {
			if (q3ide_hk_up(&s_pause_hk, qtrue) == Q3IDE_HK_DEACTIVATE) {
				q3ide_wm.streams_user_paused = qfalse;
				Q3IDE_WM_ResumeStreams();
			}
		}
		return qtrue;
	}
	/* "H" — hide windows + pause streams (tap = toggle, hold = temporary) */
	if (key == 'h') {
		static q3ide_hotkey_t s_hide_hk = Q3IDE_HOTKEY_INIT;
		if (down) {
			if (q3ide_hk_down(&s_hide_hk, s_hide_hk.locked) == Q3IDE_HK_ACTIVATE) {
				Q3IDE_WM_HideWins();
				Q3IDE_WM_PauseStreams();
			} else {
				Q3IDE_WM_ShowWins();
				Q3IDE_WM_ResumeStreams();
			}
		} else {
			if (q3ide_hk_up(&s_hide_hk, qtrue) == Q3IDE_HK_DEACTIVATE) {
				Q3IDE_WM_ShowWins();
				Q3IDE_WM_ResumeStreams();
			}
		}
		return qtrue;
	}
	/* "K" — global kill: wipe all windows, keep streams warm for instant reuse */
	if (key == 'k' && down) {
		extern void q3ide_overview_reset_state(void);
		extern void q3ide_focus3_reset_state(void);
		q3ide_overview_reset_state(); /* reset O flags, no cap_stop */
		q3ide_focus3_reset_state();   /* reset I flags, no cap_stop */
		Q3IDE_WM_CmdSoftDetachAll();  /* clear ALL tunnel windows, streams stay live */
		/* Force-clear pause flags so warm streams deliver frames the moment windows
		 * are re-placed.  Without this, movement-pause blocks the first frame. */
		q3ide_wm.streams_user_paused = qfalse;
		q3ide_wm.streams_move_paused = qfalse;
		Q3IDE_WM_ResumeStreams();
		Q3IDE_SetHudMsg("KILL WIN — all detached", Q3IDE_HUD_CONFIRM_MS);
		return qtrue;
	}
	/* Scroll wheel — resize (CMD held) takes priority, then overview scroll.
	 * Consumes the event so Quake's weapon-swap binding never fires. */
	if ((key == K_MWHEELUP || key == K_MWHEELDOWN) && down) {
		int dir = (key == K_MWHEELUP) ? 1 : -1; /* +1=up=smaller, -1=down=bigger */
		if (Q3IDE_DragResize_OnScroll(dir))
			return qtrue; /* consumed by resize */
		if (Q3IDE_ViewModes_OverviewActive()) {
			extern float g_ov_scroll_z;
			extern qboolean g_ov_scrolling;
			extern unsigned long long g_ov_last_scroll_ms;
			extern void q3ide_ov_scroll_apply(float delta);
			static unsigned long long s_last_scroll_ms = 0;
			unsigned long long now = (unsigned long long) Sys_Milliseconds();
			float delta;
			if (now - s_last_scroll_ms < Q3IDE_OVERVIEW_SCROLL_COOLDOWN_MS)
				return qtrue; /* too soon — consume but don't move */
			s_last_scroll_ms = now;
			delta = (key == K_MWHEELUP) ? -Q3IDE_OVERVIEW_SCROLL_Z_STEP : Q3IDE_OVERVIEW_SCROLL_Z_STEP;
			g_ov_scroll_z += delta;
			q3ide_ov_scroll_apply(delta);
			g_ov_scrolling = qtrue;
			g_ov_last_scroll_ms = now;
			return qtrue; /* consumed — block weapon swap */
		}
	}
	/* "M" — map/skin browser */
	if (Q3IDE_MMenu_OnKey(key, down))
		return qtrue;
	return qfalse;
}
