/*
 * q3ide_hooks_grapple.c — Grapple physics overrides (engine-side).
 *
 * Split from q3ide_hooks.c to keep file sizes under the 400-line limit.
 * Provides q3ide_grapple_type_frame() and q3ide_grapple_window_frame()
 * called each frame from Q3IDE_Frame (q3ide_hooks_frame.c).
 */

#include "q3ide_hooks.h"
#include "q3ide_log.h"
#include "q3ide_wm.h"
#include "q3ide_wm_internal.h"
#include "../qcommon/qcommon.h"
#include "../client/client.h"
#include <math.h>

extern q3ide_hooks_state_t q3ide_state;

/* ============================================================
 *  Grapple type physics (engine-side, bypasses QVM)
 * ============================================================ */

/*
 * g_grappleType: 0=hook (normal Q3), 1=rope (pendulum), 2=rail (instant teleport)
 *
 * All three types use the same QVM grapple (PMF_GRAPPLE_PULL), but the engine
 * layer overrides behaviour:
 *
 *   Hook (0) — vanilla Q3, no intervention.
 *   Rope (1) — pendulum: release hook when player reaches 50% of rest length,
 *              gravity swings player back, auto-refire after 25 frames.
 *   Rail (2) — on first attach: teleport player to hook point via setviewpos,
 *              release hook.  Feels like instant travel.
 */
static int q3ide_grapple_was_pulling = 0;
static float q3ide_rope_rest = 0.0f;
static int q3ide_rope_swing_frames = 0; /* frames since manual release */
static int q3ide_rope_attached = 0;     /* 1=hook currently engaged */

void q3ide_grapple_type_frame(void)
{
	int cur_pull, gtype;
	vec3_t gp, diff;
	float vlen;
	char cmd[256];

	if (cls.state != CA_ACTIVE)
		return;

	cur_pull = (cl.snap.ps.pm_flags & PMF_GRAPPLE_PULL) ? 1 : 0;
	gtype = Cvar_VariableIntegerValue("g_grappleType");

	/* ── Fresh attach ──────────────────────────────────────────────── */
	if (cur_pull && !q3ide_grapple_was_pulling) {
		VectorSubtract(cl.snap.ps.grapplePoint, cl.snap.ps.origin, diff);
		q3ide_rope_rest = VectorLength(diff);
		q3ide_rope_attached = 1;
		q3ide_rope_swing_frames = 0;

		if (gtype == 2) {
			/* Rail — snap player to hook point */
			VectorCopy(cl.snap.ps.grapplePoint, gp);
			Com_sprintf(cmd, sizeof(cmd), "cmd setviewpos %.0f %.0f %.0f %.0f\n", gp[0], gp[1], gp[2],
			            cl.snap.ps.viewangles[1]);
			Cbuf_AddText(cmd);
			Cbuf_AddText("-button5\n"); /* release hook after teleport */
			Com_Printf("q3ide: RAIL — snap to (%.0f %.0f %.0f)\n", gp[0], gp[1], gp[2]);
		} else if (gtype == 1) {
			Com_Printf("q3ide: ROPE — rest=%.0f\n", q3ide_rope_rest);
		} else {
			Com_Printf("q3ide: HOOK\n");
		}
	}

	/* ── Rope pendulum logic ────────────────────────────────────────── */
	if (gtype == 1) {
		if (cur_pull && q3ide_rope_attached && q3ide_rope_rest > 0.0f) {
			VectorSubtract(cl.snap.ps.grapplePoint, cl.snap.ps.origin, diff);
			vlen = VectorLength(diff);
			/* Release when player is within 50% of rest — simulate slack */
			if (vlen < q3ide_rope_rest * 0.5f) {
				Cbuf_AddText("-button5\n");
				q3ide_rope_attached = 0;
				q3ide_rope_swing_frames = 30; /* gravity swing for 30 frames */
			}
		}
		/* Auto-refire after swing */
		if (!q3ide_rope_attached && q3ide_rope_swing_frames > 0) {
			if (--q3ide_rope_swing_frames == 0)
				Cbuf_AddText("+button5\n"); /* reattach */
		}
	}

	/* ── Reset on full release ──────────────────────────────────────── */
	if (!cur_pull && q3ide_grapple_was_pulling) {
		q3ide_rope_rest = 0.0f;
		q3ide_rope_attached = 0;
		q3ide_rope_swing_frames = 0;
	}

	q3ide_grapple_was_pulling = cur_pull;
}

/* ============================================================
 *  Grapple-to-window teleport
 * ============================================================ */

/*
 * q3ide_grapple_window_frame — teleport player in front of a window when
 * the grapple hook attaches to its surface.
 */
void q3ide_grapple_window_frame(void)
{
	int i;
	q3ide_win_t *win;
	vec3_t gp, diff, right;
	float dist_to_plane, lx, ly, hw, hh, nx, ny, len, view_dist;
	float best_dist;
	int best_win;
	char cmd[160];

	if (cls.state != CA_ACTIVE)
		return;

	if (q3ide_state.grapple_tele_cooldown > 0) {
		q3ide_state.grapple_tele_cooldown--;
		return;
	}

	if (!(cl.snap.ps.pm_flags & PMF_GRAPPLE_PULL))
		return;

	VectorCopy(cl.snap.ps.grapplePoint, gp);

	best_dist = 48.0f;
	best_win = -1;

	for (i = 0; i < Q3IDE_MAX_WIN; i++) {
		win = &q3ide_wm.wins[i];
		if (!win->active || win->status == Q3IDE_WIN_STATUS_INACTIVE)
			continue;

		VectorSubtract(gp, win->origin, diff);
		dist_to_plane = fabsf(DotProduct(diff, win->normal));
		if (dist_to_plane >= best_dist)
			continue;

		nx = win->normal[0];
		ny = win->normal[1];
		len = sqrtf(nx * nx + ny * ny);
		if (len < 0.01f)
			continue;
		right[0] = -ny / len;
		right[1] = nx / len;
		right[2] = 0.0f;

		lx = DotProduct(diff, right);
		ly = diff[2];
		hw = win->world_w * 0.5f;
		hh = win->world_h * 0.5f;

		if (fabsf(lx) < hw && fabsf(ly) < hh) {
			best_dist = dist_to_plane;
			best_win = i;
		}
	}

	if (best_win < 0)
		return;

	win = &q3ide_wm.wins[best_win];

	view_dist = (win->world_w > win->world_h ? win->world_w : win->world_h) * 0.9f;
	if (view_dist < 120.0f)
		view_dist = 120.0f;
	if (view_dist > 350.0f)
		view_dist = 350.0f;

	{
		float tx = win->origin[0] + win->normal[0] * view_dist;
		float ty = win->origin[1] + win->normal[1] * view_dist;
		float tz = win->origin[2] - 26.0f;
		float yaw = atan2f(-win->normal[1], -win->normal[0]) * 180.0f / (float) M_PI;
		Com_sprintf(cmd, sizeof(cmd), "setviewpos %.0f %.0f %.0f %.0f", tx, ty, tz, yaw);
		Q3IDE_LOGI("grapple tele win[%d] view_dist=%.0f cmd=[%s]", best_win, view_dist, cmd);
		CL_AddReliableCommand(cmd, qfalse);
	}
	q3ide_state.grapple_tele_cooldown = 120;
}
