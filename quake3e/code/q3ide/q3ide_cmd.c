/*
 * q3ide_cmd.c — Q3IDE attach/desktop commands.
 *
 * "q3ide attach":
 *   Scans 12 directions to find unique walls, then distributes all windows
 *   side-by-side (centered) on each wall. Round-robin wall assignment.
 * "q3ide desktop" — mirrors all displays on the nearest wall.
 */

#include "q3ide_wm.h"
#include "q3ide_log.h"
#include "q3ide_wm_internal.h"
#include "../qcommon/qcommon.h"
#include "../client/client.h"
#include <math.h>
#include <string.h>

/* ── App classification ─────────────────────────────────────────── */

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

#define Q3IDE_SCAN_DIRS 12 /* directions to scan for walls (every 30°) */

typedef struct {
	unsigned int id;
	float aspect;
	qboolean is_display;
} q3ide_attach_item_t;

void Q3IDE_WM_CmdAttach(void)
{
	q3ide_attach_item_t items[Q3IDE_MAX_WIN];
	int item_n = 0;
	int i, j, attached = 0;
	vec3_t eye, dir;
	float yaw;

	vec3_t wall_pos[Q3IDE_SCAN_DIRS], wall_norm[Q3IDE_SCAN_DIRS];
	int wall_win[Q3IDE_SCAN_DIRS][Q3IDE_MAX_WIN];
	int wall_wcnt[Q3IDE_SCAN_DIRS];
	int n_walls = 0;

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
	yaw = cl.snap.ps.viewangles[YAW] * (float) M_PI / 180.0f;

	/* Scan for unique walls */
	memset(wall_wcnt, 0, sizeof(wall_wcnt));
	for (i = 0; i < Q3IDE_SCAN_DIRS; i++) {
		vec3_t wp, wn;
		float yt = yaw + (float) i * (2.0f * (float) M_PI / (float) Q3IDE_SCAN_DIRS);
		qboolean dup = qfalse;
		dir[0] = cosf(yt);
		dir[1] = sinf(yt);
		dir[2] = 0.0f;
		if (!Q3IDE_WM_TraceWall(eye, dir, wp, wn))
			continue;
		for (j = 0; j < n_walls; j++) {
			if (DotProduct(wn, wall_norm[j]) > 0.85f) {
				dup = qtrue;
				break;
			}
		}
		if (dup || n_walls >= Q3IDE_SCAN_DIRS)
			continue;
		VectorCopy(wp, wall_pos[n_walls]);
		VectorCopy(wn, wall_norm[n_walls]);
		n_walls++;
	}

	if (!n_walls) {
		Com_Printf("q3ide: no walls found\n");
		return;
	}
	Com_Printf("q3ide: found %d unique wall(s)\n", n_walls);

	/* Assign items round-robin to walls, then place side-by-side centered */
	memset(wall_win, 0, sizeof(wall_win));
	for (i = 0; i < item_n; i++) {
		int wi = i % n_walls;
		wall_win[wi][wall_wcnt[wi]++] = i;
	}
	for (i = 0; i < n_walls; i++) {
		vec3_t right;
		float nx, ny, horiz_len, total_w, gap, x_cursor;
		int cnt = wall_wcnt[i];

		if (!cnt)
			continue;

		nx = wall_norm[i][0];
		ny = wall_norm[i][1];
		horiz_len = sqrtf(nx * nx + ny * ny);
		if (horiz_len > 0.01f) {
			right[0] = -ny / horiz_len;
			right[1] = nx / horiz_len;
			right[2] = 0.0f;
		} else {
			right[0] = 1.0f;
			right[1] = 0.0f;
			right[2] = 0.0f;
		}

		gap = 16.0f;
		total_w = 0.0f;
		for (j = 0; j < cnt; j++) {
			float oh = Q3IDE_WIN_INCHES;
			total_w += oh * items[wall_win[i][j]].aspect;
		}
		total_w += gap * (float) (cnt - 1);

		x_cursor = -total_w * 0.5f;
		for (j = 0; j < cnt; j++) {
			int ii = wall_win[i][j];
			q3ide_attach_item_t *it = &items[ii];
			float oh = Q3IDE_WIN_INCHES;
			float ow = oh * it->aspect;
			float cx = x_cursor + ow * 0.5f;
			vec3_t pos;
			pos[0] = wall_pos[i][0] + right[0] * cx;
			pos[1] = wall_pos[i][1] + right[1] * cx;
			pos[2] = eye[2];
			if (it->is_display) {
				/* Start display stream first, then attach without re-starting */
				if (q3ide_wm.cap_start_disp(q3ide_wm.cap, it->id, Q3IDE_CAPTURE_FPS) != 0) {
					Q3IDE_LOGE("disp [%d] start failed", ii);
					x_cursor += ow + gap;
					continue;
				}
				if (Q3IDE_WM_Attach(it->id, pos, wall_norm[i], ow, oh, qfalse)) {
					attached++;
					Q3IDE_Eventf("display_attached", "\"id\":%u,\"wall\":%d,\"x\":%.0f", it->id, i, cx);
				} else {
					Q3IDE_LOGE("disp [%d] attach failed", ii);
				}
			} else {
				if (Q3IDE_WM_Attach(it->id, pos, wall_norm[i], ow, oh, qtrue)) {
					attached++;
					Q3IDE_Eventf("window_attached", "\"wid\":%u,\"wall\":%d,\"x\":%.0f", it->id, i, cx);
				} else {
					Q3IDE_LOGE("win [%d] attach failed", ii);
				}
			}
			x_cursor += ow + gap;
		}
	}

	q3ide_wm.auto_attach = qtrue;
	Q3IDE_LOGI("attached %d/%d items (windows+displays)", attached, item_n);
	Q3IDE_Eventf("attach_done", "\"attached\":%d,\"total\":%d", attached, item_n);
}

