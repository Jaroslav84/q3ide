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
static qboolean g_f3_held = qfalse; /* true while key is physically held */

/* Arc placement — q3ide_view_modes_arc.c */
extern void q3ide_focus3_place_arc(vec3_t eye, vec3_t fwd, int *idxs, int n, float pitch_rad);

/* Retry state: cap_start_disp fails if the stream is still stopping (async Swift
 * teardown).  Set g_f3_retry_at to a future ms timestamp; Tick() retries then. */
unsigned long long g_f3_retry_at = 0;
static int g_f3_retry_count = 0;

/* Forward declarations from q3ide_view_modes.c */
extern qboolean q3ide_player_axes(vec3_t eye, vec3_t fwd, vec3_t right);
extern void q3ide_overview_detach_all(void);

void q3ide_focus3_show(void)
{
	int i, n_disp, disp_idxs[3];
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
		pos[0] = eye[0] + fwd[0] * Q3IDE_VIEWMODE_ARC_DIST;
		pos[1] = eye[1] + fwd[1] * Q3IDE_VIEWMODE_ARC_DIST;
		pos[2] = eye[2];
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
		if (g_f3_retry_count < Q3IDE_SCK_FOCUS3_RETRY_COUNT) {
			g_f3_retry_count++;
			g_f3_retry_at = Sys_Milliseconds() + Q3IDE_SCK_FOCUS3_RETRY_MS;
			Q3IDE_SetHudMsg("FOCUS 3: starting...", Q3IDE_HUD_CONFIRM_MS);
			Q3IDE_LOGI("focus3: cap failed, retry %d in %dms", g_f3_retry_count, Q3IDE_SCK_FOCUS3_RETRY_MS);
		} else {
			g_f3_retry_count = 0;
			Q3IDE_SetHudMsg("FOCUS 3: no displays", Q3IDE_HUD_ERROR_MS);
			Q3IDE_LOGE("focus3: gave up after %d retries", Q3IDE_SCK_FOCUS3_RETRY_COUNT);
		}
		return;
	}

	g_f3_retry_count = 0;

	/* Reset display window sizes to base (unzoomed) before arc placement.
	 * Ensures I mode always shows default-sized panels regardless of wall zoom. */
	for (i = 0; i < n_disp; i++) {
		q3ide_win_t *w = &q3ide_wm.wins[disp_idxs[i]];
		w->world_w = w->base_world_w;
		w->world_h = w->base_world_h;
	}

	q3ide_focus3_place_arc(eye, fwd, disp_idxs, n_disp, 0.0f);
	g_f3_active = qtrue;
	Q3IDE_SetHudMsg("FOCUS 3", Q3IDE_HUD_STATUS_MS);
	Q3IDE_LOGI("focus3: %d displays in arc", n_disp);
}

void q3ide_focus3_hide(void)
{
	int i;
	g_f3_retry_at = 0;
	g_f3_retry_count = 0;
	/* cap_stop is required: display captures (owns_stream=qfalse) are NOT stopped
	 * by DetachById, so cap_start_disp on re-show would fail with "already running". */
	for (i = 0; i < Q3IDE_MAX_WIN; i++) {
		q3ide_win_t *w = &q3ide_wm.wins[i];
		if (w->active && w->is_tunnel && !w->owns_stream) {
			if (q3ide_wm.cap_stop)
				q3ide_wm.cap_stop(q3ide_wm.cap, w->capture_id);
			Q3IDE_WM_DetachById(w->capture_id);
		}
	}
	g_f3_active = qfalse;
	Q3IDE_SetHudMsg("FOCUS 3 off", Q3IDE_HUD_CONFIRM_MS);
}

/* Reset focus3 flags only — no stream/window changes.
 * Called by K (global kill): CmdSoftDetachAll handles all windows separately. */
void q3ide_focus3_reset_state(void)
{
	g_f3_retry_at = 0;
	g_f3_retry_count = 0;
	g_f3_active = qfalse;
	g_f3_held = qfalse;
	s_f3_hk.locked = qfalse;
}

/* ── Command callbacks ───────────────────────────────────────────────── */

void q3ide_cmd_focus3_down(void)
{
	if (g_f3_held)
		return; /* key repeat — ignore */

	if (Q3IDE_ViewModes_OverviewActive() || g_ov_placed)
		q3ide_overview_detach_all();

	if (cls.state != CA_ACTIVE) {
		Q3IDE_SetHudMsg("FOCUS 3: not in game", Q3IDE_HUD_STATUS_MS);
		return;
	}

	g_f3_held = qtrue;
	if (q3ide_hk_down(&s_f3_hk, g_f3_active) == Q3IDE_HK_ACTIVATE) {
		q3ide_focus3_show();
		q3ide_hk_rearm(&s_f3_hk); /* focus3_show() does stream init — restart timer */
	} else {
		q3ide_focus3_hide();
	}
}

void q3ide_cmd_focus3_up(void)
{
	g_f3_held = qfalse;
	if (q3ide_hk_up(&s_f3_hk, g_f3_active) == Q3IDE_HK_DEACTIVATE)
		q3ide_focus3_hide();
}

/* ── Per-frame tick: track player view while Focus3 is active ───────── */

void q3ide_focus3_tick(void)
{
	int i, si, n_disp, disp_idxs[3];
	vec3_t eye, fwd, right;
	vec3_t end, mins, maxs;
	trace_t tr;
	float d, scale;

	if (!g_f3_active || !g_f3_held)
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

	/* Re-apply uniform scale every frame so all panels shrink/grow together
	 * as the player moves toward or away from walls. */
	end[0] = eye[0] + fwd[0] * Q3IDE_VIEWMODE_ARC_DIST;
	end[1] = eye[1] + fwd[1] * Q3IDE_VIEWMODE_ARC_DIST;
	end[2] = eye[2];
	VectorSet(mins, -Q3IDE_TRACE_BOX_HALF, -Q3IDE_TRACE_BOX_HALF, -Q3IDE_TRACE_BOX_HALF);
	VectorSet(maxs, Q3IDE_TRACE_BOX_HALF, Q3IDE_TRACE_BOX_HALF, Q3IDE_TRACE_BOX_HALF);
	CM_BoxTrace(&tr, eye, end, mins, maxs, 0, CONTENTS_SOLID, qfalse);
	d = tr.fraction * Q3IDE_VIEWMODE_ARC_DIST - Q3IDE_WALL_OFFSET;
	if (d < Q3IDE_FOCUS3_MIN_DIST)
		d = Q3IDE_FOCUS3_MIN_DIST;
	scale = d / Q3IDE_VIEWMODE_ARC_DIST;
	if (scale > 1.0f)
		scale = 1.0f;
	for (si = 0; si < n_disp; si++) {
		q3ide_win_t *w = &q3ide_wm.wins[disp_idxs[si]];
		float asp = (w->world_h > 0.001f) ? w->world_w / w->world_h : Q3IDE_DISPLAY_ASPECT;
		w->world_w = Q3IDE_SPAWN_WIN_W * scale;
		w->world_h = w->world_w / asp;
	}

	q3ide_focus3_place_arc(eye, fwd, disp_idxs, n_disp, 0.0f);
}
