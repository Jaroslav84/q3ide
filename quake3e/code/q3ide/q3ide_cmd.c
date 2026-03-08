/*
 * q3ide_cmd.c — Q3IDE attach/desktop commands.
 *
 * "q3ide attach":
 *   Scans 12 directions to find unique walls, then distributes all windows
 *   side-by-side (centered) on each wall. Round-robin wall assignment.
 * "q3ide desktop" — mirrors all displays on the nearest wall.
 */

#include "q3ide_wm.h"
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

/* ── DetachById ─────────────────────────────────────────────────── */

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

/* ── CmdAttach ──────────────────────────────────────────────────── */

/* Number of directions to scan for unique walls (every 30 degrees) */
#define Q3IDE_SCAN_DIRS 12

void Q3IDE_WM_CmdAttach(void)
{
	Q3ideWindowList wlist;
	unsigned int win_ids[Q3IDE_MAX_WIN];
	float win_asp[Q3IDE_MAX_WIN];
	int win_n = 0;
	int i, j, attached = 0;
	vec3_t eye, dir;
	float yaw;

	/* Wall discovery state */
	vec3_t wall_pos[Q3IDE_SCAN_DIRS], wall_norm[Q3IDE_SCAN_DIRS];
	int wall_win[Q3IDE_SCAN_DIRS][Q3IDE_MAX_WIN]; /* window indices per wall */
	int wall_wcnt[Q3IDE_SCAN_DIRS];
	int n_walls = 0;

	if (!q3ide_wm.cap || !q3ide_wm.cap_list_wins) {
		Com_Printf("q3ide: not ready\n");
		return;
	}

	Q3IDE_WM_CmdDetachAll();

	wlist = q3ide_wm.cap_list_wins(q3ide_wm.cap);
	if (!wlist.windows || !wlist.count) {
		Com_Printf("q3ide: no windows found\n");
		return;
	}

	/* Collect all terminal + browser windows */
	for (i = 0; i < (int) wlist.count && win_n < Q3IDE_MAX_WIN; i++) {
		const Q3ideWindowInfo *w = &wlist.windows[i];
		qboolean dupe = qfalse;
		if ((int) w->width < Q3IDE_MIN_WIN_W || (int) w->height < Q3IDE_MIN_WIN_H)
			continue;
		if (!q3ide_match(w->app_name, q3ide_terminal_apps) && !q3ide_match(w->app_name, q3ide_browser_apps))
			continue;
		for (j = 0; j < win_n; j++)
			if (win_ids[j] == w->window_id) {
				dupe = qtrue;
				break;
			}
		if (dupe)
			continue;
		win_asp[win_n] = w->height ? (float) w->width / w->height : 16.0f / 9.0f;
		win_ids[win_n++] = w->window_id;
		Com_Printf("q3ide: found [%d] wid=%u \"%s\" %ux%u\n", win_n - 1, w->window_id, w->app_name, w->width,
		           w->height);
	}
	if (q3ide_wm.cap_free_wlist)
		q3ide_wm.cap_free_wlist(wlist);

	if (!win_n) {
		Com_Printf("q3ide: no iTerm2/Terminal/browser windows found\n");
		return;
	}

	VectorCopy(cl.snap.ps.origin, eye);
	eye[2] += cl.snap.ps.viewheight;
	yaw = cl.snap.ps.viewangles[YAW] * (float) M_PI / 180.0f;

	/* Scan Q3IDE_SCAN_DIRS evenly-spaced horizontal directions for unique walls */
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
		/* Deduplicate by wall normal: skip if we already have a wall facing the same way */
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

	/* Assign windows round-robin to walls */
	memset(wall_win, 0, sizeof(wall_win));
	for (i = 0; i < win_n; i++) {
		int wi = i % n_walls;
		wall_win[wi][wall_wcnt[wi]++] = i;
	}

	/* Place each wall's windows side-by-side, horizontally centered */
	for (i = 0; i < n_walls; i++) {
		vec3_t right;
		float nx, ny, horiz_len, total_w, gap, x_cursor;
		int cnt = wall_wcnt[i];

		if (!cnt)
			continue;

		/* Compute right vector for this wall (same basis as q3ide_add_poly) */
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

		/* Compute total span = sum of widths + gaps between */
		gap = 16.0f;
		total_w = 0.0f;
		for (j = 0; j < cnt; j++) {
			float oh = Q3IDE_WIN_INCHES;
			total_w += oh * win_asp[wall_win[i][j]];
		}
		total_w += gap * (float) (cnt - 1);

		/* Place windows centered on the wall hit point */
		x_cursor = -total_w * 0.5f;
		for (j = 0; j < cnt; j++) {
			int wi = wall_win[i][j];
			float oh = Q3IDE_WIN_INCHES;
			float ow = oh * win_asp[wi];
			float cx = x_cursor + ow * 0.5f;
			vec3_t pos;
			pos[0] = wall_pos[i][0] + right[0] * cx;
			pos[1] = wall_pos[i][1] + right[1] * cx;
			pos[2] = eye[2]; /* eye height */
			if (Q3IDE_WM_Attach(win_ids[wi], pos, wall_norm[i], ow, oh, qtrue)) {
				attached++;
				Com_Printf("q3ide: win [%d] wall %d x=%.0f\n", wi, i, cx);
			} else {
				Com_Printf("q3ide: win [%d] attach failed\n", wi);
			}
			x_cursor += ow + gap;
		}
	}

	q3ide_wm.auto_attach = qtrue; /* live-track new windows from here on */
	Com_Printf("q3ide: attached %d/%d windows\n", attached, win_n);
}

/* ── Auto-place a newly opened window on the nearest free wall ───── */

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

/* ── CmdSnap — snap wins[0] into the q3dm0 first-room teleporter ──
 *
 * BSP model *9 (trigger_teleport) center: (-1152, -1876, 66).
 * Thin in Y (8 units). Energy plane sits at the front face (y≈-1868),
 * normal (0,1,0) facing the player who approaches from positive Y.
 * Size 128×128 matches the standard Q3 teleporter arch opening.
 */
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
