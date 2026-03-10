/* q3ide_commands_query.c — PollChanges: drain background changes, auto-attach. */

#include "q3ide_win_mngr.h"
#include "q3ide_log.h"
#include "q3ide_win_mngr_internal.h"
#include "../qcommon/qcommon.h"
#include "../client/client.h"
#include <math.h>
#include <pthread.h>
#include <string.h>

/* ── From q3ide_attach_filter.c ─────────────────────────────────── */
extern const char *q3ide_terminal_apps[];
extern const char *q3ide_browser_apps[];
extern qboolean    q3ide_match(const char *app, const char **list);

/* Auto-attach a new window at spawn position (200u in front of player). */
static void q3ide_auto_attach_new(unsigned int id, float aspect, const char *app_name)
{
	vec3_t eye, fwd, pos, norm;
	float  yaw_rad;
	float  ww = 100.0f;
	float  wh = (aspect > 0.001f) ? ww / aspect : ww * 9.0f / 16.0f;
	int    idx;

	VectorCopy(cl.snap.ps.origin, eye);
	eye[2] += cl.snap.ps.viewheight;
	yaw_rad = cl.snap.ps.viewangles[YAW] * (float) M_PI / 180.0f;
	fwd[0]  = cosf(yaw_rad);
	fwd[1]  = sinf(yaw_rad);
	fwd[2]  = 0.0f;
	pos[0]  = eye[0] + fwd[0] * 200.0f;
	pos[1]  = eye[1] + fwd[1] * 200.0f;
	pos[2]  = eye[2];
	norm[0] = -fwd[0];
	norm[1] = -fwd[1];
	norm[2] = 0.0f;

	if (!Q3IDE_WM_Attach(id, pos, norm, ww, wh, qtrue, qfalse))
		return;
	idx = Q3IDE_WM_FindById(id);
	if (idx >= 0 && app_name && app_name[0])
		Q_strncpyz(q3ide_wm.wins[idx].app_name, app_name, sizeof(q3ide_wm.wins[idx].app_name));
	Q3IDE_LOGI("auto-attached wid=%u app=%s", id, app_name ? app_name : "");
}

/* ── PollChanges — background thread fetches, main thread drains ── */

static void q3ide_apply_change_list(const Q3ideWindowChangeList *clist)
{
	unsigned int i;
	for (i = 0; i < clist->count; i++) {
		const Q3ideWindowChange *ch = &clist->changes[i];
		if (ch->is_added == 0) {
			/* Window closed — remove from group (or detach panel if last window) */
			Q3IDE_WM_RemoveFromGroup(ch->window_id);
		} else if (ch->is_added == 2) {
			/* Window resized — update world aspect ratio */
			int idx = Q3IDE_WM_FindById(ch->window_id);
			if (idx >= 0 && ch->width > 0 && ch->height > 0) {
				q3ide_win_t *w   = &q3ide_wm.wins[idx];
				float new_asp    = (float) ch->width / (float) ch->height;
				w->world_w       = w->world_h * new_asp;
				Q3IDE_LOGI("win %u resized %dx%d world %.0fx%.0f", ch->window_id, ch->width, ch->height,
				           w->world_w, w->world_h);
			}
		} else if (ch->is_added == 3) {
			/* Window moved — composite crop was updated by dylib automatically */
			Q3IDE_LOGI("win %u moved, composite crop refreshed", ch->window_id);
		} else if (ch->is_added == 1 && q3ide_wm.auto_attach) {
			/* New window — auto-attach if it matches app filter */
			float aspect;
			if ((int) ch->width < Q3IDE_MIN_WIN_W || (int) ch->height < Q3IDE_MIN_WIN_H)
				continue;
			if (!q3ide_match(ch->app_name, q3ide_terminal_apps) && !q3ide_match(ch->app_name, q3ide_browser_apps))
				continue;
			aspect = ch->height ? (float) ch->width / ch->height : 16.0f / 9.0f;
			q3ide_auto_attach_new(ch->window_id, aspect, ch->app_name);
			Q3IDE_WM_SetLabel(ch->window_id, ch->app_name ? ch->app_name : "");
		}
	}
}

/* Called from Q3IDE_WM_PollFrames — drains changes fetched by background thread. */
void Q3IDE_WM_DrainPendingChanges(void)
{
	Q3ideWindowChangeList clist;
	qboolean has;

	pthread_mutex_lock(&q3ide_wm.poll_mutex);
	has = q3ide_wm.poll_has_pending;
	if (has) {
		clist                    = q3ide_wm.poll_pending;
		q3ide_wm.poll_has_pending = qfalse;
	}
	pthread_mutex_unlock(&q3ide_wm.poll_mutex);

	if (!has)
		return;
	q3ide_apply_change_list(&clist);
	if (q3ide_win_mngr.cap_free_changes)
		q3ide_win_mngr.cap_free_changes(clist);
}

/* Kept for callers that need a synchronous fetch. */
void Q3IDE_WM_PollChanges(void)
{
	Q3ideWindowChangeList clist;
	if (!q3ide_win_mngr.cap_poll_changes || !q3ide_win_mngr.cap_free_changes)
		return;
	clist = q3ide_win_mngr.cap_poll_changes(q3ide_win_mngr.cap);
	if (!clist.changes || !clist.count)
		return;
	q3ide_apply_change_list(&clist);
	q3ide_win_mngr.cap_free_changes(clist);
}
