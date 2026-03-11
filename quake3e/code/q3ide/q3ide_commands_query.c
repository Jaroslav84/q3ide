/* q3ide_commands_query.c — PollChanges: drain background changes, enqueue new windows. */

#include "q3ide_win_mngr.h"
#include "q3ide_log.h"
#include "q3ide_params.h"
#include "q3ide_win_mngr_internal.h"
#include "../qcommon/qcommon.h"
#include "../client/client.h"
#include <pthread.h>
#include <string.h>

/* ── PollChanges — background thread fetches, main thread drains ── */

static void q3ide_apply_change_list(const Q3ideWindowChangeList *clist)
{
	unsigned int i;
	for (i = 0; i < clist->count; i++) {
		const Q3ideWindowChange *ch = &clist->changes[i];
		if (ch->is_added == 0) {
			/* Window closed — detach it */
			Q3IDE_WM_DetachById(ch->window_id);
			if (q3ide_wm.macos_win_count > 0)
				q3ide_wm.macos_win_count--;
		} else if (ch->is_added == 1) {
			/* New window — enqueue for shoot-to-place (any app except system junk) */
			float aspect;
			if ((int) ch->width < Q3IDE_MIN_WIN_W || (int) ch->height < Q3IDE_MIN_WIN_H)
				continue;
			if (Q3IDE_IsJunkAppName(ch->app_name))
				continue;
			q3ide_wm.macos_win_count++; /* count only after junk+size filter */
			if (Q3IDE_WM_FindById(ch->window_id) >= 0)
				continue; /* already attached */
			aspect = ch->height ? (float) ch->width / (float) ch->height : 16.0f / 9.0f;
			Q3IDE_WM_EnqueueWindow(ch->window_id, aspect, ch->app_name);
			Q3IDE_LOGI("queued wid=%u app=%s", ch->window_id, ch->app_name ? ch->app_name : "");
		} else if (ch->is_added == 2) {
			/* Window resized — update world aspect ratio */
			int idx = Q3IDE_WM_FindById(ch->window_id);
			if (idx >= 0 && ch->width > 0 && ch->height > 0) {
				q3ide_win_t *w = &q3ide_wm.wins[idx];
				float new_asp = (float) ch->width / (float) ch->height;
				w->world_w = w->world_h * new_asp;
				Q3IDE_LOGI("win %u resized %dx%d world %.0fx%.0f", ch->window_id, ch->width, ch->height, w->world_w,
				           w->world_h);
			}
		} else if (ch->is_added == 3) {
			/* Window moved — composite crop was updated by dylib automatically */
			Q3IDE_LOGI("win %u moved, composite crop refreshed", ch->window_id);
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
		clist = q3ide_wm.poll_pending;
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
