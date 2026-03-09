/* q3ide_cmd_query.c — Q3IDE poll changes, desktop, and snap commands. */

#include "q3ide_wm.h"
#include "q3ide_layout.h"
#include "q3ide_log.h"
#include "q3ide_wm_internal.h"
#include "../qcommon/qcommon.h"
#include "../client/client.h"
#include <math.h>
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

qboolean q3ide_is_attached(unsigned int id);

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
qboolean q3ide_is_attached(unsigned int id)
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
