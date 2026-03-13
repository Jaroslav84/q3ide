/* q3ide_shoot_to_place.c — Shoot-to-place: window selection, repositioning, wall placement. */

#include "q3ide_engine_hooks.h"
#include "q3ide_log.h"
#include "q3ide_win_mngr.h"
#include "q3ide_win_mngr_internal.h"
#include "q3ide_params.h"
#include "../qcommon/qcommon.h"
#include "../client/client.h"
#include <math.h>

/* Missile tracking — q3ide_missile_tracking.c */
extern void q3ide_missile_track_frame(void);
extern void q3ide_missile_tracking_reset(void);
extern int q3ide_burst_place_end; /* defined in q3ide_missile_tracking.c */

/* Shoot-to-place state — shared with q3ide_engine.c */
extern int q3ide_selected_win; /* wins[] index, -1 = none selected */
extern int q3ide_select_time;  /* Sys_Milliseconds() when selected */
extern int q3ide_last_attack;  /* previous frame BUTTON_ATTACK state */
extern int q3ide_aimed_win;    /* window under crosshair this frame, -1 = none */

/* Placement cooldown: suppress window selection for Q3IDE_PLACE_COOLDOWN_MS after
 * placing a window, so rapid wall shots keep placing new windows instead of
 * accidentally entering reposition mode on the freshly-placed one. */
int q3ide_place_cooldown_end = 0;

/* Projectile weapons — placement handled via missile entity tracking, not attack edge. */
static qboolean q3ide_is_projectile(int weapon)
{
	return weapon == 4 || weapon == 5 || weapon == 8 || weapon == 9;
}

/* ── Hitscan fire detection ───────────────────────────────────────────── */

/* weaponTime counts down to 0 between shots. When the weapon fires, the game
 * resets it to the fire interval — so weaponTime *increasing* = gun just fired.
 * Tracking the weapon number prevents false triggers on weapon switch. */
static int g_prev_weapon_time = -1;
static int g_prev_weapon = -1;

static qboolean q3ide_weapon_fired_this_frame(void)
{
	int cur_time = cl.snap.ps.weaponTime;
	int cur_weapon = cl.snap.ps.weapon;
	qboolean fired = qfalse;
	if (g_prev_weapon == cur_weapon && g_prev_weapon_time >= 0)
		fired = (cur_time > g_prev_weapon_time);
	g_prev_weapon_time = cur_time;
	g_prev_weapon = cur_weapon;
	return fired;
}

/* ── Hitscan shoot frame ──────────────────────────────────────────────── */

