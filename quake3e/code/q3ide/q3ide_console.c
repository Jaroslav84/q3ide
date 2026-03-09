/*
 * q3ide_console.c — Q3IDE_Cmd_f: console command dispatcher.
 * Engine hooks: q3ide_engine.c.  Portal helpers: q3ide_portal.c.
 */

#include "q3ide_hooks.h"
#include "q3ide_log.h"
#include "q3ide_wm.h"
#include "q3ide_wm_internal.h"
#include "q3ide_interaction.h"
#include "../qcommon/qcommon.h"
#include "../client/client.h"
#include <math.h>

extern q3ide_hooks_state_t q3ide_state;
extern int q3ide_selected_win;
extern int q3ide_select_time;

#define Q3IDE_REPOSITION_MS 5000

void Q3IDE_Cmd_f(void)
{
	const char *sub;
	if (Cmd_Argc() < 2) {
		Com_Printf("usage: q3ide <list|attach|detach|desktop|status>\n");
		return;
	}
	sub = Cmd_Argv(1);
	if (!Q_stricmp(sub, "list"))
		Q3IDE_WM_CmdList();
	else if (!Q_stricmp(sub, "attach"))
		Q3IDE_WM_CmdAttach();
	else if (!Q_stricmp(sub, "detach")) {
		if (Cmd_Argc() >= 3)
			Q3IDE_WM_DetachById((unsigned int) atoi(Cmd_Argv(2)));
		else
			Q3IDE_WM_CmdDetachAll();
	} else if (!Q_stricmp(sub, "desktop"))
		Q3IDE_WM_CmdDesktop();
	else if (!Q_stricmp(sub, "snap"))
		Q3IDE_WM_CmdSnap();
	else if (!Q_stricmp(sub, "status"))
		Q3IDE_WM_CmdStatus();
	else if (!Q_stricmp(sub, "istate")) {
		static const char *mode_names[] = {"FPS", "POINTER", "KEYBOARD"};
		Com_Printf("q3ide istate: mode=%s focused_win=%d hover_t=%.2f dist=%.0f"
		           " focused_uv=(%.3f,%.3f) pointer_uv=(%.3f,%.3f)\n",
		           mode_names[q3ide_interaction.mode], q3ide_interaction.focused_win, q3ide_interaction.hover_t,
		           q3ide_interaction.focused_dist, q3ide_interaction.focused_uv[0], q3ide_interaction.focused_uv[1],
		           q3ide_interaction.pointer_uv[0], q3ide_interaction.pointer_uv[1]);
		if (q3ide_interaction.focused_win >= 0 && q3ide_interaction.focused_win < Q3IDE_MAX_WIN) {
			q3ide_win_t *fw = &q3ide_wm.wins[q3ide_interaction.focused_win];
			Com_Printf("  win: origin=(%.0f,%.0f,%.0f) normal=(%.2f,%.2f,%.2f)"
			           " world_w=%.0f world_h=%.0f\n",
			           fw->origin[0], fw->origin[1], fw->origin[2], fw->normal[0], fw->normal[1], fw->normal[2],
			           fw->world_w, fw->world_h);
		}
		if (q3ide_selected_win >= 0) {
			int ms_left = Q3IDE_REPOSITION_MS - (Sys_Milliseconds() - q3ide_select_time);
			Com_Printf("  reposition: win=%d time_left=%dms\n", q3ide_selected_win, ms_left > 0 ? ms_left : 0);
		} else {
			Com_Printf("  reposition: idle\n");
		}
	} else if (!Q_stricmp(sub, "entities")) {
		const char *es = CM_EntityString();
		const char *p = es;
		const char *tok, *val;
		vec3_t ppos;
		char classname[64], model[64];
		float ox, oy, oz, dx, dy, dz, d;
		int found = 0, in_ent = 0;

		if (cls.state != CA_ACTIVE) {
			Com_Printf("q3ide: not in game\n");
			return;
		}
		VectorCopy(cl.snap.ps.origin, ppos);
		Com_Printf("q3ide: entities within 640u of (%.0f,%.0f,%.0f):\n", ppos[0], ppos[1], ppos[2]);
		classname[0] = model[0] = '\0';
		ox = oy = oz = 0.0f;

		while ((tok = COM_ParseExt(&p, qtrue)) != NULL && tok[0]) {
			if (tok[0] == '{') {
				in_ent = 1;
				classname[0] = model[0] = '\0';
				ox = oy = oz = 0.0f;
			} else if (tok[0] == '}' && in_ent) {
				if (classname[0]) {
					dx = ox - ppos[0];
					dy = oy - ppos[1];
					dz = oz - ppos[2];
					d = sqrtf(dx * dx + dy * dy + dz * dz);
					if (d < 640.0f) {
						Com_Printf("  [%du] %-32s (%.0f,%.0f,%.0f)%s%s\n", (int) d, classname, ox, oy, oz,
						           model[0] ? " " : "", model[0] ? model : "");
						found++;
					}
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
					sscanf(val, "%f %f %f", &ox, &oy, &oz);
			}
		}
		Com_Printf("q3ide: %d entities found\n", found);
	} else if (!Q_stricmp(sub, "portal")) {
		if (!Q3IDE_WM_MirrorActive()) {
			Com_Printf("q3ide portal: NOT ACTIVE\n");
		} else {
			vec3_t origin, normal, pos, diff, right;
			float ww, wh, dist, lx, ly, nx, ny, len;
			Q3IDE_WM_GetMirrorOrigin(origin, normal, &ww, &wh);
			Com_Printf("q3ide portal: origin=(%.0f,%.0f,%.0f) normal=(%.2f,%.2f,%.2f)"
			           " size=%.0fx%.0f\n",
			           origin[0], origin[1], origin[2], normal[0], normal[1], normal[2], ww, wh);
			if (cls.state == CA_ACTIVE) {
				VectorCopy(cl.snap.ps.origin, pos);
				VectorSubtract(pos, origin, diff);
				dist = DotProduct(diff, normal);
				nx = normal[0];
				ny = normal[1];
				len = sqrtf(nx * nx + ny * ny);
				if (len > 0.01f) {
					right[0] = -ny / len;
					right[1] = nx / len;
					right[2] = 0.0f;
				} else {
					right[0] = 1.0f;
					right[1] = 0.0f;
					right[2] = 0.0f;
				}
				lx = DotProduct(diff, right);
				ly = diff[2];
				Com_Printf("  player=(%.0f,%.0f,%.0f) dist=%.1f lx=%.1f ly=%.1f"
				           " hw=%.0f hh=%.0f\n",
				           pos[0], pos[1], pos[2], dist, lx, ly, ww * 0.5f, wh * 0.5f);
				Com_Printf("  in_bounds=%d trigger=%d\n", (fabsf(lx) < ww * 0.5f && fabsf(ly) < wh * 0.5f + 24.0f),
				           (fabsf(dist) < 24.0f));
			}
		}
	} else if (!Q_stricmp(sub, "laser")) {
		extern qboolean q3ide_laser_active;
		Com_Printf("q3ide laser: active=%d\n", q3ide_laser_active);
	} else
		Com_Printf("q3ide: unknown sub-command '%s'\n", sub);
}
