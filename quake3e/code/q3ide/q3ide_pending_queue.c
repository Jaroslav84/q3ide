/* q3ide_pending_queue.c — Pending-attach queue: populate, enqueue, drain. */

#include "q3ide_win_mngr.h"
#include "q3ide_log.h"
#include "q3ide_params.h"
#include "q3ide_win_mngr_internal.h"
#include "../qcommon/qcommon.h"
#include <string.h>

/* From q3ide_attach_filter.c */
extern qboolean q3ide_is_attached(unsigned int id);
extern qboolean Q3IDE_IsSystemJunk(const Q3ideWindowInfo *w);
extern qboolean q3ide_match(const char *app, const char **list);
extern const char *q3ide_terminal_apps[];
extern const char *q3ide_browser_apps[];

/* ── Queue storage ───────────────────────────────────────────────────── */

typedef struct {
	unsigned int id;
	float aspect;
	qboolean is_display;
	char label[128];
} q3ide_attach_item_t;

static q3ide_attach_item_t g_pending[Q3IDE_MAX_WIN];
static int g_pending_n = 0;

/* ── Enqueue a single window (from PollChanges is_added==1) ─────────── */

void Q3IDE_WM_EnqueueWindow(unsigned int id, float aspect, const char *label)
{
	int i;
	if (g_pending_n >= Q3IDE_MAX_WIN)
		return;
	if (q3ide_is_attached(id))
		return;
	for (i = 0; i < g_pending_n; i++)
		if (g_pending[i].id == id)
			return; /* already queued */
	g_pending[g_pending_n].id = id;
	g_pending[g_pending_n].is_display = qfalse;
	g_pending[g_pending_n].aspect = (aspect > 0.001f) ? aspect : 16.0f / 9.0f;
	Q_strncpyz(g_pending[g_pending_n].label, label ? label : "Window", sizeof(g_pending[g_pending_n].label));
	g_pending_n++;
}

int Q3IDE_WM_PendingCount(void)
{
	return g_pending_n;
}

/* ── Attach one pending item ─────────────────────────────────────────── */

