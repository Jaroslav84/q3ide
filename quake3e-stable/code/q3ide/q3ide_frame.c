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


/* Shoot-to-place — q3ide_shoot_to_place.c */
extern void q3ide_shoot_frame(void);

/* Drag & resize — q3ide_drag_resize.c */
extern void Q3IDE_DragResize_Frame(void);

/* LOS cache update — q3ide_los_cache.c */
extern void Q3IDE_UpdateLOS(const vec3_t eye);

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

		VectorCopy(cl.snap.ps.origin, eye);
		eye[2] += cl.snap.ps.viewheight;
		Q3IDE_WM_UpdatePlayerPos(eye[0], eye[1], eye[2]);

		Q3IDE_UpdateLOS(eye);
		q3ide_shoot_frame();
		Q3IDE_DragResize_Frame();
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
