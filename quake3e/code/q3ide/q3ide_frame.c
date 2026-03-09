/*
 * q3ide_frame.c — Q3IDE_Frame: per-frame engine tick.
 * Engine lifecycle hooks: q3ide_engine.c.  Portal logic: q3ide_portal.c.
 */

#include "q3ide_hooks.h"
#include "q3ide_log.h"
#include "q3ide_wm.h"
#include "q3ide_wm_internal.h"
#include "q3ide_interaction.h"
#include "q3ide_aas.h"
#include "q3ide_layout.h"
#include "../qcommon/qcommon.h"
#include "../client/client.h"

extern q3ide_hooks_state_t q3ide_state;
extern int q3ide_last_attack;

/* Grapple helpers — q3ide_hooks_grapple.c */
extern void q3ide_grapple_type_frame(void);
extern void q3ide_grapple_window_frame(void);

/* Shoot-to-place — q3ide_hooks_input.c */
extern void q3ide_shoot_frame(void);

void Q3IDE_Frame(void)
{
	if (!q3ide_state.initialized)
		return;

	/* Fire nextdemo after map settles (~60 frames) */
	if (!q3ide_state.autoexec_done && cls.state == CA_ACTIVE) {
		if (++q3ide_state.autoexec_delay > 60) {
			q3ide_state.stream_last_area = Q3IDE_AAS_PointArea(cl.snap.ps.origin);
			q3ide_state.stream_cooldown = 60;
			Cbuf_AddText("give grappling hook\nweapon 10\n");
			{
				cvar_t *cmd = Cvar_Get("nextdemo", "", 0);
				if (cmd && cmd->string[0]) {
					Com_Printf("q3ide: auto: %s\n", cmd->string);
					Cbuf_AddText(cmd->string);
					Cbuf_AddText("\n");
					Cvar_Set("nextdemo", "");
				}
			}
			q3ide_state.autoexec_done = qtrue;
		}
	}

	/* Streaming placement: reflow when player enters a new AAS area. */
	if (q3ide_wm.num_active && cls.state == CA_ACTIVE) {
		if (q3ide_state.stream_cooldown > 0) {
			q3ide_state.stream_cooldown--;
		} else if (Q3IDE_AAS_IsLoaded()) {
			int cur_area = Q3IDE_AAS_PointArea(cl.snap.ps.origin);
			if (cur_area > 0 && cur_area != q3ide_state.stream_last_area) {
				q3ide_state.stream_last_area = cur_area;
				Q3IDE_WM_Reflow();
				q3ide_state.stream_cooldown = 30;
			}
		}
	}

	if (cls.state == CA_ACTIVE) {
		vec3_t eye;
		int buttons;
		float cur_yaw, cur_pitch, mouse_dx, mouse_dy;
		qboolean attacking, use_key, escape, lock_key;

		VectorCopy(cl.snap.ps.origin, eye);
		eye[2] += cl.snap.ps.viewheight;
		Q3IDE_WM_UpdatePlayerPos(eye[0], eye[1], eye[2]);

		{
			int cur_health = cl.snap.ps.stats[STAT_HEALTH];
			if (q3ide_state.last_health <= 0 && cur_health > 0)
				Cbuf_AddText("give grappling hook\nweapon 10\n");
			q3ide_state.last_health = cur_health;
		}

		cur_yaw = cl.snap.ps.viewangles[YAW];
		cur_pitch = cl.snap.ps.viewangles[PITCH];
		mouse_dx = (cur_yaw - q3ide_state.last_yaw) * 8.0f;
		mouse_dy = (cur_pitch - q3ide_state.last_pitch) * 8.0f;
		q3ide_state.last_yaw = cur_yaw;
		q3ide_state.last_pitch = cur_pitch;

		buttons =
		    Q3IDE_Interaction_ConsumesInput() ? q3ide_state.raw_buttons : cl.cmds[cl.cmdNumber & CMD_MASK].buttons;
		attacking = (buttons & BUTTON_ATTACK) && !(q3ide_last_attack);
		use_key = (Cvar_VariableIntegerValue("q3ide_use_key") == 1);
		if (use_key)
			Cvar_Set("q3ide_use_key", "0");
		escape = (Cvar_VariableIntegerValue("q3ide_escape") == 1);
		if (escape)
			Cvar_Set("q3ide_escape", "0");
		lock_key = (Cvar_VariableIntegerValue("q3ide_lock") == 1);
		if (lock_key)
			Cvar_Set("q3ide_lock", "0");

		Q3IDE_Interaction_Frame(attacking, use_key, escape, lock_key, mouse_dx, mouse_dy);
		Q3IDE_UpdateEntityHover();

		if (!Q3IDE_Interaction_ConsumesInput())
			q3ide_shoot_frame();
		else
			q3ide_last_attack = buttons & BUTTON_ATTACK;

		q3ide_grapple_type_frame();
		q3ide_grapple_window_frame();
	}

	q3ide_layout_tick();
	Q3IDE_WM_ReflowTick();
	Q3IDE_WM_PollFrames();

	/* Heartbeat every 5 seconds so crash time is visible in q3ide.log */
	{
		static unsigned long long last_hb_ms;
		unsigned long long now_ms = Sys_Milliseconds();
		if (now_ms - last_hb_ms >= 5000) {
			Q3IDE_LOGI("heartbeat active=%d", q3ide_wm.num_active);
			last_hb_ms = now_ms;
		}
	}
}