static void q3ide_auto_attach_new(unsigned int id, float aspect)
{
	vec3_t eye, dir, wp, wn;
	float yaw, oh, ow;
	int t;

	oh = Q3IDE_WIN_INCHES;
	ow = oh * aspect;
	VectorCopy(cl.snap.ps.origin, eye);
	eye[2] += cl.snap.ps.viewheight;
	yaw = cl.snap.ps.viewangles[YAW] * (float) M_PI / 180.0f;

	for (t = 0; t < Q3IDE_SCAN_DIRS; t++) {
		float yt = yaw + (float) t * (2.0f * (float) M_PI / (float) Q3IDE_SCAN_DIRS);
		dir[0] = cosf(yt);
		dir[1] = sinf(yt);
		dir[2] = 0.0f;
		if (!Q3IDE_WM_TraceWall(eye, dir, wp, wn))
			continue;
		wp[2] = eye[2];
		if (Q3IDE_WM_Attach(id, wp, wn, ow, oh, qtrue)) {
			Com_Printf("q3ide: auto-attached wid=%u dir=%d\n", id, t);
			return;
		}
	}
	Com_Printf("q3ide: auto-attach failed wid=%u\n", id);
}

/* ── PollChanges — called every 2s from Q3IDE_WM_PollFrames ────────
 * Detaches closed windows; auto-attaches new matching windows when
 * auto_attach mode is enabled (set by "q3ide attach all").
 */

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
		}
	}

	q3ide_wm.cap_free_changes(clist);
}

/* ── CmdDesktop ─────────────────────────────────────────────────── */

void Q3IDE_WM_CmdDesktop(void)
{
	Q3ideDisplayList dlist;
	vec3_t eye, dir, wall_pos, wall_normal;
	float yaw, pitch;
	unsigned int i;
	int attached = 0;

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

	VectorCopy(cl.snap.ps.origin, eye);
	eye[2] += cl.snap.ps.viewheight;
	yaw = cl.snap.ps.viewangles[YAW] * (float) M_PI / 180.0f;
	pitch = cl.snap.ps.viewangles[PITCH] * (float) M_PI / 180.0f;
	dir[0] = cosf(pitch) * cosf(yaw);
	dir[1] = cosf(pitch) * sinf(yaw);
	dir[2] = -sinf(pitch);

	if (!Q3IDE_WM_TraceWall(eye, dir, wall_pos, wall_normal)) {
		Com_Printf("q3ide: no wall found\n");
		if (q3ide_wm.cap_free_dlist)
			q3ide_wm.cap_free_dlist(dlist);
		return;
	}

	for (i = 0; i < dlist.count && (int) i < Q3IDE_MAX_WIN; i++) {
		const Q3ideDisplayInfo *d = &dlist.displays[i];
		float oh = Q3IDE_WIN_INCHES;
		float ow = oh * ((float) d->width / (float) d->height);
		vec3_t pos;

		if (q3ide_wm.cap_start_disp(q3ide_wm.cap, d->display_id, Q3IDE_CAPTURE_FPS) != 0) {
			Com_Printf("q3ide: display %u start failed\n", d->display_id);
			continue;
		}

		VectorCopy(wall_pos, pos);
		pos[2] = eye[2] + (float) i * (oh + 8.0f) - (float) (dlist.count - 1) * (oh + 8.0f) * 0.5f;

		if (Q3IDE_WM_Attach(d->display_id, pos, wall_normal, ow, oh, qfalse)) {
			attached++;
			Com_Printf("q3ide: display %u (%ux%u) %.0fx%.0f\n", d->display_id, d->width, d->height, ow, oh);
		}
	}

	if (q3ide_wm.cap_free_dlist)
		q3ide_wm.cap_free_dlist(dlist);
	Com_Printf("q3ide: desktop: %d display(s)\n", attached);
}

/* ── CmdSnap — snap wins[0] into the q3dm0 teleporter ── */
void Q3IDE_WM_CmdSnap(void)
{
	vec3_t pos = {-1152.0f, -1868.0f, 84.0f};
	vec3_t normal = {0.0f, 1.0f, 0.0f};

	if (!q3ide_wm.wins[0].active) {
		Com_Printf("q3ide: snap — attach a window first\n");
		return;
	}
	VectorCopy(pos, q3ide_wm.wins[0].origin);
	VectorCopy(normal, q3ide_wm.wins[0].normal);
	q3ide_wm.wins[0].world_w = 128.0f;
	q3ide_wm.wins[0].world_h = 128.0f;
	Com_Printf("q3ide: snapped win[0] into teleporter at (%.0f,%.0f,%.0f)\n", pos[0], pos[1], pos[2]);
}
