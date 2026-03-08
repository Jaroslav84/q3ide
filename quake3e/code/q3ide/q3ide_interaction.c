/*
 * q3ide_interaction.c — Interaction model (Batch 2).
 *
 * Three modes:
 *   FPS Mode   — normal gameplay; crosshair aims at Windows; dwell → hover
 *   Pointer Mode — mouse cursor controls Window; click routes to app; Esc exits
 *   Keyboard    — all keys route to captured app; Esc exits
 */

#include "q3ide_interaction.h"
#include "q3ide_wm.h"
#include "q3ide_wm_internal.h"
#include "../qcommon/qcommon.h"
#include "../client/client.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>

/* ============================================================
 *  Global state
 * ============================================================ */

q3ide_interaction_state_t q3ide_interaction;

/* ============================================================
 *  Helpers
 * ============================================================ */

static void q3ide_do_click(unsigned int wid, float uv_x, float uv_y)
{
	if (q3ide_wm.cap_inject_click)
		q3ide_wm.cap_inject_click(q3ide_wm.cap, wid, uv_x, uv_y);
	else
		Com_Printf("q3ide: click wid=%u uv=(%.2f,%.2f) — inject unavailable\n", wid, uv_x, uv_y);
}

static void q3ide_cosmetic_fire(void)
{
	/* Fire weapon animation without consuming ammo */
	Cbuf_AddText("give ammo\n");
	/* The actual weapon will fire via the attack button being pressed,
	 * but Q3IDE_Interaction_ConsumesInput() should suppress damage.
	 * For now: just refill ammo so weapon never dry-fires. */
}

/* Compute UV coords, distance, and 3D hit position for a window hit by crosshair ray */
static int q3ide_crosshair_window(float *out_uv, float *out_dist, vec3_t out_hit_pos)
{
	vec3_t eye, fwd;
	float p, y;
	int hit;

	if (cls.state != CA_ACTIVE)
		return -1;

	VectorCopy(cl.snap.ps.origin, eye);
	eye[2] += cl.snap.ps.viewheight;
	p = cl.snap.ps.viewangles[PITCH] * (float) M_PI / 180.0f;
	y = cl.snap.ps.viewangles[YAW] * (float) M_PI / 180.0f;
	fwd[0] = cosf(p) * cosf(y);
	fwd[1] = cosf(p) * sinf(y);
	fwd[2] = -sinf(p);

	hit = Q3IDE_WM_TraceWindowHit(eye, fwd);
	if (hit < 0)
		return -1;

	/* Compute UV and distance for the hit window */
	{
		q3ide_win_t *win = &q3ide_wm.wins[hit];
		vec3_t right, up, diff, hit_point;
		float t, denom, hw, hh, lx, ly;

		denom = DotProduct(fwd, win->normal);
		if (fabsf(denom) < 0.001f)
			return -1;

		VectorSubtract(win->origin, eye, diff);
		t = DotProduct(diff, win->normal) / denom;
		if (t < 0)
			return -1;

		hit_point[0] = eye[0] + fwd[0] * t;
		hit_point[1] = eye[1] + fwd[1] * t;
		hit_point[2] = eye[2] + fwd[2] * t;
		VectorCopy(hit_point, out_hit_pos);

		/* Basis: identical to q3ide_add_poly — right perp to normal in XY, up = world Z */
		{
			float nx2 = win->normal[0], ny2 = win->normal[1];
			float hlen = sqrtf(nx2 * nx2 + ny2 * ny2);
			if (hlen > 0.01f) {
				right[0] = -ny2 / hlen;
				right[1] = nx2 / hlen;
				right[2] = 0.0f;
			} else {
				right[0] = 1.0f;
				right[1] = 0.0f;
				right[2] = 0.0f;
			}
			up[0] = 0.0f;
			up[1] = 0.0f;
			up[2] = 1.0f;
		}

		VectorSubtract(hit_point, win->origin, diff);
		lx = DotProduct(diff, right);
		ly = DotProduct(diff, up);
		hw = win->world_w * 0.5f;
		hh = win->world_h * 0.5f;

		/* UV: right→ has ST.x=0, up↑ has ST.y=0 — both decrease as local coord increases */
		out_uv[0] = 1.0f - (lx / hw + 1.0f) * 0.5f;
		out_uv[1] = 1.0f - (ly / hh + 1.0f) * 0.5f;
		*out_dist = t;

		Com_DPrintf("q3ide UV win=%d lx=%.2f ly=%.2f hw=%.2f hh=%.2f uv=(%.3f,%.3f) n=(%.2f,%.2f,%.2f)\n", hit, lx, ly,
		            hw, hh, out_uv[0], out_uv[1], win->normal[0], win->normal[1], win->normal[2]);

		return hit;
	}
}

