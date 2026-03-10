/*
 * q3ide_interaction_frame.c — Per-frame interaction logic and key routing.
 */

#include "q3ide_engine_hooks.h"
#include "q3ide_interaction.h"
#include "q3ide_win_mngr.h"
#include "q3ide_win_mngr_internal.h"
#include "q3ide_log.h"
#include "../qcommon/qcommon.h"
#include "../client/client.h"
#include <math.h>

extern q3ide_interaction_state_t q3ide_interaction;
extern void q3ide_do_click(unsigned int wid, float uv_x, float uv_y);
extern int q3ide_crosshair_window(float *out_uv, float *out_dist, vec3_t out_hit_pos);

void Q3IDE_Interaction_Frame(qboolean attacking, qboolean use_key, qboolean escape, qboolean lock_key, float mouse_dx,
                             float mouse_dy)
{
	if (q3ide_interaction.mode == Q3IDE_MODE_FPS) {
		int prev_win = q3ide_interaction.focused_win;
		float uv[2], dist;

		q3ide_interaction.focused_win = q3ide_crosshair_window(uv, &dist, q3ide_interaction.focused_hit_pos);

		if (q3ide_interaction.focused_win >= 0) {
			q3ide_interaction.focused_uv[0] = uv[0];
			q3ide_interaction.focused_uv[1] = uv[1];
			q3ide_interaction.focused_dist = dist;

			if (q3ide_interaction.focused_win != prev_win) {
				q3ide_interaction.dwell_start_ms = (float) Sys_Milliseconds();
				q3ide_interaction.hover_t = 0.0f;
				if (prev_win >= 0)
					Q3IDE_WM_SetHover(prev_win, 0.0f);
				Com_DPrintf("q3ide: crosshair → win=%d dist=%.0f\n", q3ide_interaction.focused_win, dist);
			}

			if (q3ide_interaction.dwell_start_ms >= 0.0f) {
				/* Visual highlight is instant; dwell_start_ms only retained for pointer-mode entry. */
				q3ide_interaction.hover_t = 1.0f;
				Q3IDE_WM_SetHover(q3ide_interaction.focused_win, 1.0f);
			}

			if (lock_key && q3ide_interaction.focused_win >= 0 && q3ide_interaction.hover_t >= 1.0f &&
			    q3ide_interaction.focused_dist <= Q3IDE_POINTER_MAX_DIST) {
				q3ide_interaction.mode = Q3IDE_MODE_POINTER;
				q3ide_interaction.pointer_uv[0] = q3ide_interaction.focused_uv[0];
				q3ide_interaction.pointer_uv[1] = q3ide_interaction.focused_uv[1];
				Q3IDE_LOGI("entered Pointer Mode win=%d (L-key)", q3ide_interaction.focused_win);
				Q3IDE_SetHudMsg("L  POINTER MODE", 1500);
				return;
			}
		} else {
			if (prev_win >= 0)
				Q3IDE_WM_SetHover(prev_win, 0.0f);
			q3ide_interaction.dwell_start_ms = -1.0f;
			q3ide_interaction.hover_t = 0.0f;
		}

	} else if (q3ide_interaction.mode == Q3IDE_MODE_POINTER) {
		int win_idx = q3ide_interaction.focused_win;
		if (win_idx < 0 || !q3ide_wm.wins[win_idx].active) {
			if (win_idx >= 0)
				Q3IDE_WM_SetHover(win_idx, 0.0f);
			q3ide_interaction.mode = Q3IDE_MODE_FPS;
			q3ide_interaction.hover_t = 0.0f;
			q3ide_interaction.dwell_start_ms = (float) Sys_Milliseconds();
			Q3IDE_LOGI("exited Pointer Mode (window lost)");
			return;
		}

		{
			const q3ide_win_t *win = &q3ide_wm.wins[win_idx];
			float dist = q3ide_interaction.focused_dist > 100.0f ? q3ide_interaction.focused_dist : 100.0f;
			float uv_per_deg = (float) (M_PI / 180.0) * dist / win->world_w / 8.0f;
			q3ide_interaction.pointer_uv[0] += mouse_dx * uv_per_deg;
			q3ide_interaction.pointer_uv[1] += mouse_dy * uv_per_deg * (win->world_w / win->world_h);
		}

		if (q3ide_interaction.pointer_uv[0] < Q3IDE_EDGE_ZONE_UV ||
		    q3ide_interaction.pointer_uv[0] > 1.0f - Q3IDE_EDGE_ZONE_UV ||
		    q3ide_interaction.pointer_uv[1] < Q3IDE_EDGE_ZONE_UV ||
		    q3ide_interaction.pointer_uv[1] > 1.0f - Q3IDE_EDGE_ZONE_UV) {
			Q3IDE_WM_SetHover(win_idx, 0.0f);
			q3ide_interaction.mode = Q3IDE_MODE_FPS;
			q3ide_interaction.hover_t = 0.0f;
			q3ide_interaction.dwell_start_ms = (float) Sys_Milliseconds();
			Q3IDE_LOGI("exited Pointer Mode (edge)");
			return;
		}

		if (q3ide_interaction.pointer_uv[0] < 0.0f)
			q3ide_interaction.pointer_uv[0] = 0.0f;
		if (q3ide_interaction.pointer_uv[0] > 1.0f)
			q3ide_interaction.pointer_uv[0] = 1.0f;
		if (q3ide_interaction.pointer_uv[1] < 0.0f)
			q3ide_interaction.pointer_uv[1] = 0.0f;
		if (q3ide_interaction.pointer_uv[1] > 1.0f)
			q3ide_interaction.pointer_uv[1] = 1.0f;

		if (attacking)
			q3ide_do_click((unsigned int) q3ide_wm.wins[win_idx].capture_id, q3ide_interaction.pointer_uv[0],
			               q3ide_interaction.pointer_uv[1]);
		if (use_key) {
			q3ide_interaction.mode = Q3IDE_MODE_KEYBOARD;
			Q3IDE_LOGI("entered Keyboard Mode");
			Q3IDE_SetHudMsg("KEYBOARD MODE", 1500);
			return;
		}
		if (escape) {
			Q3IDE_WM_SetHover(win_idx, 0.0f);
			q3ide_interaction.mode = Q3IDE_MODE_FPS;
			q3ide_interaction.hover_t = 0.0f;
			q3ide_interaction.dwell_start_ms = (float) Sys_Milliseconds();
			Q3IDE_LOGI("exited Pointer Mode (ESC)");
			return;
		}

	} else if (q3ide_interaction.mode == Q3IDE_MODE_KEYBOARD) {
		(void) attacking;
		if (escape) {
			q3ide_interaction.mode = Q3IDE_MODE_FPS;
			Q3IDE_LOGI("exited Keyboard Mode (ESC)");
		}
	}
}

