/* q3ide_commands_attach.c — Q3IDE_WM_CmdAttach: enumerate + attach windows.
 *
 * Queue order (each item = exactly 1 window placed per wall-shot):
 *   1. Monitors  — one whole 16:9 capture per physical display, full UV
 *   2. Terminals
 *   3. Browsers
 *   4. Other app windows
 *
 * App windows use actual macOS width/height for aspect ratio.
 * Displays are treated as plain windows: fixed 16:9, full UV (0..1), no slicing.
 *
 * q3ide_spawn_count (cvar): 0 = all immediately, N = N at spawn, rest on wall-shots.
 */

#include "q3ide_win_mngr.h"
#include "q3ide_log.h"
#include "q3ide_win_mngr_internal.h"
#include "../qcommon/qcommon.h"
#include "../client/client.h"
#include <math.h>
#include <string.h>

extern qboolean     q3ide_is_attached(unsigned int id);
extern const char  *q3ide_terminal_apps[];
extern const char  *q3ide_browser_apps[];
extern qboolean     q3ide_match(const char *app, const char **list);

typedef struct {
	unsigned int id;
	float        aspect;
	qboolean     is_display;
	char         label[128];
	char         app_name[64];
} q3ide_attach_item_t;

/* ── Pending queue ─────────────────────────────────────────────── */
static q3ide_attach_item_t g_pending[Q3IDE_MAX_WIN];
static int                 g_pending_n = 0;

int Q3IDE_WM_PendingCount(void)
{
	return g_pending_n;
}

static qboolean q3ide_item_is_attached(const q3ide_attach_item_t *it)
{
	return q3ide_is_attached(it->id);
}

static qboolean q3ide_attach_one(const q3ide_attach_item_t *it, vec3_t pos, vec3_t norm)
{
	float    ww = Q3IDE_SPAWN_WIN_W;
	float    wh = (it->aspect > 0.001f) ? ww / it->aspect : ww / Q3IDE_DISPLAY_ASPECT;
	qboolean ok;

	if (it->is_display) {
		if (!q3ide_win_mngr.cap_start_disp ||
		    q3ide_win_mngr.cap_start_disp(q3ide_win_mngr.cap, it->id, Q3IDE_CAPTURE_FPS) != 0)
			return qfalse;
		ok = Q3IDE_WM_Attach(it->id, pos, norm, ww, wh, qfalse, qfalse);
	} else {
		ok = Q3IDE_WM_Attach(it->id, pos, norm, ww, wh, qtrue, qfalse);
	}
	if (ok)
		Q3IDE_WM_SetLabel(it->id, it->label);
	return ok;
}

/* Pop up to Q3IDE_ATTACH_MAX_SKIP items until one succeeds or queue empties. */
qboolean Q3IDE_WM_AttachNextPending(vec3_t pos, vec3_t norm)
{
	int tries;
	for (tries = 0; tries < Q3IDE_ATTACH_MAX_SKIP && g_pending_n > 0; tries++) {
		q3ide_attach_item_t it = g_pending[0];
		memmove(g_pending, g_pending + 1, (size_t)(--g_pending_n) * sizeof(q3ide_attach_item_t));
		if (q3ide_attach_one(&it, pos, norm)) {
			Q3IDE_LOGI("spawn: placed '%s' id=%u  %d remaining", it.label, it.id, g_pending_n);
			return qtrue;
		}
		Q3IDE_LOGI("spawn: skip '%s' id=%u (cap failed)", it.label, it.id);
	}
	return qfalse;
}

/* ── Helpers to push items into g_pending[] ─────────────────────── */

static void q3ide_push_display(unsigned int did, int disp_idx)
{
	q3ide_attach_item_t it;
	if (g_pending_n >= Q3IDE_MAX_WIN)
		return;
	memset(&it, 0, sizeof(it));
	it.id         = did;
	it.aspect     = Q3IDE_DISPLAY_ASPECT;
	it.is_display = qtrue;
	Com_sprintf(it.label, sizeof(it.label), "Display %d", disp_idx + 1);
	g_pending[g_pending_n++] = it;
}

/*
 * Returns qtrue for windows that are system UI — not real application content.
 * Keeps: real apps, productivity tools, Finder file-browser windows, widgets.
 * Drops: Wallpaper layers, Dock, loginwindow, Spotlight, menu-bar agents, dialogs.
 *
 * Many system windows have an empty title; their app_name carries the name.
 * So we check both fields for every pattern.
 */
