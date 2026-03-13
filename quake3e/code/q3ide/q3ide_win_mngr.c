/*
 * q3ide_win_mngr.c — Window manager: global state, attach, move, find.
 * Dylib load / poll thread / init / shutdown: q3ide_dylib.c.
 * Stream pause/resume + hide/show: q3ide_win_streams.c.
 */

#include "q3ide_win_mngr.h"
#include "q3ide_log.h"
#include "q3ide_win_mngr_internal.h"

#include "../qcommon/qcommon.h"
#include <math.h>
#include <string.h>

q3ide_wm_t q3ide_wm;

/* Geometry clamp — q3ide_geometry_clamp.c */
extern void q3ide_clamp_window_size(q3ide_win_t *win);
/* Window basis — q3ide_geometry.c */
extern void q3ide_win_basis(q3ide_win_t *win, vec3_t right, vec3_t up);

/*
 * Push win further from the wall by Q3IDE_WALL_WINDOWS_OFFSET for each existing
 * active window that shares the same wall and overlaps in 2D.
 * self_idx: the wins[] index of win itself (skipped in scan).
 */
static void q3ide_apply_wall_stack(q3ide_win_t *win, int self_idx)
{
	int i;
	float my_d, max_d, push;
	qboolean found = qfalse;
	vec3_t right, up;
	q3ide_win_basis(win, right, up);
	my_d = DotProduct(win->origin, win->normal);
	max_d = my_d;
	for (i = 0; i < Q3IDE_MAX_WIN; i++) {
		q3ide_win_t *w;
		vec3_t diff;
		float along_n, dx, dz, wd;
		if (i == self_idx)
			continue;
		w = &q3ide_wm.wins[i];
		if (!w->active)
			continue;
		/* Same wall direction */
		if (DotProduct(w->normal, win->normal) < 0.99f)
			continue;
		/* Near-coplanar: within tolerance along the shared normal */
		VectorSubtract(win->origin, w->origin, diff);
		along_n = DotProduct(diff, win->normal);
		if (fabsf(along_n) > Q3IDE_WALL_OVERLAP_PLANE_TOL)
			continue;
		/* 2D AABB overlap in right/up space */
		dx = DotProduct(diff, right);
		dz = diff[2]; /* up is always world Z */
		if (fabsf(dx) < (win->world_w + w->world_w) * 0.5f && fabsf(dz) < (win->world_h + w->world_h) * 0.5f) {
			wd = DotProduct(w->origin, win->normal);
			if (wd > max_d)
				max_d = wd;
			found = qtrue;
		}
	}
	if (!found)
		return;
	/* Place on top of the deepest overlapping window */
	push = max_d + Q3IDE_WALL_WINDOWS_OFFSET - my_d;
	if (push > 0.0f) {
		win->origin[0] += win->normal[0] * push;
		win->origin[1] += win->normal[1] * push;
		win->origin[2] += win->normal[2] * push;
	}
}

qboolean Q3IDE_WM_Attach(unsigned int id, vec3_t origin, vec3_t normal, float ww, float wh, qboolean do_start,
                         qboolean skip_clamp)
{
	int i, slot;
	q3ide_win_t *win;
	float len;

	for (i = 0; i < Q3IDE_MAX_WIN; i++)
		if (q3ide_wm.wins[i].active && q3ide_wm.wins[i].capture_id == id)
			return qfalse;

	for (i = 0; i < Q3IDE_MAX_WIN; i++)
		if (!q3ide_wm.wins[i].active)
			break;
	if (i >= Q3IDE_MAX_WIN) {
		Com_Printf("q3ide: max windows\n");
		return qfalse;
	}

	/* Find lowest scratch slot not currently in use */
	for (slot = 0; slot < Q3IDE_MAX_WIN; slot++) {
		int k;
		qboolean used = qfalse;
		for (k = 0; k < Q3IDE_MAX_WIN; k++)
			if (q3ide_wm.wins[k].active && q3ide_wm.wins[k].scratch_slot == slot) {
				used = qtrue;
				break;
			}
		if (!used)
			break;
	}
	if (slot >= Q3IDE_MAX_WIN) {
		Com_Printf("q3ide: no scratch slots\n");
		return qfalse;
	}

	if (do_start && q3ide_wm.cap_start && q3ide_wm.cap_start(q3ide_wm.cap, id, Q3IDE_CAPTURE_FPS) != 0) {
		Com_Printf("q3ide: capture start failed id=%u\n", id);
		return qfalse;
	}
	win = &q3ide_wm.wins[i];
	memset(win, 0, sizeof(*win));
	win->active = qtrue;
	win->capture_id = id;
	win->scratch_slot = slot;
	VectorCopy(origin, win->origin);
	VectorCopy(normal, win->normal);
	win->normal[2] = 0.0f;
	len = sqrtf(win->normal[0] * win->normal[0] + win->normal[1] * win->normal[1]);
	if (len > 0.001f) {
		win->normal[0] /= len;
		win->normal[1] /= len;
	}
	win->world_w = ww;
	win->world_h = wh;
	win->is_tunnel = qtrue;   /* OS screen-capture window — removed by detach-all */
	win->wall_placed = qtrue; /* Attach = wall placement by default; overview clears this for arc-only windows */
	win->uv_x0 = 0.0f;
	win->uv_x1 = 1.0f;
	win->owns_stream = do_start;
	win->stream_active = do_start; /* stream starts live; cleared on failure */
	q3ide_wm.num_active++;
	if (!skip_clamp)
		q3ide_clamp_window_size(win);
	q3ide_apply_wall_stack(win, i);
	return qtrue;
}

/* g_ov_active: true while overview is showing (q3ide_view_modes_overview.c) */
extern qboolean g_ov_active;
/* Restack arc after a window leaves it (q3ide_view_modes_grid.c) */
extern void q3ide_overview_layout(void);

void Q3IDE_WM_MoveWindow(int idx, vec3_t origin, vec3_t normal, qboolean skip_clamp)
{
	float len;
	if (idx < 0 || idx >= Q3IDE_MAX_WIN || !q3ide_wm.wins[idx].active)
		return;
	VectorCopy(origin, q3ide_wm.wins[idx].origin);
	VectorCopy(normal, q3ide_wm.wins[idx].normal);
	q3ide_wm.wins[idx].normal[2] = 0.0f;
	len = sqrtf(q3ide_wm.wins[idx].normal[0] * q3ide_wm.wins[idx].normal[0] +
	            q3ide_wm.wins[idx].normal[1] * q3ide_wm.wins[idx].normal[1]);
	if (len > 0.001f) {
		q3ide_wm.wins[idx].normal[0] /= len;
		q3ide_wm.wins[idx].normal[1] /= len;
	}
	if (!skip_clamp)
		q3ide_clamp_window_size(&q3ide_wm.wins[idx]);
	/* User shot this window onto a wall during overview: mark it wall-placed,
	 * pull it from the arc, and restack the remaining arc windows. */
	if (!skip_clamp && g_ov_active) {
		q3ide_wm.wins[idx].wall_placed = qtrue;
		q3ide_wm.wins[idx].in_overview = qfalse;
		q3ide_overview_layout();
	}
	q3ide_apply_wall_stack(&q3ide_wm.wins[idx], idx);
}

int Q3IDE_WM_FindById(unsigned int cid)
{
	int i;
	for (i = 0; i < Q3IDE_MAX_WIN; i++)
		if (q3ide_wm.wins[i].active && q3ide_wm.wins[i].capture_id == cid)
			return i;
	return -1;
}
