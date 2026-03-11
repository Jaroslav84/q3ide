/*
 * q3ide_view_modes_monitors.c — Monitor arc (I key) view mode.
 * Arc placement math: q3ide_view_modes_arc.c.
 */

#include "q3ide_view_modes.h"
#include "../client/client.h"
#include "../qcommon/qcommon.h"
#include "q3ide_engine_hooks.h"
#include "q3ide_hotkey.h"
#include "q3ide_log.h"
#include "q3ide_params.h"
#include "q3ide_win_mngr.h"
#include "q3ide_win_mngr_internal.h"

/* Shared state — defined in q3ide_view_modes.c */
extern qboolean g_f3_active;
extern qboolean g_ov_placed;

static q3ide_hotkey_t s_f3_hk = Q3IDE_HOTKEY_INIT;

/* Arc placement — q3ide_view_modes_arc.c */
extern void q3ide_focus3_place_arc(vec3_t eye, vec3_t fwd, int *idxs, int n, float pitch_rad);

/* Retry state: cap_start_disp fails if the stream is still stopping (async Swift
 * teardown).  Set g_f3_retry_at to a future ms timestamp; Tick() retries then. */
unsigned long long g_f3_retry_at    = 0;
static int         g_f3_retry_count = 0;

/* Forward declarations from q3ide_view_modes.c */
extern qboolean q3ide_player_axes(vec3_t eye, vec3_t fwd, vec3_t right);
extern void     q3ide_overview_detach_all(void);

void q3ide_focus3_show(void)
{
	int    i, n_disp, disp_idxs[3];
	vec3_t eye, fwd, right, pos, norm;

	if (!q3ide_wm.cap) {
		Q3IDE_LOGE("focus3: capture dylib not loaded");
		Q3IDE_SetHudMsg("FOCUS 3: no capture dylib", 2000);
		return;
	}

	if (!q3ide_player_axes(eye, fwd, right))
		return;

	/* Collect already-attached display captures */
	n_disp = 0;
	for (i = 0; i < Q3IDE_MAX_WIN && n_disp < 3; i++) {
		q3ide_win_t *w = &q3ide_wm.wins[i];
		if (w->active && w->is_tunnel && !w->owns_stream)
			disp_idxs[n_disp++] = i;
	}

	if (n_disp == 0) {
		/* Attach displays from OS list (displays_only=qtrue) */
		Q3IDE_WM_PopulateQueue(qtrue);
		pos[0]  = eye[0] + fwd[0] * Q3IDE_FOCUS3_DIST;
		pos[1]  = eye[1] + fwd[1] * Q3IDE_FOCUS3_DIST;
		pos[2]  = eye[2];
		norm[0] = -fwd[0];
		norm[1] = -fwd[1];
		norm[2] = 0.0f;
		for (i = 0; i < 3 && Q3IDE_WM_PendingCount() > 0; i++)
			Q3IDE_WM_AttachNextPending(pos, norm);

		/* Re-collect */
		n_disp = 0;
		for (i = 0; i < Q3IDE_MAX_WIN && n_disp < 3; i++) {
			q3ide_win_t *w = &q3ide_wm.wins[i];
			if (w->active && w->is_tunnel && !w->owns_stream)
				disp_idxs[n_disp++] = i;
		}
	}

	if (n_disp == 0) {
		/* cap_start_disp failed — stream is likely still stopping (async Swift
		 * teardown).  Schedule a retry; Tick() will call q3ide_focus3_show() again. */
		if (g_f3_retry_count < 10) {
			g_f3_retry_count++;
			g_f3_retry_at = Sys_Milliseconds() + 300;
			Q3IDE_SetHudMsg("FOCUS 3: starting...", 1000);
			Q3IDE_LOGI("focus3: cap failed, retry %d in 300ms", g_f3_retry_count);
		} else {
			g_f3_retry_count = 0;
			Q3IDE_SetHudMsg("FOCUS 3: no displays", 2000);
			Q3IDE_LOGE("focus3: gave up after 10 retries");
		}
		return;
	}

	g_f3_retry_count = 0;

	/* Scale sizes once to fit available depth (place_arc only positions). */
	{
		vec3_t end, mins, maxs;
		trace_t tr;
		float d, scale;
		end[0] = eye[0] + fwd[0] * Q3IDE_FOCUS3_DIST;
		end[1] = eye[1] + fwd[1] * Q3IDE_FOCUS3_DIST;
		end[2] = eye[2];
		VectorSet(mins, -4.0f, -4.0f, -4.0f);
		VectorSet(maxs, 4.0f, 4.0f, 4.0f);
		CM_BoxTrace(&tr, eye, end, mins, maxs, 0, CONTENTS_SOLID, qfalse);
		d = tr.fraction * Q3IDE_FOCUS3_DIST - Q3IDE_WALL_OFFSET;
		if (d < Q3IDE_FOCUS3_MIN_DIST)
			d = Q3IDE_FOCUS3_MIN_DIST;
		scale = d / Q3IDE_FOCUS3_DIST;
		if (scale > 1.0f)
			scale = 1.0f;
		if (scale < 0.999f) {
			int si;
			for (si = 0; si < n_disp; si++) {
				q3ide_wm.wins[disp_idxs[si]].world_w *= scale;
				q3ide_wm.wins[disp_idxs[si]].world_h *= scale;
			}
		}
	}

	q3ide_focus3_place_arc(eye, fwd, disp_idxs, n_disp, 0.0f);
	g_f3_active = qtrue;
	Q3IDE_SetHudMsg("FOCUS 3", 1500);
	Q3IDE_LOGI("focus3: %d displays in arc", n_disp);
}

