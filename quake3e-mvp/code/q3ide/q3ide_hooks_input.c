/*
 * q3ide_hooks_input.c — Q3IDE shooting/repositioning input handling.
 */

#include "q3ide_hooks.h"
#include "q3ide_log.h"
#include "q3ide_wm.h"
#include "q3ide_wm_internal.h"
#include "../qcommon/qcommon.h"
#include "../client/client.h"
#include <math.h>

/* Shoot-to-place state — shared with q3ide_hooks.c */
extern int q3ide_selected_win; /* wins[] index, -1 = none selected */
extern int q3ide_select_time;  /* Sys_Milliseconds() when selected */
extern int q3ide_last_attack;  /* previous frame BUTTON_ATTACK state */

#define Q3IDE_REPOSITION_MS 5000 /* ms to shoot destination after selecting */

/* Rapid-fire weapons: machinegun=2, lightning=6, plasmagun=8 */
static qboolean q3ide_is_rapid_fire(int weapon)
{
	return weapon == 2 || weapon == 6 || weapon == 8;
}

void q3ide_shoot_frame(void)
{
	vec3_t eye, fwd;
	float p, y;
	int buttons, attacking, hit;
	qboolean rapid_hold;

	if (cls.state != CA_ACTIVE)
		return;

	buttons = cl.cmds[cl.cmdNumber & CMD_MASK].buttons;
	attacking = buttons & BUTTON_ATTACK;

	/* Rapid-fire continuous reposition: when a window is selected and the player
	 * holds attack with a rapid-fire weapon, reposition every frame without
	 * requiring a new leading edge. */
	rapid_hold = (attacking && q3ide_selected_win >= 0 && q3ide_is_rapid_fire(cl.snap.ps.weapon) &&
	              Sys_Milliseconds() - q3ide_select_time < Q3IDE_REPOSITION_MS);

	/* Only act on the leading edge of the attack button (unless rapid-fire hold) */
	if (!rapid_hold && (!attacking || q3ide_last_attack)) {
		q3ide_last_attack = attacking;
		/* Expire stale selection */
		if (q3ide_selected_win >= 0 && Sys_Milliseconds() - q3ide_select_time >= Q3IDE_REPOSITION_MS) {
			Q3IDE_LOGI("selection expired");
			q3ide_selected_win = -1;
		}
		return;
	}
	q3ide_last_attack = attacking;

	VectorCopy(cl.snap.ps.origin, eye);
	eye[2] += cl.snap.ps.viewheight;
	p = cl.snap.ps.viewangles[PITCH] * (float) M_PI / 180.0f;
	y = cl.snap.ps.viewangles[YAW] * (float) M_PI / 180.0f;
	fwd[0] = cosf(p) * cosf(y);
	fwd[1] = cosf(p) * sinf(y);
	fwd[2] = -sinf(p);

	hit = Q3IDE_WM_TraceWindowHit(eye, fwd);

	if (hit >= 0) {
		/* Shot hit a window → select it */
		q3ide_selected_win = hit;
		q3ide_select_time = Sys_Milliseconds();
		Q3IDE_LOGI("selected [%d] -> shoot surface to move (5s)", hit);
		Cbuf_AddText("give ammo\n");
	} else if (q3ide_selected_win >= 0 && Sys_Milliseconds() - q3ide_select_time < Q3IDE_REPOSITION_MS) {
		/* Selection active, shot missed windows → move to hit surface */
		vec3_t wall_pos, wall_normal;
		if (Q3IDE_WM_TraceWall(eye, fwd, wall_pos, wall_normal)) {
			wall_pos[2] = eye[2];                                                   /* keep at eye height */
			Q3IDE_WM_MoveWindow(q3ide_selected_win, wall_pos, wall_normal, qfalse); /* user-placed: clamp to fit */
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
		q3ide_select_time = Sys_Milliseconds(); /* restart 5s window — keep shooting to keep moving */
		Cbuf_AddText("give ammo\n");
	}
}
