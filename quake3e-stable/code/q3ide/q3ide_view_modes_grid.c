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
extern qboolean g_ov_active;
extern qboolean g_ov_placed;
extern float g_ov_scroll_z;

/* Helpers from other overview/focus3 TUs */
extern qboolean q3ide_player_axes(vec3_t eye, vec3_t fwd, vec3_t right);
extern void q3ide_ov_place_arc(vec3_t eye, vec3_t fwd, int *idxs, int n, float pitch_rad);

/* App category lists — q3ide_attach_filter.c */
extern const char *q3ide_terminal_apps[];
extern const char *q3ide_browser_apps[];
extern qboolean q3ide_match(const char *app, const char **list);

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

/* ── Direct scroll apply — no re-arc, just shift ov_origin Z ────────── */

void q3ide_ov_scroll_apply(float delta)
{
	int i;
	for (i = 0; i < Q3IDE_MAX_WIN; i++) {
		if (q3ide_wm.wins[i].active && q3ide_wm.wins[i].in_overview)
			q3ide_wm.wins[i].ov_origin[2] += delta;
	}
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

	/* Collect only in_overview windows for arc layout */
	n = 0;
	for (i = 0; i < Q3IDE_MAX_WIN; i++)
		if (q3ide_wm.wins[i].active && q3ide_wm.wins[i].in_overview)
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

	/* g_ov_scroll_z shifts the whole grid up/down in world units. */
	z_off = -(float) cl.snap.ps.viewheight + g_ov_scroll_z;
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
			float h = q3ide_wm.wins[row_idxs[i]].world_h;
			if (h > wh)
				wh = h;
		}
		if (wh < 1.0f)
			wh = 45.0f;

		z_off += cell_h * 0.5f + (cell_h > 0.0f ? Q3IDE_OVERVIEW_GAP : 0.0f) + wh * 0.5f;
		cell_h = wh;

		VectorCopy(eye, row_eye);
		row_eye[2] += z_off;
		q3ide_ov_place_arc(row_eye, fwd, row_idxs, row_n, 0.0f);
	}
}