qboolean Q3IDE_Interaction_OnKeyEvent(int key, qboolean down)
{
	int win_idx;

	if (q3ide_interaction.mode == Q3IDE_MODE_POINTER) {
		if (down && key == 27) { /* K_ESCAPE */
			win_idx = q3ide_interaction.focused_win;
			if (win_idx >= 0)
				Q3IDE_WM_SetHover(win_idx, 0.0f);
			q3ide_interaction.mode = Q3IDE_MODE_FPS;
			q3ide_interaction.hover_t = 0.0f;
			q3ide_interaction.dwell_start_ms = (float) Sys_Milliseconds();
			Q3IDE_LOGI("exited Pointer Mode (ESC)");
			return qtrue;
		}
		return qfalse;
	}

	if (q3ide_interaction.mode != Q3IDE_MODE_KEYBOARD)
		return qfalse;

	if (down && key == 27) {
		q3ide_interaction.mode = Q3IDE_MODE_FPS;
		Q3IDE_LOGI("exited Keyboard Mode (ESC)");
		return qtrue;
	}

	win_idx = q3ide_interaction.focused_win;
	if (win_idx < 0 || !q3ide_wm.wins[win_idx].active)
		return qfalse;
	if (!q3ide_win_mngr.cap_inject_key)
		return qfalse;
	q3ide_win_mngr.cap_inject_key(q3ide_win_mngr.cap, q3ide_wm.wins[win_idx].capture_id, key, down ? 1 : 0);
	return qtrue;
}

q3ide_interaction_mode_t Q3IDE_Interaction_GetMode(void)
{
	return q3ide_interaction.mode;
}

qboolean Q3IDE_Interaction_ConsumesInput(void)
{
	return q3ide_interaction.mode != Q3IDE_MODE_FPS;
}
