/*
 * q3ide_cmd.c — Q3IDE attach/desktop commands.
 *
 * "q3ide attach":
 *   - Terminal apps (iTerm2, Terminal) → float in air, spread side-by-side
 *   - Browser apps (Chrome, Safari, …) → placed on walls around player
 * "q3ide desktop" — mirrors all displays on the nearest wall.
 */

#include "q3ide_wm.h"
#include "q3ide_wm_internal.h"
#include "../qcommon/qcommon.h"
#include "../client/client.h"
#include <math.h>
#include <string.h>

/* ── App classification ─────────────────────────────────────────── */

static const char *q3ide_terminal_apps[] = { "iTerm2", "Terminal", NULL };

static const char *q3ide_browser_apps[] = {
	"Google Chrome", "Chromium", "Safari", "Firefox",
	"Arc",           "Brave Browser", "Opera", "Microsoft Edge",
	NULL,
};

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

/* ── Wall yaw offsets (browsers) ────────────────────────────────── */

static const float q3ide_yaw_offsets[] = { 0.0f, -45.0f, 45.0f, -90.0f,
	                                        90.0f, -135.0f, 135.0f, 180.0f };
#define Q3IDE_NUM_OFFSETS 8

/* ── Floating placement (terminals) ────────────────────────────────
 * Places N windows side-by-side in the air in front of the player.
 * dist   — how far forward (Quake units)
 * height — Z offset from eye
 * idx    — which window in the row (0-based)
 * total  — total windows in this row
 */
static void q3ide_attach_floating(unsigned int id, float aspect, vec3_t eye, float yaw, int idx,
                                  int total, float dist, float height_offset)
{
	float oh = Q3IDE_WIN_INCHES;
	float ow = oh * aspect;
	float fwd_x = cosf(yaw);
	float fwd_y = sinf(yaw);
	/* vertical stack: spread windows up/down in Z */
	float z_spread = (float)(idx - (total - 1) * 0.5f) * (oh + 8.0f);
	vec3_t pos, normal;

	pos[0] = eye[0] + fwd_x * dist;
	pos[1] = eye[1] + fwd_y * dist;
	pos[2] = eye[2] + height_offset + z_spread;

	/* normal faces back toward player */
	normal[0] = -fwd_x;
	normal[1] = -fwd_y;
	normal[2] = 0.0f;

	if (Q3IDE_WM_Attach(id, pos, normal, ow, oh, qtrue))
		Com_Printf("q3ide: terminal [%d/%d] floating dist=%.0f\n", idx + 1, total, dist);
	else
		Com_Printf("q3ide: terminal [%d/%d] attach failed\n", idx + 1, total);
}

/* ── CmdAttach ──────────────────────────────────────────────────── */

