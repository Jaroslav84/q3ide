/* q3ide_commands_attach.c — Q3IDE_WM_CmdAttach: enumerate + attach windows.
 *
 * Filter helpers / pending queue: q3ide_attach_filter.c.
 */

#include "q3ide_win_mngr.h"
#include "q3ide_log.h"
#include "q3ide_win_mngr_internal.h"
#include "../qcommon/qcommon.h"
#include "../client/client.h"
#include <math.h>
#include <string.h>

/* ── From q3ide_attach_filter.c ─────────────────────────────────── */
extern const char *q3ide_terminal_apps[];
extern const char *q3ide_browser_apps[];
extern qboolean    q3ide_match(const char *app, const char **list);
extern qboolean    q3ide_is_attached(unsigned int id);
extern qboolean    Q3IDE_IsSystemJunk(const Q3ideWindowInfo *w);
extern void        q3ide_push_display(unsigned int did, int disp_idx);
extern void        q3ide_push_win(const Q3ideWindowInfo *w);
extern void        q3ide_reset_pending(void);
extern void        q3ide_compact_pending(void);
extern void        q3ide_log_pending(void);
extern int         q3ide_flush_pending(int n, vec3_t pos, vec3_t norm);
extern int         q3ide_pending_n(void);

void Q3IDE_WM_CmdAttach(void)
{
	Q3ideDisplayList dlist        = {NULL, 0};
	Q3ideWindowList  wlist        = {NULL, 0};
	int              i, display_n = 0;
	unsigned int     display_ids[Q3IDE_MAX_WIN];
	unsigned int     target_ids[Q3IDE_MAX_WIN];
	int              target_n = 0;
	vec3_t           eye, fwd, pos, norm;
	float            yaw_rad;

	if (!q3ide_win_mngr.cap || !q3ide_win_mngr.cap_list_wins) {
		Com_Printf("q3ide: not ready\n");
		return;
	}

	/* ── Collect displays ───────────────────────────────────────── */
	if (q3ide_win_mngr.cap_list_disp && q3ide_win_mngr.cap_start_disp) {
		dlist = q3ide_win_mngr.cap_list_disp(q3ide_win_mngr.cap);
		if (dlist.displays) {
			for (i = 0; i < (int) dlist.count && display_n < Q3IDE_MAX_WIN; i++) {
				display_ids[display_n] = dlist.displays[i].display_id;
				target_ids[target_n++] = display_ids[display_n];
				display_n++;
			}
		}
	}

	/* ── Collect app windows (size-filtered) ────────────────────── */
	wlist = q3ide_win_mngr.cap_list_wins(q3ide_win_mngr.cap);

	/* ── Detach windows no longer alive ─────────────────────────── */
	if (wlist.windows) {
		for (i = 0; i < (int) wlist.count && target_n < Q3IDE_MAX_WIN; i++) {
			const Q3ideWindowInfo *w = &wlist.windows[i];
			if ((int) w->width < Q3IDE_MIN_WIN_W || (int) w->height < Q3IDE_MIN_WIN_H)
				continue;
			if ((w->title && strstr(w->title, "StatusIndicator")) ||
			    (w->app_name && strstr(w->app_name, "StatusIndicator")))
				continue;
			if (Q3IDE_IsSystemJunk(w))
				continue;
			target_ids[target_n++] = w->window_id;
		}
	}
	{
		int si, ki;
		for (si = 0; si < Q3IDE_MAX_WIN; si++) {
			qboolean keep = qfalse;
			if (!q3ide_wm.wins[si].active)
				continue;
			for (ki = 0; ki < target_n; ki++)
				if (target_ids[ki] == q3ide_wm.wins[si].capture_id) {
					keep = qtrue;
					break;
				}
			if (!keep)
				Q3IDE_WM_DetachById(q3ide_wm.wins[si].capture_id);
		}
	}

	/* ── Default spawn position ─────────────────────────────────── */
	VectorCopy(cl.snap.ps.origin, eye);
	eye[2] += cl.snap.ps.viewheight;
	yaw_rad = cl.snap.ps.viewangles[YAW] * (float) M_PI / 180.0f;
	fwd[0]  = cosf(yaw_rad);
	fwd[1]  = sinf(yaw_rad);
	fwd[2]  = 0.0f;
	pos[0]  = eye[0] + fwd[0] * Q3IDE_SPAWN_WIN_DIST;
	pos[1]  = eye[1] + fwd[1] * Q3IDE_SPAWN_WIN_DIST;
	pos[2]  = eye[2];
	norm[0] = -fwd[0];
	norm[1] = -fwd[1];
	norm[2] = 0.0f;

	/* ── Build pending queue: monitors → terminals → browsers → rest ── */
	q3ide_reset_pending();

	for (i = 0; i < display_n; i++)
		q3ide_push_display(display_ids[i], i);

	if (wlist.windows) {
		int pass;
		for (pass = 0; pass < 3; pass++) {
			for (i = 0; i < (int) wlist.count; i++) {
				const Q3ideWindowInfo *w = &wlist.windows[i];
				qboolean is_term, is_browser;
				if ((int) w->width < Q3IDE_MIN_WIN_W || (int) w->height < Q3IDE_MIN_WIN_H)
					continue;
				if ((w->title && strstr(w->title, "StatusIndicator")) ||
				    (w->app_name && strstr(w->app_name, "StatusIndicator")))
					continue;
				if (Q3IDE_IsSystemJunk(w))
					continue;
				is_term    = q3ide_match(w->app_name, q3ide_terminal_apps);
				is_browser = q3ide_match(w->app_name, q3ide_browser_apps);
				if (pass == 0 && !is_term)
					continue;
				if (pass == 1 && !is_browser)
					continue;
				if (pass == 2 && (is_term || is_browser))
					continue;
				q3ide_push_win(w);
			}
		}
	}

	if (dlist.displays && q3ide_win_mngr.cap_free_dlist)
		q3ide_win_mngr.cap_free_dlist(dlist);
	if (wlist.windows && q3ide_win_mngr.cap_free_wlist)
		q3ide_win_mngr.cap_free_wlist(wlist);

	q3ide_compact_pending();
	q3ide_log_pending();

	/* Attach first spawn_n from queue; rest stay pending for wall-shots. */
	{
		int spawn_n = Cvar_VariableIntegerValue("q3ide_spawn_count");
		Q3IDE_LOGI("attach: spawn_count=%d  pending=%d", spawn_n, q3ide_pending_n());
		q3ide_flush_pending(spawn_n, pos, norm);
	}

	q3ide_wm.auto_attach = qtrue;
	Com_Printf("q3ide: attached %d, %d pending\n", q3ide_wm.num_active, q3ide_pending_n());
	Q3IDE_Eventf("attach_done", "\"attached\":%d,\"pending\":%d", q3ide_wm.num_active, q3ide_pending_n());
}