/* ============================================================
 *  Public API
 * ============================================================ */

void Q3IDE_Interaction_Init(void)
{
	memset(&q3ide_interaction, 0, sizeof(q3ide_interaction));
	q3ide_interaction.focused_win = -1;
	q3ide_interaction.dwell_start_ms = -1.0f;
	q3ide_interaction.mode = Q3IDE_MODE_FPS;
	/* Register pain sounds for window hit feedback */
	q3ide_interaction.pain_sfx[0] = S_RegisterSound("sound/player/Sarge/pain25_1.wav", qfalse);
	q3ide_interaction.pain_sfx[1] = S_RegisterSound("sound/player/Sarge/pain50_1.wav", qfalse);
	q3ide_interaction.pain_sfx[2] = S_RegisterSound("sound/player/Sarge/pain75_1.wav", qfalse);
}

void Q3IDE_Interaction_Frame(qboolean attacking, qboolean use_key, qboolean escape, float mouse_dx, float mouse_dy)
{
	qboolean attack_start = attacking && !q3ide_interaction.prev_attacking;
	q3ide_interaction.prev_attacking = attacking;

	if (q3ide_interaction.mode == Q3IDE_MODE_FPS) {
		int prev_win = q3ide_interaction.focused_win;
		float uv[2], dist;

		/* Check crosshair for window hit */
		q3ide_interaction.focused_win = q3ide_crosshair_window(uv, &dist, q3ide_interaction.focused_hit_pos);
		if (q3ide_interaction.focused_win >= 0) {
			q3ide_interaction.focused_uv[0] = uv[0];
			q3ide_interaction.focused_uv[1] = uv[1];
			q3ide_interaction.focused_dist = dist;

			/* Shot landed on a window — blood splat + pain sound */
		/* DISABLED temporarily
		if (attack_start) {
			q3ide_win_t *win = &q3ide_wm.wins[q3ide_interaction.focused_win];
			sfxHandle_t sfx = q3ide_interaction.pain_sfx[rand() % 3];
			win->hit_time_ms = (unsigned long long) Sys_Milliseconds();
			VectorCopy(q3ide_interaction.focused_hit_pos, win->hit_pos);
			if (sfx)
				S_StartSound(win->hit_pos, 0, CHAN_VOICE, sfx);
		}
		*/
		(void) attack_start;

			/* Continue dwell if same window; otherwise restart */
			if (q3ide_interaction.focused_win != prev_win) {
				q3ide_interaction.dwell_start_ms = (float) Sys_Milliseconds();
				q3ide_interaction.hover_t = 0.0f;
				if (prev_win >= 0)
					Q3IDE_WM_SetHover(prev_win, 0.0f);
				Com_DPrintf("q3ide: crosshair → win=%d dist=%.0f\n", q3ide_interaction.focused_win, dist);
			}

			/* Accumulate dwell time and update hover */
			if (q3ide_interaction.dwell_start_ms >= 0.0f) {
				float dwell_elapsed = (float) Sys_Milliseconds() - q3ide_interaction.dwell_start_ms;
				q3ide_interaction.hover_t = dwell_elapsed / Q3IDE_DWELL_MS;
				if (q3ide_interaction.hover_t > 1.0f)
					q3ide_interaction.hover_t = 1.0f;
				Q3IDE_WM_SetHover(q3ide_interaction.focused_win, q3ide_interaction.hover_t);

				/* Auto-enter Pointer Mode: dwell complete + within 10m */
				if (q3ide_interaction.hover_t >= 1.0f && q3ide_interaction.focused_dist <= Q3IDE_POINTER_MAX_DIST) {
					q3ide_interaction.mode = Q3IDE_MODE_POINTER;
					q3ide_interaction.pointer_uv[0] = q3ide_interaction.focused_uv[0];
					q3ide_interaction.pointer_uv[1] = q3ide_interaction.focused_uv[1];
					q3ide_interaction.dwell_start_ms = -1.0f; /* disarm dwell so ESC re-entry needs fresh dwell */
					Com_Printf("q3ide: entered Pointer Mode win=%d\n", q3ide_interaction.focused_win);
					return; /* skip FPS shoot-frame processing this frame */
				}
			}
		} else {
			/* No window under crosshair */
			if (prev_win >= 0) {
				Q3IDE_WM_SetHover(prev_win, 0.0f);
			}
			q3ide_interaction.dwell_start_ms = -1.0f;
			q3ide_interaction.hover_t = 0.0f;
		}
	} else if (q3ide_interaction.mode == Q3IDE_MODE_POINTER) {
		int win_idx = q3ide_interaction.focused_win;
		if (win_idx < 0 || !q3ide_wm.wins[win_idx].active) {
			if (win_idx >= 0)
				Q3IDE_WM_SetHover(win_idx, 0.0f);
			q3ide_interaction.mode = Q3IDE_MODE_FPS;
			q3ide_interaction.hover_t = 0.0f;
			q3ide_interaction.dwell_start_ms = (float) Sys_Milliseconds();
			Com_Printf("q3ide: exited Pointer Mode (window lost)\n");
			return;
		}

		/* Update pointer position based on mouse delta */
		{
			const q3ide_win_t *win = &q3ide_wm.wins[win_idx];
			/* Angular sensitivity: map view-angle delta to UV delta.
			 * mouse_dx = yaw_change_degrees * 8 (from hooks.c).
			 * UV per degree = (pi/180) * dist / world_w (small angle approx).
			 * Use a safe minimum distance of 100 to avoid divide-by-zero. */
			{
				float dist = q3ide_interaction.focused_dist > 100.0f ? q3ide_interaction.focused_dist : 100.0f;
				float uv_per_deg = (float) (M_PI / 180.0) * dist / win->world_w / 8.0f;
				q3ide_interaction.pointer_uv[0] += mouse_dx * uv_per_deg;
				q3ide_interaction.pointer_uv[1] += mouse_dy * uv_per_deg * (win->world_w / win->world_h);
			}

			/* Clamp to edge zone — exiting the window edge returns to FPS */
			if (q3ide_interaction.pointer_uv[0] < Q3IDE_EDGE_ZONE_UV ||
			    q3ide_interaction.pointer_uv[0] > 1.0f - Q3IDE_EDGE_ZONE_UV ||
			    q3ide_interaction.pointer_uv[1] < Q3IDE_EDGE_ZONE_UV ||
			    q3ide_interaction.pointer_uv[1] > 1.0f - Q3IDE_EDGE_ZONE_UV) {
				Q3IDE_WM_SetHover(win_idx, 0.0f);
				q3ide_interaction.mode = Q3IDE_MODE_FPS;
				q3ide_interaction.hover_t = 0.0f;
				q3ide_interaction.dwell_start_ms = (float) Sys_Milliseconds();
				Com_Printf("q3ide: exited Pointer Mode (edge)\n");
				return;
			}

			/* Clamp to [0..1] */
			if (q3ide_interaction.pointer_uv[0] < 0.0f)
				q3ide_interaction.pointer_uv[0] = 0.0f;
			if (q3ide_interaction.pointer_uv[0] > 1.0f)
				q3ide_interaction.pointer_uv[0] = 1.0f;
			if (q3ide_interaction.pointer_uv[1] < 0.0f)
				q3ide_interaction.pointer_uv[1] = 0.0f;
			if (q3ide_interaction.pointer_uv[1] > 1.0f)
				q3ide_interaction.pointer_uv[1] = 1.0f;
		}

		/* Handle input in Pointer Mode */
		if (attacking) {
			q3ide_do_click((unsigned int) q3ide_wm.wins[win_idx].capture_id, q3ide_interaction.pointer_uv[0],
			               q3ide_interaction.pointer_uv[1]);
			q3ide_cosmetic_fire();
		}
		if (use_key) {
			q3ide_interaction.mode = Q3IDE_MODE_KEYBOARD;
			Com_Printf("q3ide: entered Keyboard Mode\n");
			return;
		}
		if (escape) {
			Q3IDE_WM_SetHover(win_idx, 0.0f);
			q3ide_interaction.mode = Q3IDE_MODE_FPS;
			q3ide_interaction.hover_t = 0.0f;
			q3ide_interaction.dwell_start_ms = (float) Sys_Milliseconds(); /* must re-dwell to re-enter */
			Com_Printf("q3ide: exited Pointer Mode (ESC)\n");
			return;
		}
	} else if (q3ide_interaction.mode == Q3IDE_MODE_KEYBOARD) {
		/* Key forwarding is handled per-event by Q3IDE_Interaction_OnKeyEvent. */
		if (attacking)
			q3ide_cosmetic_fire(); /* weapon fires normally; refill ammo */
		if (escape) {
			q3ide_interaction.mode = Q3IDE_MODE_FPS;
			Com_Printf("q3ide: exited Keyboard Mode (ESC)\n");
			return;
		}
	}
}

