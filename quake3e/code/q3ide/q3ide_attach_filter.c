/* q3ide_attach_filter.c — App lists, filter helpers, pending-attach queue. */

#include "q3ide_win_mngr.h"
#include "q3ide_log.h"
#include "q3ide_win_mngr_internal.h"
#include "../qcommon/qcommon.h"
#include <math.h>
#include <string.h>

/* ── App category lists ─────────────────────────────────────────── */

const char *q3ide_terminal_apps[] = {"iTerm2", "Terminal", NULL};
const char *q3ide_browser_apps[]  = {"Google Chrome", "Chromium", "Safari",        "Firefox",
                                     "Arc",           "Brave Browser", "Opera", "Microsoft Edge", NULL};

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

/*
 * Returns qtrue for windows that are system UI — not real application content.
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

qboolean Q3IDE_IsSystemJunk(const Q3ideWindowInfo *w)
{
	return q3ide_is_system_junk(w);
}

/* ── Pending queue ─────────────────────────────────────────────── */

typedef struct {
	unsigned int id;
	float        aspect;
	qboolean     is_display;
	char         label[128];
	char         app_name[64];
} q3ide_attach_item_t;

static q3ide_attach_item_t g_pending[Q3IDE_MAX_WIN];
static int                 g_pending_n = 0;

int Q3IDE_WM_PendingCount(void)
{
	return g_pending_n;
}

static qboolean q3ide_attach_one(const q3ide_attach_item_t *it, vec3_t pos, vec3_t norm)
{
	float    ww = Q3IDE_SPAWN_WIN_W;
	float    wh = (it->aspect > 0.001f) ? ww / it->aspect : ww / Q3IDE_DISPLAY_ASPECT;
	qboolean ok;
	int      pi, idx;

	/* ── Panel grouping: same app → add as background window ──── */
	if (!it->is_display && it->app_name[0]) {
		for (pi = 0; pi < Q3IDE_MAX_WIN; pi++) {
			q3ide_win_t *pw = &q3ide_wm.wins[pi];
			if (!pw->active || !pw->app_name[0])
				continue;
			if (pw->window_count < 8 && !Q_stricmp(pw->app_name, it->app_name)) {
				pw->window_ids[pw->window_count++] = it->id;
				Q3IDE_LOGI("group: wid=%u → panel=%d [%s] (%d windows)", it->id, pi, pw->app_name,
				           pw->window_count);
				return qtrue;
			}
		}
	}

	/* ── Create new panel ─────────────────────────────────────── */
	if (it->is_display) {
		if (!q3ide_win_mngr.cap_start_disp ||
		    q3ide_win_mngr.cap_start_disp(q3ide_win_mngr.cap, it->id, Q3IDE_CAPTURE_FPS) != 0)
			return qfalse;
		ok = Q3IDE_WM_Attach(it->id, pos, norm, ww, wh, qfalse, qfalse);
	} else {
		ok = Q3IDE_WM_Attach(it->id, pos, norm, ww, wh, qtrue, qfalse);
	}
	if (ok) {
		idx = Q3IDE_WM_FindById(it->id);
		if (idx >= 0)
			Q_strncpyz(q3ide_wm.wins[idx].app_name, it->app_name, sizeof(q3ide_wm.wins[idx].app_name));
		Q3IDE_WM_SetLabel(it->id, it->label);
	}
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

/* ── Push helpers — called from Q3IDE_WM_CmdAttach ─────────────── */

void q3ide_push_display(unsigned int did, int disp_idx)
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

void q3ide_push_win(const Q3ideWindowInfo *w)
{
	q3ide_attach_item_t it;
	if (g_pending_n >= Q3IDE_MAX_WIN)
		return;
	memset(&it, 0, sizeof(it));
	it.id         = w->window_id;
	it.aspect     = w->height ? (float) w->width / w->height : Q3IDE_DISPLAY_ASPECT;
	it.is_display = qfalse;
	if (w->app_name && w->app_name[0] && w->title && w->title[0])
		Com_sprintf(it.label, sizeof(it.label), "%s: %s", w->app_name, w->title);
	else if (w->title && w->title[0])
		Q_strncpyz(it.label, w->title, sizeof(it.label));
	else
		Q_strncpyz(it.label, w->app_name ? w->app_name : "", sizeof(it.label));
	Q_strncpyz(it.app_name, w->app_name ? w->app_name : "", sizeof(it.app_name));
	g_pending[g_pending_n++] = it;
}

void q3ide_reset_pending(void)
{
	g_pending_n = 0;
}

int q3ide_pending_n(void)
{
	return g_pending_n;
}

/* Compact: remove already-attached items from pending queue. */
void q3ide_compact_pending(void)
{
	int j = 0, k;
	for (k = 0; k < g_pending_n; k++) {
		if (!q3ide_is_attached(g_pending[k].id))
			g_pending[j++] = g_pending[k];
	}
	g_pending_n = j;
}

/* Log the full pending queue. */
void q3ide_log_pending(void)
{
	int k;
	for (k = 0; k < g_pending_n; k++)
		Q3IDE_LOGI("queue[%d]: '%s' id=%u %s", k, g_pending[k].label, g_pending[k].id,
		           g_pending[k].is_display ? "(display)" : "");
}

/* Attach first n items from queue. Returns number attached. */
int q3ide_flush_pending(int n, vec3_t pos, vec3_t norm)
{
	int to_attach = (n <= 0 || n >= g_pending_n) ? g_pending_n : n;
	int j, attached = 0;
	for (j = 0; j < to_attach && g_pending_n > 0; j++) {
		if (q3ide_attach_one(&g_pending[0], pos, norm))
			attached++;
		memmove(g_pending, g_pending + 1, (size_t)(--g_pending_n) * sizeof(q3ide_attach_item_t));
	}
	return attached;
}
