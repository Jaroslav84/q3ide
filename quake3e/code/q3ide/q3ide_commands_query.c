/* q3ide_commands_query.c — Q3IDE poll changes and desktop command. */

#include "q3ide_win_mngr.h"
#include "q3ide_log.h"
#include "q3ide_win_mngr_internal.h"
#include "../qcommon/qcommon.h"
#include "../client/client.h"
#include <math.h>
#include <pthread.h>
#include <string.h>

const char *q3ide_terminal_apps[] = {"iTerm2", "Terminal", NULL};
const char *q3ide_browser_apps[] = {"Google Chrome", "Chromium", "Safari",         "Firefox", "Arc",
                                    "Brave Browser", "Opera",    "Microsoft Edge", NULL};

qboolean q3ide_match(const char *app, const char **list)
{
	int i;
	if (!app)
		return qfalse;
	for (i = 0; list[i]; i++)
		if (Q_stristr(app, list[i]))
			return qtrue;
	return qfalse;
}

qboolean q3ide_is_attached(unsigned int id)
{
	int i;
	for (i = 0; i < Q3IDE_MAX_WIN; i++)
		if (q3ide_wm.wins[i].active && q3ide_wm.wins[i].capture_id == id)
			return qtrue;
	return qfalse;
}

/* Auto-attach a new window at spawn position (200u in front of player). */
static void q3ide_auto_attach_new(unsigned int id, float aspect)
{
	vec3_t eye, fwd, pos, norm;
	float yaw_rad;
	float ww = 100.0f;
	float wh = (aspect > 0.001f) ? ww / aspect : ww * 9.0f / 16.0f;

	VectorCopy(cl.snap.ps.origin, eye);
	eye[2] += cl.snap.ps.viewheight;
	yaw_rad = cl.snap.ps.viewangles[YAW] * (float) M_PI / 180.0f;
	fwd[0] = cosf(yaw_rad);
	fwd[1] = sinf(yaw_rad);
	fwd[2] = 0.0f;
	pos[0] = eye[0] + fwd[0] * 200.0f;
	pos[1] = eye[1] + fwd[1] * 200.0f;
	pos[2] = eye[2];
	norm[0] = -fwd[0];
	norm[1] = -fwd[1];
	norm[2] = 0.0f;

	Q3IDE_WM_Attach(id, pos, norm, ww, wh, qtrue, qfalse);
	Q3IDE_LOGI("auto-attached wid=%u", id);
}

/* ── PollChanges — background thread fetches, main thread drains ── */

static void q3ide_apply_change_list(const Q3ideWindowChangeList *clist)
{
	unsigned int i;
	for (i = 0; i < clist->count; i++) {
		const Q3ideWindowChange *ch = &clist->changes[i];
		if (ch->is_added == 0) {
			/* Window closed — detach if attached */
			if (q3ide_is_attached(ch->window_id))
				Q3IDE_WM_DetachById(ch->window_id);
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
		} else if (ch->is_added == 1 && q3ide_wm.auto_attach) {
			/* New window — auto-attach if it matches app filter */
			float aspect;
			if ((int) ch->width < Q3IDE_MIN_WIN_W || (int) ch->height < Q3IDE_MIN_WIN_H)
				continue;
			if (!q3ide_match(ch->app_name, q3ide_terminal_apps) && !q3ide_match(ch->app_name, q3ide_browser_apps))
				continue;
			aspect = ch->height ? (float) ch->width / ch->height : 16.0f / 9.0f;
			q3ide_auto_attach_new(ch->window_id, aspect);
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

/* ── CmdDesktop — mirror each macOS display onto its game monitor ── */
void Q3IDE_WM_CmdDesktop(void)
{
	Q3ideDisplayList dlist;
	Q3ideDisplayInfo sorted[Q3IDE_MAX_WIN];
	vec3_t eye, dir, wpos, wnorm, pos;
	float yaw, angle;
	int n_disp, n_mon, center, i, j, attached = 0;

	if (!q3ide_win_mngr.cap || !q3ide_win_mngr.cap_list_disp || !q3ide_win_mngr.cap_start_disp) {
		Q3IDE_LOGI("display capture not available");
		return;
	}
	Q3IDE_WM_CmdDetachAll();
	dlist = q3ide_win_mngr.cap_list_disp(q3ide_win_mngr.cap);
	if (!dlist.displays || !dlist.count) {
		Q3IDE_LOGI("no displays found");
		return;
	}
	n_disp = (int) dlist.count;
	if (n_disp > Q3IDE_MAX_WIN)
		n_disp = Q3IDE_MAX_WIN;
	for (i = 0; i < n_disp; i++)
		sorted[i] = dlist.displays[i];
	for (i = 0; i < n_disp - 1; i++) /* sort displays left→right by macOS x */
		for (j = i + 1; j < n_disp; j++)
			if (sorted[j].x < sorted[i].x) {
				Q3ideDisplayInfo t = sorted[i];
				sorted[i] = sorted[j];
				sorted[j] = t;
			}
	if (q3ide_win_mngr.cap_free_dlist)
		q3ide_win_mngr.cap_free_dlist(dlist);

	VectorCopy(cl.snap.ps.origin, eye);
	eye[2] += cl.snap.ps.viewheight;
	yaw = cl.snap.ps.viewangles[YAW] * (float) M_PI / 180.0f;
	n_mon = Cvar_VariableIntegerValue("r_mmNumMon");
	if (n_mon < 1)
		n_mon = 1;
	center = n_mon / 2;
	angle = Cvar_VariableValue("r_monitorAngle");

	for (i = 0; i < n_disp; i++) {
		Q3ideDisplayInfo *d = &sorted[i];
		float yt = yaw + (float) (center - i) * angle * (float) M_PI / 180.0f;
		float oh = Q3IDE_WIN_INCHES;
		float ow = d->height ? oh * (float) d->width / (float) d->height : oh * 16.0f / 9.0f;
		dir[0] = cosf(yt);
		dir[1] = sinf(yt);
		dir[2] = 0.0f;
		if (!Q3IDE_WM_TraceWall(eye, dir, wpos, wnorm)) {
			wpos[0] = eye[0] + dir[0] * 512.0f;
			wpos[1] = eye[1] + dir[1] * 512.0f;
			wpos[2] = eye[2];
			wnorm[0] = -dir[0];
			wnorm[1] = -dir[1];
			wnorm[2] = 0.0f;
		}
		if (q3ide_win_mngr.cap_start_disp(q3ide_win_mngr.cap, d->display_id, Q3IDE_CAPTURE_FPS) != 0) {
			Q3IDE_LOGI("display %u start failed", d->display_id);
			continue;
		}
		VectorCopy(wpos, pos);
		pos[2] = eye[2];
		if (Q3IDE_WM_Attach(d->display_id, pos, wnorm, ow, oh, qfalse, qfalse)) {
			Q3IDE_WM_SetLabel(d->display_id, va("Display %d", i + 1));
			attached++;
		}
	}
	Q3IDE_LOGI("mirror: %d/%d display(s)", attached, n_disp);
}