qboolean Q3IDE_Interaction_OnKeyEvent(int key, qboolean down)
{
	int win_idx;

	/* Pointer Mode: intercept ESC to exit the mode and block Q3's escape menu.
	 * All other keys pass through so Q3 bindings (e.g. "set q3ide_use_key 1") still fire.
	 * Movement suppression is handled by zeroing forwardmove/sidemove/upmove in
	 * CL_CreateNewCommands — no need to swallow individual movement keys here. */
	if (q3ide_interaction.mode == Q3IDE_MODE_POINTER) {
		if (down && key == 27) { /* K_ESCAPE */
			win_idx = q3ide_interaction.focused_win;
			if (win_idx >= 0)
				Q3IDE_WM_SetHover(win_idx, 0.0f);
			q3ide_interaction.mode = Q3IDE_MODE_FPS;
			q3ide_interaction.hover_t = 0.0f;
			q3ide_interaction.dwell_start_ms = (float) Sys_Milliseconds(); /* must re-dwell to re-enter */
			Com_Printf("q3ide: exited Pointer Mode (ESC)\n");
			return qtrue; /* consumed: Q3 menu will not open */
		}
		return qfalse;
	}

	if (q3ide_interaction.mode != Q3IDE_MODE_KEYBOARD)
		return qfalse;

	/* Keyboard Mode: ESC exits (blocking Q3 from processing it).
	 * All other keys are forwarded to the captured app and swallowed from Q3. */
	if (down && key == 27) { /* K_ESCAPE */
		q3ide_interaction.mode = Q3IDE_MODE_FPS;
		Com_Printf("q3ide: exited Keyboard Mode (ESC)\n");
		return qtrue;
	}

	win_idx = q3ide_interaction.focused_win;
	if (win_idx < 0 || !q3ide_wm.wins[win_idx].active)
		return qfalse;
	if (!q3ide_wm.cap_inject_key)
		return qfalse;
	q3ide_wm.cap_inject_key(q3ide_wm.cap, q3ide_wm.wins[win_idx].capture_id, key, down ? 1 : 0);
	return qtrue;
}

q3ide_interaction_mode_t Q3IDE_Interaction_GetMode(void)
{
	return q3ide_interaction.mode;
}

qboolean Q3IDE_Interaction_ConsumesInput(void)
{
	return q3ide_interaction.mode != Q3IDE_MODE_FPS;
}
