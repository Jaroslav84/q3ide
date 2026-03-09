/* q3ide_cmd.c — Q3IDE attach/desktop commands. */

#include "q3ide_wm.h"
#include "q3ide_layout.h"
#include "q3ide_log.h"
#include "q3ide_wm_internal.h"
#include "../qcommon/qcommon.h"
#include "../client/client.h"
#include <math.h>
#include <string.h>

static qboolean q3ide_is_attached(unsigned int id);

static const char *q3ide_terminal_apps[] = {"iTerm2", "Terminal", NULL};
static const char *q3ide_browser_apps[] = {"Google Chrome", "Chromium", "Safari",         "Firefox", "Arc",
                                           "Brave Browser", "Opera",    "Microsoft Edge", NULL};

static qboolean q3ide_match(const char *app, const char **list)
{
	int i;
	if (!app)
		return qfalse;
	for (i = 0; list[i]; i++)
		if (Q_stristr(app, list[i]))
			return qtrue;
	return qfalse;
}

qboolean Q3IDE_WM_DetachById(unsigned int cid)
{
	int i;
	for (i = 0; i < Q3IDE_MAX_WIN; i++) {
		q3ide_win_t *w = &q3ide_wm.wins[i];
		if (!w->active || w->capture_id != cid)
			continue;
		if (q3ide_wm.cap_stop)
			q3ide_wm.cap_stop(q3ide_wm.cap, cid);
		q3ide_wm.slot_mask &= ~(1u << w->scratch_slot);
		memset(w, 0, sizeof(q3ide_win_t));
		q3ide_wm.num_active--;
		Com_Printf("q3ide: detached wid=%u\n", cid);
		return qtrue;
	}
	Com_Printf("q3ide: wid=%u not found\n", cid);
	return qfalse;
}

typedef struct {
	unsigned int id;
	float aspect;
	qboolean is_display;
	char label[128];
} q3ide_attach_item_t;

