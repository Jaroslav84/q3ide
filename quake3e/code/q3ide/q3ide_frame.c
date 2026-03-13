/*
 * q3ide_frame.c — Q3IDE_Frame: per-frame engine tick.
 * Engine lifecycle hooks: q3ide_engine.c.  Portal logic: q3ide_portal.c.
 */

#include "q3ide_engine_hooks.h"
#include "q3ide_log.h"
#include "q3ide_params.h"
#include "q3ide_view_modes.h"
#include "q3ide_win_mngr.h"
#include "q3ide_win_mngr_internal.h"
#include "q3ide_aas.h"
#include "../qcommon/qcommon.h"
#include "../client/client.h"
#include <math.h>

extern q3ide_hooks_state_t q3ide_state;
extern int q3ide_last_attack;
extern int q3ide_aimed_win;

/* q3ide_gamma.c */
extern void q3ide_gamma_tick(const char *cur_map);

/* Shoot-to-place — q3ide_engine_hooks_input.c */
extern void q3ide_shoot_frame(void);

static const char *map_pack_name(const char *m)
{
	if (!m || !m[0])
		return "Quake III";
	if (!Q_stricmpn(m, "acid", 4))
		return "Acid";
	if (!Q_stricmpn(m, "cpmctf", 6) || !Q_stricmpn(m, "cpma", 4))
		return "CPMA";
	if (!Q_stricmpn(m, "cpm", 3))
		return "CPMA";
	if (!Q_stricmpn(m, "dc_map", 6))
		return "DC Mappack";
	if (!Q_stricmpn(m, "ztn", 3))
		return "ZTN";
	if (!Q_stricmpn(m, "osp", 3))
		return "OSP";
	if (!Q_stricmpn(m, "q3wcp", 5) || !Q_stricmpn(m, "q3wxs", 5))
		return "Threewave CTF";
	if (!Q_stricmpn(m, "wtf", 3))
		return "WTF Pack";
	if (!Q_stricmpn(m, "13", 2))
		return "sst13";
	if (!Q_stricmpn(m, "pro-", 4) || !Q_stricmpn(m, "pukka", 5) || !Q_stricmp(m, "hub3aeroq3") ||
	    !Q_stricmp(m, "aggressor") || !Q_stricmp(m, "bloodcovenant") || !Q_stricmp(m, "overkill") ||
	    !Q_stricmp(m, "tig_den"))
		return "Pro DM";
	if (!Q_stricmpn(m, "egypt", 5) || !Q_stricmpn(m, "gpl-gypt", 8))
		return "Egyptian";
	if (!Q_stricmpn(m, "jlctf", 5) || !Q_stricmp(m, "q3tourney6_ctf") || !Q_stricmp(m, "QuadCTF") ||
	    !Q_stricmp(m, "q3ctfchnu01"))
		return "CTF";
	if (!Q_stricmpn(m, "q3dm", 4) || !Q_stricmpn(m, "q3tourney", 9))
		return "Quake III";
	return "Custom";
}