static qboolean q3ide_is_system_junk(const Q3ideWindowInfo *w)
{
	const char *t = w->title ? w->title : "";
	const char *a = w->app_name ? w->app_name : "";

	if (Q_stristr(t, "Wallpaper-") || Q_stristr(a, "Wallpaper"))
		return qtrue;
	if (!Q_stricmp(t, "Dock") || !Q_stricmp(a, "Dock"))
		return qtrue;
	if (!Q_stricmp(t, "LPSpringboard") || !Q_stricmp(a, "LPSpringboard"))
		return qtrue;
	if (!Q_stricmp(t, "loginwindow") || !Q_stricmp(a, "loginwindow"))
		return qtrue;
	if (Q_stristr(t, "Accessibility Services") || Q_stristr(a, "Accessibility Services"))
		return qtrue;
	if (!Q_stricmp(t, "Spotlight") || !Q_stricmp(a, "Spotlight"))
		return qtrue;
	if (!Q_stricmp(t, "Notification Center") || !Q_stricmp(a, "NotificationCenter"))
		return qtrue;
	if (Q_stristr(t, "Open and Save Panel Service") || Q_stristr(a, "Open and Save Panel Service"))
		return qtrue;
	if (Q_stristr(t, "Screen & System Audio Recording") || Q_stristr(a, "Screen & System Audio Recording"))
		return qtrue;
	if (!Q_stricmp(a, "WindowServer"))
		return qtrue;
	if (!Q_stricmp(t, "Desktop") || !Q_stricmp(a, "Desktop"))
		return qtrue; /* Finder desktop layers */
	if (!Q_stricmp(t, "Little Snitch Agent") || !Q_stricmp(a, "Little Snitch Agent"))
		return qtrue;
	if (Q_stristr(t, "Keyboard Maestro Engine") || Q_stristr(a, "Keyboard Maestro Engine"))
		return qtrue;
	if (!Q_stricmp(a, "Default Folder X"))
		return qtrue;
	/* Exclude the game itself — capturing the Quake window inside itself kills FPS */
	if (Q_stristr(t, "Quake 3") || Q_stristr(a, "Quake3"))
		return qtrue;

	return qfalse;
}

static void q3ide_push_win(const Q3ideWindowInfo *w)
{
	q3ide_attach_item_t it;
	if (g_pending_n >= Q3IDE_MAX_WIN)
		return;
	memset(&it, 0, sizeof(it));
	it.id         = w->window_id;
	it.aspect     = w->height ? (float) w->width / w->height : Q3IDE_DISPLAY_ASPECT;
	it.is_display = qfalse;
	/* Format label as "AppName: Title" for overlay display */
	if (w->app_name && w->app_name[0] && w->title && w->title[0])
		Com_sprintf(it.label, sizeof(it.label), "%s: %s", w->app_name, w->title);
	else if (w->title && w->title[0])
		Q_strncpyz(it.label, w->title, sizeof(it.label));
	else
		Q_strncpyz(it.label, w->app_name ? w->app_name : "", sizeof(it.label));
	Q_strncpyz(it.app_name, w->app_name ? w->app_name : "", sizeof(it.app_name));
	g_pending[g_pending_n++] = it;
}

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
			if (q3ide_is_system_junk(w))
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
	g_pending_n = 0;

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
				if (q3ide_is_system_junk(w))
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

	/* Drop already-attached items. */
	{
		int j = 0, k;
		for (k = 0; k < g_pending_n; k++) {
			if (!q3ide_item_is_attached(&g_pending[k]))
				g_pending[j++] = g_pending[k];
		}
		g_pending_n = j;
	}

	/* Log pending queue so the user can see what's queued */
	{
		int k;
		for (k = 0; k < g_pending_n; k++)
			Q3IDE_LOGI("queue[%d]: '%s' id=%u %s", k, g_pending[k].label, g_pending[k].id,
			           g_pending[k].is_display ? "(display)" : "");
	}

	/* Attach first spawn_n from queue; rest stay pending for wall-shots. */
	{
		int spawn_n   = Cvar_VariableIntegerValue("q3ide_spawn_count");
		int to_attach = (spawn_n <= 0 || spawn_n >= g_pending_n) ? g_pending_n : spawn_n;
		int j;
		Q3IDE_LOGI("attach: spawn_count=%d  to_attach=%d  pending=%d", spawn_n, to_attach, g_pending_n);
		for (j = 0; j < to_attach && g_pending_n > 0; j++) {
			q3ide_attach_one(&g_pending[0], pos, norm);
			memmove(g_pending, g_pending + 1, (size_t)(--g_pending_n) * sizeof(q3ide_attach_item_t));
		}
	}

	q3ide_wm.auto_attach = qtrue;
	Com_Printf("q3ide: attached %d, %d pending\n", q3ide_wm.num_active, g_pending_n);
	Q3IDE_Eventf("attach_done", "\"attached\":%d,\"pending\":%d", q3ide_wm.num_active, g_pending_n);
}