void Q3IDE_WM_CmdAttach(void)
{
	q3ide_attach_item_t items[Q3IDE_MAX_WIN];
	int item_n = 0;
	int i, j;
	vec3_t eye;

	if (!q3ide_wm.cap || !q3ide_wm.cap_list_wins) {
		Com_Printf("q3ide: not ready\n");
		return;
	}

	Q3IDE_WM_CmdDetachAll();

	/* Collect app windows (terminal + browser) */
	{
		Q3ideWindowList wlist = q3ide_wm.cap_list_wins(q3ide_wm.cap);
		if (wlist.windows && wlist.count) {
			for (i = 0; i < (int) wlist.count && item_n < Q3IDE_MAX_WIN; i++) {
				const Q3ideWindowInfo *w = &wlist.windows[i];
				qboolean dupe = qfalse;
				if ((int) w->width < Q3IDE_MIN_WIN_W || (int) w->height < Q3IDE_MIN_WIN_H)
					continue;
				if (!q3ide_match(w->app_name, q3ide_terminal_apps) && !q3ide_match(w->app_name, q3ide_browser_apps))
					continue;
				for (j = 0; j < item_n; j++)
					if (items[j].id == w->window_id) {
						dupe = qtrue;
						break;
					}
				if (dupe)
					continue;
				items[item_n].id = w->window_id;
				items[item_n].aspect = w->height ? (float) w->width / w->height : 16.0f / 9.0f;
				items[item_n].is_display = qfalse;
				Q_strncpyz(items[item_n].label, (w->title && w->title[0]) ? w->title : (w->app_name ? w->app_name : ""),
				           sizeof(items[item_n].label));
				Com_Printf("q3ide: app [%d] wid=%u \"%s\" %ux%u\n", item_n, w->window_id, w->app_name, w->width,
				           w->height);
				Q3IDE_Eventf("window_found", "\"wid\":%u,\"w\":%u,\"h\":%u", w->window_id, w->width, w->height);
				item_n++;
			}
		}
		if (q3ide_wm.cap_free_wlist)
			q3ide_wm.cap_free_wlist(wlist);
	}

	/* Collect displays */
	if (q3ide_wm.cap_list_disp && q3ide_wm.cap_start_disp) {
		Q3ideDisplayList dlist = q3ide_wm.cap_list_disp(q3ide_wm.cap);
		if (dlist.displays && dlist.count) {
			for (i = 0; i < (int) dlist.count && item_n < Q3IDE_MAX_WIN; i++) {
				const Q3ideDisplayInfo *d = &dlist.displays[i];
				items[item_n].id = d->display_id;
				items[item_n].aspect = d->height ? (float) d->width / d->height : 16.0f / 9.0f;
				items[item_n].is_display = qtrue;
				Q_strncpyz(items[item_n].label, va("Display %d", i + 1), sizeof(items[item_n].label));
				Com_Printf("q3ide: disp [%d] id=%u %ux%u\n", item_n, d->display_id, d->width, d->height);
				Q3IDE_Eventf("display_found", "\"id\":%u,\"w\":%u,\"h\":%u", d->display_id, d->width, d->height);
				item_n++;
			}
		}
		if (q3ide_wm.cap_free_dlist)
			q3ide_wm.cap_free_dlist(dlist);
	}

	if (!item_n) {
		Com_Printf("q3ide: no windows or displays found\n");
		return;
	}

	VectorCopy(cl.snap.ps.origin, eye);
	eye[2] += cl.snap.ps.viewheight;

	/* Scan room walls and place windows using the layout engine */
	{
		q3ide_room_t room;
		unsigned int ids[Q3IDE_MAX_WIN];
		float aspects[Q3IDE_MAX_WIN];
		int is_disp[Q3IDE_MAX_WIN];
		int n_wall_placed = 0;

		q3ide_room_scan(eye, &room);
		if (room.n) {
			for (i = 0; i < item_n; i++) {
				ids[i] = items[i].id;
				aspects[i] = items[i].aspect;
				is_disp[i] = items[i].is_display ? 1 : 0;
			}
			n_wall_placed = q3ide_room_layout(&room, ids, aspects, is_disp, item_n);
		} else {
			Com_Printf("q3ide: no walls found — placing floating\n");
		}

		/* Float any items the wall layout couldn't fit ergonomically.
		 * Each overflow window fans out at increasing angles around the player. */
		if (n_wall_placed < item_n) {
			float base_yaw = cl.snap.ps.viewangles[YAW] * (float) M_PI / 180.0f;
			int float_n = 0;
			for (i = 0; i < item_n; i++) {
				float yaw, fy;
				vec3_t pos, norm;
				if (q3ide_is_attached(items[i].id))
					continue; /* already on a wall */
				/* Spread overflow: 0°, ±45°, ±90°, ±135°, 180° from forward */
				fy = base_yaw + (float) (((float_n & 1) ? 1 : -1) * ((float_n + 1) / 2)) * (float) M_PI / 4.0f;
				yaw = fy;
				pos[0] = eye[0] + cosf(yaw) * 220.0f;
				pos[1] = eye[1] + sinf(yaw) * 220.0f;
				pos[2] = eye[2];
				norm[0] = -cosf(yaw);
				norm[1] = -sinf(yaw);
				norm[2] = 0.0f;
				Q3IDE_WM_Attach(items[i].id, pos, norm, 160.0f, 90.0f, qtrue, qfalse);
				Com_Printf("q3ide: float[%d] id=%u at angle=%.0f\n", float_n, items[i].id,
				           fy * 180.0f / (float) M_PI);
				float_n++;
			}
		}

		/* Set labels on all items (no-op for any that failed to attach) */
		for (i = 0; i < item_n; i++)
			Q3IDE_WM_SetLabel(items[i].id, items[i].label);
	}

	q3ide_wm.auto_attach = qtrue;
	Q3IDE_LOGI("attached %d/%d items (windows+displays)", q3ide_wm.num_active, item_n);
	Q3IDE_Eventf("attach_done", "\"attached\":%d,\"total\":%d", q3ide_wm.num_active, item_n);
}

static void q3ide_auto_attach_new(unsigned int id, float aspect)
{
	vec3_t eye;
	q3ide_room_t room;
	unsigned int ids[1];
	float aspects[1];
	int is_disp[1];

	VectorCopy(cl.snap.ps.origin, eye);
	eye[2] += cl.snap.ps.viewheight;

	q3ide_room_scan(eye, &room);
	if (!room.n) {
		Com_Printf("q3ide: auto-attach failed wid=%u (no walls)\n", id);
		return;
	}

	ids[0] = id;
	aspects[0] = aspect;
	is_disp[0] = 0;
	q3ide_room_layout(&room, ids, aspects, is_disp, 1);
	Com_Printf("q3ide: auto-attached wid=%u\n", id);
}