void q3ide_focus3_hide(void)
{
	int i;
	g_f3_retry_at    = 0;
	g_f3_retry_count = 0;
	/* cap_stop is required: display captures (owns_stream=qfalse) are NOT stopped
	 * by DetachById, so cap_start_disp on re-show would fail with "already running".
	 * Swift teardown is async — q3ide_focus3_show() handles the delay via retry logic. */
	for (i = 0; i < Q3IDE_MAX_WIN; i++) {
		q3ide_win_t *w = &q3ide_wm.wins[i];
		if (w->active && w->is_tunnel && !w->owns_stream) {
			if (q3ide_wm.cap_stop)
				q3ide_wm.cap_stop(q3ide_wm.cap, w->capture_id);
			Q3IDE_WM_DetachById(w->capture_id);
		}
	}
	g_f3_active = qfalse;
	Q3IDE_SetHudMsg("FOCUS 3 off", 1000);
}

/* ── Command callbacks ───────────────────────────────────────────────── */

void q3ide_cmd_focus3_down(void)
{
	if (Q3IDE_ViewModes_OverviewActive() || g_ov_placed)
		q3ide_overview_detach_all();

	if (cls.state != CA_ACTIVE) {
		Q3IDE_SetHudMsg("FOCUS 3: not in game", 1500);
		return;
	}

	if (q3ide_hk_down(&s_f3_hk, g_f3_active) == Q3IDE_HK_ACTIVATE) {
		q3ide_focus3_show();
		q3ide_hk_rearm(&s_f3_hk); /* focus3_show() does stream init — restart timer */
	} else {
		q3ide_focus3_hide();
	}
}

void q3ide_cmd_focus3_up(void)
{
	if (q3ide_hk_up(&s_f3_hk, g_f3_active) == Q3IDE_HK_DEACTIVATE)
		q3ide_focus3_hide();
}

/* ── Per-frame tick: track player view while Focus3 is active ───────── */

void q3ide_focus3_tick(void)
{
	int    i, n_disp, disp_idxs[3];
	vec3_t eye, fwd, right;

	if (!g_f3_active)
		return;
	if (!q3ide_player_axes(eye, fwd, right))
		return;

	n_disp = 0;
	for (i = 0; i < Q3IDE_MAX_WIN && n_disp < 3; i++) {
		q3ide_win_t *w = &q3ide_wm.wins[i];
		if (w->active && w->is_tunnel && !w->owns_stream)
			disp_idxs[n_disp++] = i;
	}
	if (n_disp == 0)
		return;

	q3ide_focus3_place_arc(eye, fwd, disp_idxs, n_disp, 0.0f);
}