static qboolean q3ide_attach_one(const q3ide_attach_item_t *it, vec3_t pos, vec3_t norm)
{
	float ww = Q3IDE_SPAWN_WIN_W;
	float wh = (it->aspect > 0.001f) ? ww / it->aspect : ww / Q3IDE_DISPLAY_ASPECT;
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

/* ── Populate queue from OS ──────────────────────────────────────────── */
/*
 * Fills g_pending[] in priority order:
 *   Displays : Center → Left → Right (per Q3IDE_TRIMON_IDX_* constants)
 *   Apps     : Terminals → Browsers → Others
 * Skips already-attached and already-queued items.
 */
void Q3IDE_WM_PopulateQueue(qboolean displays_only)
{
	unsigned int i;
	g_pending_n = 0;

	if (!q3ide_wm.cap)
		return;

	/* ── Displays: Center → Left → Right, then extras ───────────── */
	if (q3ide_wm.cap_list_disp && q3ide_wm.cap_free_dlist) {
		static const int kOrder[3] = {Q3IDE_TRIMON_IDX_CENTER, Q3IDE_TRIMON_IDX_LEFT, Q3IDE_TRIMON_IDX_RIGHT};
		static const char *kLabel[3] = {"Display Center", "Display Left", "Display Right"};
		Q3ideDisplayList dl = q3ide_wm.cap_list_disp(q3ide_wm.cap);
		int oi;

		for (oi = 0; oi < 3 && g_pending_n < Q3IDE_MAX_WIN; oi++) {
			int idx = kOrder[oi];
			const Q3ideDisplayInfo *d;
			if (idx < 0 || (unsigned int) idx >= dl.count)
				continue;
			d = &dl.displays[idx];
			if (q3ide_is_attached(d->display_id))
				continue;
			g_pending[g_pending_n].id = d->display_id;
			g_pending[g_pending_n].is_display = qtrue;
			g_pending[g_pending_n].aspect =
			    (d->height > 0) ? (float) d->width / (float) d->height : Q3IDE_DISPLAY_ASPECT;
			Q_strncpyz(g_pending[g_pending_n].label, kLabel[oi], sizeof(g_pending[g_pending_n].label));
			g_pending_n++;
		}
		for (i = 0; i < dl.count && g_pending_n < Q3IDE_MAX_WIN; i++) {
			const Q3ideDisplayInfo *d;
			if ((int) i == Q3IDE_TRIMON_IDX_CENTER || (int) i == Q3IDE_TRIMON_IDX_LEFT ||
			    (int) i == Q3IDE_TRIMON_IDX_RIGHT)
				continue;
			d = &dl.displays[i];
			if (q3ide_is_attached(d->display_id))
				continue;
			g_pending[g_pending_n].id = d->display_id;
			g_pending[g_pending_n].is_display = qtrue;
			g_pending[g_pending_n].aspect =
			    (d->height > 0) ? (float) d->width / (float) d->height : Q3IDE_DISPLAY_ASPECT;
			Q_strncpyz(g_pending[g_pending_n].label, "Display", sizeof(g_pending[g_pending_n].label));
			g_pending_n++;
		}
		q3ide_wm.cap_free_dlist(dl);
	}

	if (displays_only)
		return;

	/* ── App windows: Terminals → Browsers → Others ─────────────── */
	if (q3ide_wm.cap_list_wins && q3ide_wm.cap_free_wlist) {
		Q3ideWindowList wl = q3ide_wm.cap_list_wins(q3ide_wm.cap);
		int pass;
		for (pass = 0; pass < 3 && g_pending_n < Q3IDE_MAX_WIN; pass++) {
			for (i = 0; i < wl.count && g_pending_n < Q3IDE_MAX_WIN; i++) {
				const Q3ideWindowInfo *w = &wl.windows[i];
				qboolean is_term, is_browser;
				if (Q3IDE_IsSystemJunk(w) || q3ide_is_attached(w->window_id))
					continue;
				if ((int) w->width < Q3IDE_MIN_WIN_W || (int) w->height < Q3IDE_MIN_WIN_H)
					continue;
				is_term = q3ide_match(w->app_name, q3ide_terminal_apps);
				is_browser = q3ide_match(w->app_name, q3ide_browser_apps);
				if (pass == 0 && !is_term)
					continue;
				if (pass == 1 && !is_browser)
					continue;
				if (pass == 2 && (is_term || is_browser))
					continue;
				g_pending[g_pending_n].id = w->window_id;
				g_pending[g_pending_n].is_display = qfalse;
				g_pending[g_pending_n].aspect =
				    (w->height > 0) ? (float) w->width / (float) w->height : Q3IDE_DISPLAY_ASPECT;
				if (w->app_name && w->app_name[0] && w->title && w->title[0])
					Com_sprintf(g_pending[g_pending_n].label, sizeof(g_pending[g_pending_n].label), "%s: %s",
					            w->app_name, w->title);
				else if (w->title && w->title[0])
					Q_strncpyz(g_pending[g_pending_n].label, w->title, sizeof(g_pending[g_pending_n].label));
				else if (w->app_name && w->app_name[0])
					Q_strncpyz(g_pending[g_pending_n].label, w->app_name, sizeof(g_pending[g_pending_n].label));
				else
					Q_strncpyz(g_pending[g_pending_n].label, "Window", sizeof(g_pending[g_pending_n].label));
				g_pending_n++;
			}
		}
		q3ide_wm.cap_free_wlist(wl);
	}
}

/* Drain entire queue into the scene at pos/norm. Returns count attached. */
int Q3IDE_WM_FlushAllPending(vec3_t pos, vec3_t norm)
{
	int count = 0;
	while (g_pending_n > 0)
		if (Q3IDE_WM_AttachNextPending(pos, norm))
			count++;
	return count;
}

/* Pop up to Q3IDE_ATTACH_MAX_SKIP items until one succeeds or queue empties. */
qboolean Q3IDE_WM_AttachNextPending(vec3_t pos, vec3_t norm)
{
	int tries;
	for (tries = 0; tries < Q3IDE_ATTACH_MAX_SKIP && g_pending_n > 0; tries++) {
		q3ide_attach_item_t it = g_pending[0];
		g_pending_n--;
		memmove(g_pending, g_pending + 1, (size_t) g_pending_n * sizeof(q3ide_attach_item_t));
		if (q3ide_attach_one(&it, pos, norm)) {
			Q3IDE_LOGI("spawn: placed '%s' id=%u  %d remaining", it.label, it.id, g_pending_n);
			return qtrue;
		}
		Q3IDE_LOGI("spawn: skip '%s' id=%u (cap failed)", it.label, it.id);
	}
	return qfalse;
}