void q3ide_shoot_frame(void)
{
	vec3_t eye, fwd;
	float p, y;
	int buttons, hit;

	if (cls.state != CA_ACTIVE) {
		q3ide_aimed_win = -1;
		q3ide_missile_tracking_reset();
		g_prev_weapon_time = -1;
		g_prev_weapon = -1;
		return;
	}

	/* Projectile tracking — handles all ET_MISSILE based weapons. */
	q3ide_missile_track_frame();

	/* Compute eye + forward every frame — needed for aim highlight and shoot. */
	VectorCopy(cl.snap.ps.origin, eye);
	eye[2] += cl.snap.ps.viewheight;
	p = cl.snap.ps.viewangles[PITCH] * (float) M_PI / 180.0f;
	y = cl.snap.ps.viewangles[YAW] * (float) M_PI / 180.0f;
	fwd[0] = cosf(p) * cosf(y);
	fwd[1] = cosf(p) * sinf(y);
	fwd[2] = -sinf(p);

	/* Aim highlight already computed in Q3IDE_Frame before this call — reuse it.
	 * Suppress window hits during placement cooldown so rapid wall shots keep
	 * spawning new windows rather than selecting a freshly-placed one. */
	hit = (Sys_Milliseconds() < q3ide_place_cooldown_end) ? -1 : q3ide_aimed_win;

	buttons = cl.cmds[cl.cmdNumber & CMD_MASK].buttons;
	q3ide_last_attack = buttons & BUTTON_ATTACK;

	/* Expire stale selection regardless of fire state. */
	if (q3ide_selected_win >= 0 && Sys_Milliseconds() - q3ide_select_time >= Q3IDE_REPOSITION_MS) {
		Q3IDE_LOGI("selection expired");
		q3ide_selected_win = -1;
	}

	/* Gate all hitscan placement on the weapon actually discharging this frame.
	 * Prevents placement during reload (railgun, shotgun, etc.). */
	if (!q3ide_weapon_fired_this_frame())
		return;

	if (hit >= 0 && hit == q3ide_selected_win) {
		/* Re-hit the already-selected window: cycle to the next one behind it */
		int next = Q3IDE_WM_TraceWindowHit(eye, fwd, hit);
		if (next >= 0) {
			q3ide_selected_win = next;
			q3ide_select_time = Sys_Milliseconds();
			Q3IDE_LOGI("cycled to [%d] -> shoot surface to move (3s)", next);
			Cbuf_AddText("give ammo\n");
			return;
		}
		/* No other window behind it — treat as miss so move path triggers */
		hit = -1;
	} else if (hit >= 0) {
		/* Shot hit a new window → select it */
		q3ide_selected_win = hit;
		q3ide_select_time = Sys_Milliseconds();
		Q3IDE_LOGI("selected [%d] -> shoot surface to move (3s)", hit);
		Cbuf_AddText("give ammo\n");
		return;
	}

	/* Projectile weapons: placement handled by missile impact tracking, not here.
	 * Pre-populate the queue now (at fire time) so it's ready when the missile
	 * hits — avoids the expensive PopulateQueue call mid-flight. */
	if (q3ide_is_projectile(cl.snap.ps.weapon)) {
		if (Q3IDE_WM_PendingCount() == 0)
			Q3IDE_WM_PopulateQueue(qfalse);
		return;
	}

	if (hit < 0 && q3ide_selected_win >= 0 && Sys_Milliseconds() - q3ide_select_time < Q3IDE_REPOSITION_MS) {
		/* Selection active, shot missed windows → move to hit surface (hitscan) */
		vec3_t wall_pos, wall_normal;
		if (Q3IDE_WM_TraceWall(eye, fwd, wall_pos, wall_normal)) {
			Q3IDE_WM_MoveWindow(q3ide_selected_win, wall_pos, wall_normal, qfalse);
			Q3IDE_LOGI("moved [%d] to (%.0f,%.0f,%.0f)", q3ide_selected_win, wall_pos[0], wall_pos[1], wall_pos[2]);
		} else {
			/* No wall — move to floating position in front */
			vec3_t float_pos, float_normal;
			float_pos[0] = eye[0] + fwd[0] * 300.0f;
			float_pos[1] = eye[1] + fwd[1] * 300.0f;
			float_pos[2] = eye[2];
			float_normal[0] = -fwd[0];
			float_normal[1] = -fwd[1];
			float_normal[2] = 0.0f;
			Q3IDE_WM_MoveWindow(q3ide_selected_win, float_pos, float_normal, qfalse);
			Q3IDE_LOGI("moved [%d] floating", q3ide_selected_win);
		}
		q3ide_select_time = Sys_Milliseconds();
		Cbuf_AddText("give ammo\n");
	} else if (hit < 0 && q3ide_selected_win < 0) {
		/* No window hit, no selection — pop next pending window onto this wall.
		 * Burst timer limits sweep rate to Q3IDE_BURST_PLACE_MS between placements. */
		if (Sys_Milliseconds() >= q3ide_burst_place_end) {
			vec3_t wall_pos, wall_normal;
			if (Q3IDE_WM_TraceWall(eye, fwd, wall_pos, wall_normal)) {
				if (Q3IDE_WM_PendingCount() == 0)
					Q3IDE_WM_PopulateQueue(qfalse);
				if (Q3IDE_WM_PendingCount() > 0) {
					Q3IDE_WM_AttachNextPending(wall_pos, wall_normal);
					q3ide_place_cooldown_end = Sys_Milliseconds() + Q3IDE_PLACE_COOLDOWN_MS;
					q3ide_burst_place_end = Sys_Milliseconds() + Q3IDE_BURST_PLACE_MS;
					Cbuf_AddText("give ammo\n");
				}
			}
		}
	}
}