void Q3IDE_WM_CmdAttach(void)
{
	Q3ideWindowList wlist;
	/* separate buckets for terminals and browsers */
	unsigned int term_ids[Q3IDE_MAX_WIN], brow_ids[Q3IDE_MAX_WIN];
	float term_asp[Q3IDE_MAX_WIN], brow_asp[Q3IDE_MAX_WIN];
	int term_n = 0, brow_n = 0;
	int i, j, attached = 0;
	vec3_t eye, wall_pos, wall_normal, dir;
	float yaw, pitch;

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

	/* Bucket windows into terminals / browsers */
	for (i = 0; i < (int)wlist.count; i++) {
		const Q3ideWindowInfo *w = &wlist.windows[i];
		qboolean dupe = qfalse;
		if ((int)w->width < Q3IDE_MIN_WIN_W || (int)w->height < Q3IDE_MIN_WIN_H)
			continue;
		/* dedup by window id across both buckets */
		for (j = 0; j < term_n; j++)
			if (term_ids[j] == w->window_id) {
				dupe = qtrue;
				break;
			}
		for (j = 0; j < brow_n && !dupe; j++)
			if (brow_ids[j] == w->window_id) {
				dupe = qtrue;
				break;
			}
		if (dupe)
			continue;

		if (q3ide_match(w->app_name, q3ide_terminal_apps) && term_n < Q3IDE_MAX_WIN) {
			term_asp[term_n] = w->height ? (float)w->width / w->height : 16.0f / 9.0f;
			term_ids[term_n++] = w->window_id;
			Com_Printf("q3ide: terminal [%d] wid=%u \"%s\" %ux%u\n", term_n - 1,
			           w->window_id, w->app_name, w->width, w->height);
		} else if (q3ide_match(w->app_name, q3ide_browser_apps) && brow_n < Q3IDE_MAX_WIN) {
			brow_asp[brow_n] = w->height ? (float)w->width / w->height : 16.0f / 9.0f;
			brow_ids[brow_n++] = w->window_id;
			Com_Printf("q3ide: browser  [%d] wid=%u \"%s\" %ux%u\n", brow_n - 1,
			           w->window_id, w->app_name, w->width, w->height);
		}
	}
	if (q3ide_wm.cap_free_wlist)
		q3ide_wm.cap_free_wlist(wlist);

	if (!term_n && !brow_n) {
		Com_Printf("q3ide: no iTerm2/Terminal/browser windows found\n");
		return;
	}

	VectorCopy(cl.snap.ps.origin, eye);
	eye[2] += cl.snap.ps.viewheight;
	yaw = cl.snap.ps.viewangles[YAW] * (float)M_PI / 180.0f;
	pitch = cl.snap.ps.viewangles[PITCH] * (float)M_PI / 180.0f;

	/* Terminals — float in air, side-by-side, 300 units forward */
	for (i = 0; i < term_n; i++) {
		q3ide_attach_floating(term_ids[i], term_asp[i], eye, yaw, i, term_n, 300.0f, 0.0f);
		attached++;
	}

	/* Browsers — place on walls, round-robin yaw offsets */
	for (i = 0; i < brow_n; i++) {
		float oh = Q3IDE_WIN_INCHES;
		float ow = oh * brow_asp[i];
		int t;
		int base = i % Q3IDE_NUM_OFFSETS;
		for (t = 0; t < Q3IDE_NUM_OFFSETS; t++) {
			int slot = (base + t) % Q3IDE_NUM_OFFSETS;
			float yt = yaw + q3ide_yaw_offsets[slot] * (float)M_PI / 180.0f;
			dir[0] = cosf(pitch) * cosf(yt);
			dir[1] = cosf(pitch) * sinf(yt);
			dir[2] = -sinf(pitch) * 0.2f;
			if (!Q3IDE_WM_TraceWall(eye, dir, wall_pos, wall_normal))
				continue;
			wall_pos[2] = eye[2];
			if (Q3IDE_WM_Attach(brow_ids[i], wall_pos, wall_normal, ow, oh, qtrue)) {
				attached++;
				Com_Printf("q3ide: browser  [%d] on wall yaw+%.0f\n", i,
				           q3ide_yaw_offsets[slot]);
				break;
			}
		}
	}

	Com_Printf("q3ide: attached %d windows (%d terminal, %d browser)\n", attached, term_n,
	           brow_n);
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
	yaw = cl.snap.ps.viewangles[YAW] * (float)M_PI / 180.0f;
	pitch = cl.snap.ps.viewangles[PITCH] * (float)M_PI / 180.0f;
	dir[0] = cosf(pitch) * cosf(yaw);
	dir[1] = cosf(pitch) * sinf(yaw);
	dir[2] = -sinf(pitch);

	if (!Q3IDE_WM_TraceWall(eye, dir, wall_pos, wall_normal)) {
		Com_Printf("q3ide: no wall found\n");
		if (q3ide_wm.cap_free_dlist)
			q3ide_wm.cap_free_dlist(dlist);
		return;
	}

	for (i = 0; i < dlist.count && (int)i < Q3IDE_MAX_WIN; i++) {
		const Q3ideDisplayInfo *d = &dlist.displays[i];
		float oh = Q3IDE_WIN_INCHES;
		float ow = oh * ((float)d->width / (float)d->height);
		vec3_t pos;

		if (q3ide_wm.cap_start_disp(q3ide_wm.cap, d->display_id, Q3IDE_CAPTURE_FPS) != 0) {
			Com_Printf("q3ide: display %u start failed\n", d->display_id);
			continue;
		}

		VectorCopy(wall_pos, pos);
		pos[2] = eye[2] + (float)i * (oh + 8.0f) -
		         (float)(dlist.count - 1) * (oh + 8.0f) * 0.5f;

		if (Q3IDE_WM_Attach(d->display_id, pos, wall_normal, ow, oh, qfalse)) {
			attached++;
			Com_Printf("q3ide: display %u (%ux%u) %.0fx%.0f\n", d->display_id, d->width,
			           d->height, ow, oh);
		}
	}

	if (q3ide_wm.cap_free_dlist)
		q3ide_wm.cap_free_dlist(dlist);
	Com_Printf("q3ide: desktop: %d display(s)\n", attached);
}