void Q3IDE_Frame(void)
{
	if (!q3ide_state.initialized)
		return;

	/* Fire nextdemo after map settles (1 second) */
	if (!q3ide_state.autoexec_done && cls.state == CA_ACTIVE) {
		if (q3ide_state.autoexec_delay == 0)
			q3ide_state.autoexec_delay = Sys_Milliseconds();
		if (Sys_Milliseconds() - q3ide_state.autoexec_delay >= Q3IDE_AUTOEXEC_DELAY_MS) {
			{
				cvar_t *cmd = Cvar_Get("nextdemo", "", 0);
				if (cmd && cmd->string[0]) {
					Q3IDE_LOGI("auto: %s", cmd->string);
					Cbuf_AddText(cmd->string);
					Cbuf_AddText("\n");
					Cvar_Set("nextdemo", "");
				}
			}
			/* Dump all map entities to q3ide.log once on map load — for LLM debugging. */
			{
				const char *es = CM_EntityString();
				const char *p = es;
				const char *tok, *val;
				char classname[64], model[64], origin[64];
				int n = 0, in_ent = 0;
				classname[0] = model[0] = origin[0] = '\0';
				Q3IDE_LOGI("map entities:");
				while ((tok = COM_ParseExt(&p, qtrue)) != NULL && tok[0]) {
					if (tok[0] == '{') {
						in_ent = 1;
						classname[0] = model[0] = origin[0] = '\0';
					} else if (tok[0] == '}' && in_ent) {
						if (classname[0]) {
							Q3IDE_LOGI("  ent[%d] %s  origin=%s%s%s", n++, classname, origin[0] ? origin : "?",
							           model[0] ? "  model=" : "", model[0] ? model : "");
						}
						in_ent = 0;
					} else if (in_ent) {
						val = COM_ParseExt(&p, qtrue);
						if (!val || !val[0])
							break;
						if (!Q_stricmp(tok, "classname"))
							Q_strncpyz(classname, val, sizeof(classname));
						else if (!Q_stricmp(tok, "model"))
							Q_strncpyz(model, val, sizeof(model));
						else if (!Q_stricmp(tok, "origin"))
							Q_strncpyz(origin, val, sizeof(origin));
					}
				}
				Q3IDE_LOGI("map entities: %d total", n);
			}
			{
				const char *mapname = Cvar_VariableString("mapname");
				char banner[64];
				Com_sprintf(banner, sizeof(banner), "%s  /  %s", map_pack_name(mapname), mapname);
				Q3IDE_SetHudMsg(banner, Q3IDE_MAP_BANNER_MS);
			}
			q3ide_state.autoexec_done = qtrue;
		}
	}


	if (cls.state == CA_ACTIVE) {
		vec3_t eye;

		/* Bright-map gamma — detect map change every frame, act only on change */
		q3ide_gamma_tick(Cvar_VariableString("mapname"));

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
					float _nx = _w->normal[0], _ny = _w->normal[1];
					float _hl = sqrtf(_nx * _nx + _ny * _ny);
					float _rx, _ry, _hw, _hh;
					vec3_t _pt;
					trace_t _tr;
					int _pi;
					/* 5 sample points: center + 4 corners inset by Q3IDE_LOS_CORNER_INSET. */
					static const float _cx[5] = {0.0f, -Q3IDE_LOS_CORNER_INSET, Q3IDE_LOS_CORNER_INSET,
					                             Q3IDE_LOS_CORNER_INSET, -Q3IDE_LOS_CORNER_INSET};
					static const float _cy[5] = {0.0f, -Q3IDE_LOS_CORNER_INSET, -Q3IDE_LOS_CORNER_INSET,
					                             Q3IDE_LOS_CORNER_INSET, Q3IDE_LOS_CORNER_INSET};
					static vec3_t _mins = {0, 0, 0}, _maxs = {0, 0, 0};

					_rx = (_hl > 0.01f) ? -_ny / _hl : 1.0f;
					_ry = (_hl > 0.01f) ? _nx / _hl : 0.0f;
					_hw = _w->world_w * 0.5f;
					_hh = _w->world_h * 0.5f;

					/* Proximity exemption: within N× diagonal, always show.
					 * Hysteresis: already-visible windows use larger threshold
					 * to avoid flicker at the boundary. */
					{
						float _diag  = sqrtf(_hw * _hw + _hh * _hh);
						float _thresh = _diag * (_w->los_visible ? Q3IDE_LOS_PROXIMITY_HYST_MULT
						                                          : Q3IDE_LOS_PROXIMITY_MULT);
						if (_w->player_dist < _thresh) {
							_w->los_visible = qtrue;
							continue;
						}
					}

					_w->los_visible = qfalse;
					for (_pi = 0; _pi < 5; _pi++) {
						_pt[0] = _w->origin[0] + _rx * _cx[_pi] * _hw;
						_pt[1] = _w->origin[1] + _ry * _cx[_pi] * _hw;
						_pt[2] = _w->origin[2] + _cy[_pi] * _hh;
						CM_BoxTrace(&_tr, eye, _pt, _mins, _maxs, 0, CONTENTS_SOLID, qfalse);
						if (_tr.fraction >= Q3IDE_LOS_HIT_THRESHOLD) {
							_w->los_visible = qtrue;
							break;
						}
					}
				}
#endif /* Q3IDE_DISABLE_LOS_CHECK */
			}
		}

		/* Per-frame crosshair hover — red border on aimed window */
		if (q3ide_wm.num_active) {
			vec3_t h_eye, h_fwd;
			float _hp, _hy;
			VectorCopy(cl.snap.ps.origin, h_eye);
			h_eye[2] += cl.snap.ps.viewheight;
			_hp = cl.snap.ps.viewangles[PITCH] * ((float) M_PI / 180.0f);
			_hy = cl.snap.ps.viewangles[YAW] * ((float) M_PI / 180.0f);
			h_fwd[0] = cosf(_hp) * cosf(_hy);
			h_fwd[1] = cosf(_hp) * sinf(_hy);
			h_fwd[2] = -sinf(_hp);
			q3ide_aimed_win = Q3IDE_WM_TraceWindowHit(h_eye, h_fwd, -1);
		} else {
			q3ide_aimed_win = -1;
		}

		q3ide_shoot_frame();

	}

	Q3IDE_ViewModes_Tick();
	Q3IDE_WM_PollFrames();

	/* Heartbeat + FPS stats every 5 seconds */
	{
		static unsigned long long last_hb_ms;
		unsigned long long now_ms = Sys_Milliseconds();
		if (now_ms - last_hb_ms >= Q3IDE_HEARTBEAT_MS) {
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
