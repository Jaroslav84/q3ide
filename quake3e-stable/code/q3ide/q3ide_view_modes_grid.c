/*
 * q3ide_view_modes_grid.c — Overview grid layout algorithm.
 * Called every frame while overview is active.
 */

#include "q3ide_view_modes.h"
#include "../client/client.h"
#include "../qcommon/qcommon.h"
#include "q3ide_engine_hooks.h"
#include "q3ide_params.h"
#include "q3ide_win_mngr.h"
#include "q3ide_win_mngr_internal.h"

/* Shared overview state — defined in q3ide_view_modes_overview.c */
typedef struct {
	qboolean active;
	vec3_t origin[Q3IDE_MAX_WIN];
	vec3_t normal[Q3IDE_MAX_WIN];
	float world_w[Q3IDE_MAX_WIN];
	float world_h[Q3IDE_MAX_WIN];
} win_snapshot_t;

extern win_snapshot_t g_ov;
extern qboolean g_ov_placed;

/* Helpers from other overview/focus3 TUs */
extern qboolean q3ide_player_axes(vec3_t eye, vec3_t fwd, vec3_t right);
extern void q3ide_focus3_place_arc(vec3_t eye, vec3_t fwd, int *idxs, int n, float pitch_rad);

/* App category lists — q3ide_attach_filter.c */
extern const char *q3ide_terminal_apps[];
extern const char *q3ide_browser_apps[];
extern qboolean q3ide_match(const char *app, const char **list);

/* ── Window snapshot ─────────────────────────────────────────────────── */

void q3ide_ov_snapshot_save(win_snapshot_t *s)
{
	int i;
	for (i = 0; i < Q3IDE_MAX_WIN; i++) {
		VectorCopy(q3ide_wm.wins[i].origin, s->origin[i]);
		VectorCopy(q3ide_wm.wins[i].normal, s->normal[i]);
		s->world_w[i] = q3ide_wm.wins[i].world_w;
		s->world_h[i] = q3ide_wm.wins[i].world_h;
	}
	s->active = qtrue;
}

/* Category: 0=display, 1=terminal, 2=browser, 3=other */
static int ov_win_category(const q3ide_win_t *w)
{
	if (w->is_tunnel && !w->owns_stream)
		return 0;
	if (q3ide_match(w->label, q3ide_terminal_apps))
		return 1;
	if (q3ide_match(w->label, q3ide_browser_apps))
		return 2;
	return 3;
}

/* ── Overview layout (re-run every frame while active) ──────────────── */

void q3ide_overview_layout(void)
{
	int i, k, n, row_n;
	int active_idx[Q3IDE_MAX_WIN];
	int row_idxs[3];
	float cell_h, wh, z_off;
	vec3_t eye, fwd, right, row_eye;
	(void) right;

	n = 0;
	for (i = 0; i < Q3IDE_MAX_WIN; i++)
		if (q3ide_wm.wins[i].active)
			active_idx[n++] = i;

	if (n == 0 || !q3ide_player_axes(eye, fwd, right))
		return;

	/* Stable insertion sort by category */
	for (i = 1; i < n; i++) {
		int tmp = active_idx[i];
		int ci = ov_win_category(&q3ide_wm.wins[tmp]);
		int j = i - 1;
		while (j >= 0 && ov_win_category(&q3ide_wm.wins[active_idx[j]]) > ci) {
			active_idx[j + 1] = active_idx[j];
			j--;
		}
		active_idx[j + 1] = tmp;
	}

	z_off = -(float) cl.snap.ps.viewheight;
	cell_h = 0.0f;
	for (k = 0; k < n; k += 3) {
		row_n = 0;
		row_idxs[row_n++] = active_idx[k];
		if (k + 1 < n)
			row_idxs[row_n++] = active_idx[k + 1];
		if (k + 2 < n)
			row_idxs[row_n++] = active_idx[k + 2];

		wh = 0.0f;
		for (i = 0; i < row_n; i++) {
			int idx = row_idxs[i];
			float h = (g_ov.active && g_ov.world_h[idx] > 0.0f) ? g_ov.world_h[idx] : q3ide_wm.wins[idx].world_h;
			if (h > wh)
				wh = h;
		}
		if (wh < 1.0f)
			wh = 45.0f;

		z_off += cell_h * 0.5f + (cell_h > 0.0f ? Q3IDE_OVERVIEW_GAP : 0.0f) + wh * 0.5f;
		cell_h = wh;

		for (i = 0; i < row_n; i++) {
			int idx = row_idxs[i];
			if (g_ov.active && g_ov.world_w[idx] > 0.0f) {
				q3ide_wm.wins[idx].world_w = g_ov.world_w[idx];
				q3ide_wm.wins[idx].world_h = g_ov.world_h[idx];
			}
			q3ide_wm.wins[idx].in_overview = qtrue;
		}

		VectorCopy(eye, row_eye);
		row_eye[2] += z_off;
		q3ide_focus3_place_arc(row_eye, fwd, row_idxs, row_n, 0.0f);
	}
}
