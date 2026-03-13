/* q3ide_view_modes_overview_sync.c — Live sync: add/remove windows while overview is open. */

#include "q3ide_view_modes.h"
#include "../qcommon/qcommon.h"
#include "q3ide_params.h"
#include "q3ide_win_mngr.h"
#include "q3ide_win_mngr_internal.h"

extern qboolean g_ov_active;
extern qboolean g_ov_held;
extern qboolean g_ov_scrolling;
extern unsigned long long g_ov_last_scroll_ms;
extern void q3ide_overview_layout(void);
extern qboolean q3ide_player_axes(vec3_t eye, vec3_t fwd, vec3_t right);

static void q3ide_overview_live_sync(void)
{
	int i;
	qboolean was_active[Q3IDE_MAX_WIN];
	qboolean changed = qfalse;
	vec3_t eye, fwd, dummy_pos, dummy_norm;

	/* Add newly opened macOS windows to the arc. */
	Q3IDE_WM_PopulateQueue(qfalse);
	if (Q3IDE_WM_PendingCount() > 0) {
		if (q3ide_player_axes(eye, fwd, dummy_norm)) {
			dummy_pos[0] = eye[0] + fwd[0] * Q3IDE_SPAWN_WIN_DIST;
			dummy_pos[1] = eye[1] + fwd[1] * Q3IDE_SPAWN_WIN_DIST;
			dummy_pos[2] = eye[2];
			dummy_norm[0] = -fwd[0];
			dummy_norm[1] = -fwd[1];
			dummy_norm[2] = 0.0f;
			for (i = 0; i < Q3IDE_MAX_WIN; i++)
				was_active[i] = q3ide_wm.wins[i].active;
			Q3IDE_WM_FlushAllPending(dummy_pos, dummy_norm);
			for (i = 0; i < Q3IDE_MAX_WIN; i++) {
				if (q3ide_wm.wins[i].active && !was_active[i]) {
					q3ide_wm.wins[i].wall_placed = qfalse;
					q3ide_wm.wins[i].in_overview = qtrue;
					changed = qtrue;
				}
			}
		}
	}

	/* Remove closed app windows (owns_stream distinguishes them from display captures). */
	if (q3ide_wm.cap_list_wins && q3ide_wm.cap_free_wlist) {
		Q3ideWindowList wl = q3ide_wm.cap_list_wins(q3ide_wm.cap);
		for (i = 0; i < Q3IDE_MAX_WIN; i++) {
			q3ide_win_t *w = &q3ide_wm.wins[i];
			unsigned int j;
			qboolean found;
			if (!w->active || !w->owns_stream)
				continue;
			found = qfalse;
			for (j = 0; j < wl.count; j++) {
				if (wl.windows[j].window_id == w->capture_id) {
					found = qtrue;
					break;
				}
			}
			if (!found) {
				q3ide_wm.cap_stop(q3ide_wm.cap, w->capture_id);
				memset(w, 0, sizeof(q3ide_win_t));
				q3ide_wm.num_active--;
				changed = qtrue;
			}
		}
		q3ide_wm.cap_free_wlist(wl);
	}

	if (changed)
		q3ide_overview_layout();
}

void Q3IDE_Overview_Tick(void)
{
	static unsigned long long last_sync_ms = 0;
	unsigned long long now;
	if (!g_ov_active)
		return;
	now = (unsigned long long) Sys_Milliseconds();
	if (g_ov_scrolling) {
		if (now - g_ov_last_scroll_ms < Q3IDE_OVERVIEW_SCROLL_IDLE_MS)
			return;
		g_ov_scrolling = qfalse;
	}
	if (g_ov_held)
		q3ide_overview_layout();
	if (now - last_sync_ms >= 1000) {
		last_sync_ms = now;
		q3ide_overview_live_sync();
	}
}
