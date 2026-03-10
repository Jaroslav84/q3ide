/*
 * q3ide_win_mngr.c — Window manager: global state, attach, move, find.
 * Dylib load / poll thread / init / shutdown: q3ide_dylib.c.
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

	if (do_start && q3ide_wm.cap_start &&
	    q3ide_wm.cap_start(q3ide_wm.cap, id, Q3IDE_CAPTURE_FPS) != 0) {
		Com_Printf("q3ide: capture start failed id=%u\n", id);
		return qfalse;
	}
	win = &q3ide_wm.wins[i];
	memset(win, 0, sizeof(*win));
	win->active      = qtrue;
	win->capture_id  = id;
	win->scratch_slot = slot;
	VectorCopy(origin, win->origin);
	VectorCopy(normal, win->normal);
	win->normal[2] = 0.0f;
	len = sqrtf(win->normal[0] * win->normal[0] + win->normal[1] * win->normal[1]);
	if (len > 0.001f) {
		win->normal[0] /= len;
		win->normal[1] /= len;
	}
	win->world_w     = ww;
	win->world_h     = wh;
	win->wall_mounted = skip_clamp;
	win->is_tunnel   = qtrue; /* OS screen-capture window — removed by detach-all */
	win->uv_x0         = 0.0f;
	win->uv_x1         = 1.0f;
	win->window_ids[0] = id;
	win->window_count  = 1;
	win->window_cur    = 0;
	win->owns_stream   = do_start;
	win->stream_active = do_start; /* stream starts live; cleared on failure */
	q3ide_wm.num_active++;
	if (!skip_clamp)
		q3ide_clamp_window_size(win);
	return qtrue;
}

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
}

int Q3IDE_WM_FindById(unsigned int cid)
{
	int i;
	for (i = 0; i < Q3IDE_MAX_WIN; i++)
		if (q3ide_wm.wins[i].active && q3ide_wm.wins[i].capture_id == cid)
			return i;
	return -1;
}

void Q3IDE_WM_CycleWindow(int idx)
{
	q3ide_win_t *w;
	unsigned int old_id, new_id;

	if (idx < 0 || idx >= Q3IDE_MAX_WIN || !q3ide_wm.wins[idx].active)
		return;
	w = &q3ide_wm.wins[idx];
	if (w->window_count <= 1)
		return;

	old_id         = w->capture_id;
	w->window_cur  = (w->window_cur + 1) % w->window_count;
	new_id         = w->window_ids[w->window_cur];

	if (w->owns_stream) {
		if (q3ide_wm.cap_stop)
			q3ide_wm.cap_stop(q3ide_wm.cap, old_id);
		if (q3ide_wm.cap_start)
			q3ide_wm.cap_start(q3ide_wm.cap, new_id, Q3IDE_CAPTURE_FPS);
	}
	w->capture_id      = new_id;
	w->stream_active   = qtrue;
	w->window_user_sel = qtrue;
	if (q3ide_wm.cap_raise_win)
		q3ide_wm.cap_raise_win(q3ide_wm.cap, new_id);
	Q3IDE_LOGI("cycle win=%d [%d/%d] id=%u", idx, w->window_cur + 1, w->window_count, new_id);
}

void Q3IDE_WM_PauseStreams(void)
{
	if (q3ide_wm.streams_paused)
		return;
	q3ide_wm.streams_paused = qtrue;
	if (q3ide_wm.cap_pause_streams && q3ide_wm.cap)
		q3ide_wm.cap_pause_streams(q3ide_wm.cap);
	Q3IDE_LOGI("streams paused (;)");
}

void Q3IDE_WM_ResumeStreams(void)
{
	if (!q3ide_wm.streams_paused)
		return;
	q3ide_wm.streams_paused = qfalse;
	if (q3ide_wm.cap_resume_streams && q3ide_wm.cap)
		q3ide_wm.cap_resume_streams(q3ide_wm.cap);
	Q3IDE_LOGI("streams resumed (;)");
}

qboolean Q3IDE_WM_RemoveFromGroup(unsigned int wid)
{
	int pi, k, pos;
	q3ide_win_t *pw;

	/* Find which panel contains this window ID */
	for (pi = 0; pi < Q3IDE_MAX_WIN; pi++) {
		pw = &q3ide_wm.wins[pi];
		if (!pw->active)
			continue;
		pos = -1;
		for (k = 0; k < pw->window_count; k++) {
			if (pw->window_ids[k] == wid) {
				pos = k;
				break;
			}
		}
		if (pos < 0)
			continue;

		/* Found it */
		if (pw->window_count == 1) {
			/* Last window of this app — detach the whole panel */
			Q3IDE_WM_DetachById(pw->capture_id);
			return qtrue;
		}

		if (pw->capture_id == wid) {
			/* Currently displayed window closed — advance to next */
			unsigned int new_id;
			memmove(&pw->window_ids[pos], &pw->window_ids[pos + 1],
			        (pw->window_count - pos - 1) * sizeof(unsigned int));
			pw->window_count--;
			if (pw->window_cur >= pw->window_count)
				pw->window_cur = 0;
			new_id = pw->window_ids[pw->window_cur];
			if (pw->owns_stream && q3ide_wm.cap_start)
				q3ide_wm.cap_start(q3ide_wm.cap, new_id, Q3IDE_CAPTURE_FPS);
			pw->capture_id   = new_id;
			pw->stream_active = qtrue;
			if (q3ide_wm.cap_raise_win)
				q3ide_wm.cap_raise_win(q3ide_wm.cap, new_id);
			Q3IDE_LOGI("group: wid=%u closed, panel=%d → wid=%u [%d/%d]",
			           wid, pi, new_id, pw->window_cur + 1, pw->window_count);
		} else {
			/* Background window closed — just remove */
			memmove(&pw->window_ids[pos], &pw->window_ids[pos + 1],
			        (pw->window_count - pos - 1) * sizeof(unsigned int));
			pw->window_count--;
			if (pw->window_cur >= pw->window_count)
				pw->window_cur = pw->window_count - 1;
			Q3IDE_LOGI("group: bg wid=%u removed from panel=%d (%d remaining)", wid, pi, pw->window_count);
		}
		return qfalse;
	}
	return qfalse; /* not in any group */
}