/* ── PollChanges — called every 2s; auto-attaches new windows ─── */
static qboolean q3ide_is_attached(unsigned int id)
{
	int i;
	for (i = 0; i < Q3IDE_MAX_WIN; i++)
		if (q3ide_wm.wins[i].active && q3ide_wm.wins[i].capture_id == id)
			return qtrue;
	return qfalse;
}

void Q3IDE_WM_PollChanges(void)
{
	Q3ideWindowChangeList clist;
	unsigned int i;

	if (!q3ide_wm.cap_poll_changes || !q3ide_wm.cap_free_changes)
		return;

	clist = q3ide_wm.cap_poll_changes(q3ide_wm.cap);
	if (!clist.changes || !clist.count)
		return;

	for (i = 0; i < clist.count; i++) {
		const Q3ideWindowChange *ch = &clist.changes[i];
		if (!ch->is_added) {
			/* Window closed — detach silently if we had it */
			if (q3ide_is_attached(ch->window_id))
				Q3IDE_WM_DetachById(ch->window_id);
		} else if (q3ide_wm.auto_attach) {
			float aspect;
			/* New window — auto-attach if it matches our app filter */
			if ((int) ch->width < Q3IDE_MIN_WIN_W || (int) ch->height < Q3IDE_MIN_WIN_H)
				continue;
			if (!q3ide_match(ch->app_name, q3ide_terminal_apps) && !q3ide_match(ch->app_name, q3ide_browser_apps))
				continue;
			aspect = ch->height ? (float) ch->width / ch->height : 16.0f / 9.0f;
			q3ide_auto_attach_new(ch->window_id, aspect);
			Q3IDE_WM_SetLabel(ch->window_id, ch->app_name ? ch->app_name : "");
		}
	}

	q3ide_wm.cap_free_changes(clist);
}

/* ── CmdDesktop — mirror each macOS display onto its game monitor ── */
void Q3IDE_WM_CmdDesktop(void)
{
	Q3ideDisplayList dlist;
	Q3ideDisplayInfo sorted[Q3IDE_MAX_WIN];
	vec3_t eye, dir, wpos, wnorm, pos;
	float yaw, angle;
	int n_disp, n_mon, center, i, j, attached = 0;

	if (!q3ide_wm.cap || !q3ide_wm.cap_list_disp || !q3ide_wm.cap_start_disp) {
		Com_Printf("q3ide: display capture not available\n");
		return;
	}
	Q3IDE_WM_CmdDetachAll();
	dlist = q3ide_wm.cap_list_disp(q3ide_wm.cap);
	if (!dlist.displays || !dlist.count) {
		Com_Printf("q3ide: no displays found\n");
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
	if (q3ide_wm.cap_free_dlist)
		q3ide_wm.cap_free_dlist(dlist);

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
		if (q3ide_wm.cap_start_disp(q3ide_wm.cap, d->display_id, Q3IDE_CAPTURE_FPS) != 0) {
			Com_Printf("q3ide: display %u start failed\n", d->display_id);
			continue;
		}
		VectorCopy(wpos, pos);
		pos[2] = eye[2];
		if (Q3IDE_WM_Attach(d->display_id, pos, wnorm, ow, oh, qfalse, qfalse)) {
			Q3IDE_WM_SetLabel(d->display_id, va("Display %d", i + 1));
			attached++;
		}
	}
	Com_Printf("q3ide: mirror: %d/%d display(s)\n", attached, n_disp);
}

/* ── CmdSnap — snap wins[0] into the q3dm0 teleporter ── */
void Q3IDE_WM_CmdSnap(void)
{
	vec3_t pos = {-1152.0f, -1868.0f, 84.0f};
	vec3_t normal = {0.0f, 1.0f, 0.0f};
	Q3IDE_WM_PlaceMirror(pos, normal, 112.0f, 176.0f);
	Com_Printf("q3ide: snapped mirror into teleporter arch at (%.0f,%.0f,%.0f)\n", pos[0], pos[1], pos[2]);
}
