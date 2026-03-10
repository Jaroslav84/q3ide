/*
 * q3ide_frame.c — Q3IDE_Frame: per-frame engine tick.
 * Engine lifecycle hooks: q3ide_engine.c.  Portal logic: q3ide_portal.c.
 */

#include "q3ide_engine_hooks.h"
#include "q3ide_log.h"
#include "q3ide_params.h"
#include "q3ide_win_mngr.h"
#include "q3ide_win_mngr_internal.h"
#include "q3ide_interaction.h"
#include "q3ide_aas.h"
#include "../qcommon/qcommon.h"
#include "../client/client.h"
#include <math.h>

extern q3ide_hooks_state_t q3ide_state;
extern int q3ide_last_attack;

/* Grapple helpers — q3ide_engine_hooks_grapple.c */
extern void q3ide_grapple_type_frame(void);
extern void q3ide_grapple_window_frame(void);

/* Spawn-focus — q3ide_spawn.c */
extern void q3ide_spawn_focus_terminal(const vec3_t eye);

/* Shoot-to-place — q3ide_engine_hooks_input.c */
extern void q3ide_shoot_frame(void);

void Q3IDE_Frame(void)
{
	if (!q3ide_state.initialized)
		return;

	/* Fire nextdemo after map settles (1 second) */
	if (!q3ide_state.autoexec_done && cls.state == CA_ACTIVE) {
		if (q3ide_state.autoexec_delay == 0)
			q3ide_state.autoexec_delay = Sys_Milliseconds();
		if (Sys_Milliseconds() - q3ide_state.autoexec_delay >= 1000) {
			q3ide_state.stream_last_area = Q3IDE_AAS_PointArea(cl.snap.ps.origin);
			Cbuf_AddText("give grappling hook\nweapon 10\nq3ide attach all\n");
			{
				cvar_t *cmd = Cvar_Get("nextdemo", "", 0);
				if (cmd && cmd->string[0]) {
					Q3IDE_LOGI("auto: %s", cmd->string);
					Cbuf_AddText(cmd->string);
					Cbuf_AddText("\n");
					Cvar_Set("nextdemo", "");
				}
			}
			q3ide_state.autoexec_done = qtrue;
		}
	}

	/* Track current AAS area (for future placement use). */
	if (cls.state == CA_ACTIVE && Q3IDE_AAS_IsLoaded()) {
		int cur_area = Q3IDE_AAS_PointArea(cl.snap.ps.origin);
		if (cur_area > 0 && cur_area != q3ide_state.stream_last_area) {
			q3ide_state.stream_last_area = cur_area;
			Q3IDE_LOGI("area change: %d", cur_area);
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

		/* Cache LOS visibility once per frame — reused across all monitor render passes.
		 * Pixel-perfect: trace to center + 4 corners using the same right/up basis as
		 * the renderer. Visible if any of the 5 points has an unobstructed path.
		 * Works from both sides — no normal-offset tricks needed. */
		if (q3ide_wm.num_active) {
			int _li;
			for (_li = 0; _li < Q3IDE_MAX_WIN; _li++) {
				q3ide_win_t *_w = &q3ide_wm.wins[_li];
				if (!_w->active) {
					_w->los_visible = qfalse;
					continue;
				}
#if Q3IDE_DISABLE_LOS_CHECK
				_w->los_visible = qtrue;
#else
				{
					/* Build same right/up basis as q3ide_win_basis(). */
					float  _nx = _w->normal[0], _ny = _w->normal[1];
					float  _hl = sqrtf(_nx * _nx + _ny * _ny);
					float  _rx, _ry, _hw, _hh;
					vec3_t _pt;
					trace_t _tr;
					int    _pi;
					/* 5 sample points: center, BL, BR, TR, TL corners (inset 10%). */
					static const float _cx[5] = {0.0f, -0.9f,  0.9f, 0.9f, -0.9f};
					static const float _cy[5] = {0.0f, -0.9f, -0.9f, 0.9f,  0.9f};
					static vec3_t      _mins  = {0, 0, 0}, _maxs = {0, 0, 0};

					_rx = (_hl > 0.01f) ? -_ny / _hl : 1.0f;
					_ry = (_hl > 0.01f) ?  _nx / _hl : 0.0f;
					_hw = _w->world_w * 0.5f;
					_hh = _w->world_h * 0.5f;

					/* Proximity exemption: within 1 window diagonal the player is
					 * clearly next to the TV — skip LOS, always show it. */
					if (_w->player_dist < sqrtf(_hw * _hw + _hh * _hh) * 2.0f) {
						_w->los_visible = qtrue;
						continue;
					}

					_w->los_visible = qfalse;
					for (_pi = 0; _pi < 5; _pi++) {
						_pt[0] = _w->origin[0] + _rx * _cx[_pi] * _hw;
						_pt[1] = _w->origin[1] + _ry * _cx[_pi] * _hw;
						_pt[2] = _w->origin[2] + _cy[_pi] * _hh;
						CM_BoxTrace(&_tr, eye, _pt, _mins, _maxs, 0, CONTENTS_SOLID, qfalse);
						if (_tr.fraction >= 0.95f) {
							_w->los_visible = qtrue;
							break;
						}
					}
				}
#endif /* Q3IDE_DISABLE_LOS_CHECK */
			}
		}

		{
			int cur_health = cl.snap.ps.stats[STAT_HEALTH];
			if (q3ide_state.last_health <= 0 && cur_health > 0) {
				Cbuf_AddText("give grappling hook\nweapon 10\n");
				q3ide_spawn_focus_terminal(eye);
			}
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

	Q3IDE_WM_PollFrames();

	/* Heartbeat + FPS stats every 5 seconds */
	{
		static unsigned long long last_hb_ms;
		unsigned long long now_ms = Sys_Milliseconds();
		if (now_ms - last_hb_ms >= 5000) {
			static int last_framecount;
			unsigned long long elapsed = now_ms - last_hb_ms;
			int frames = cls.framecount - last_framecount;
			int fps = (int) ((unsigned long long) frames * 1000 / elapsed);
			Q3IDE_LOGI("heartbeat active=%d fps=%d uploads=%d", q3ide_wm.num_active, fps, q3ide_wm.frame_uploads);
			q3ide_wm.frame_uploads = 0;
			last_hb_ms = now_ms;
			last_framecount = cls.framecount;
		}
	}
}
