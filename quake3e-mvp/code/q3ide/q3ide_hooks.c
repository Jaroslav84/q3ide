/*
 * q3ide_hooks.c — Q3IDE engine integration hooks.
 *
 * Hooks added to Quake3e engine files (all guarded by #ifdef USE_Q3IDE):
 *   cl_main.c:  Q3IDE_Init, Q3IDE_OnVidRestart, Q3IDE_Frame, Q3IDE_Shutdown
 *   cl_cgame.c: Q3IDE_AddPolysToScene (before re.RenderScene)
 *
 * Shoot-to-place:
 *   First shot on a window  → selects it (5s window)
 *   Second shot on any surface → moves the selected window there
 *   "give ammo" is injected after each shot so the weapon never dry-fires.
 */

#include "q3ide_hooks.h"
#include "q3ide_wm.h"
#include "../qcommon/qcommon.h"
#include "../client/client.h"

#define Q3IDE_REPOSITION_MS 5000 /* ms to shoot destination after selecting */

/* ============================================================
 *  State
 * ============================================================ */

static struct {
	qboolean initialized;
	qboolean autoexec_done;
	int autoexec_delay;
	/* shoot-to-place */
	int selected_win; /* wins[] index, -1 = none selected */
	int select_time;  /* Sys_Milliseconds() when selected */
	int last_attack;  /* previous frame BUTTON_ATTACK state */
} q3ide_state;

/* ============================================================
 *  Console command dispatcher
 * ============================================================ */

static void Q3IDE_Cmd_f(void)
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
	else if (!Q_stricmp(sub, "status"))
		Q3IDE_WM_CmdStatus();
	else
		Com_Printf("q3ide: unknown sub-command '%s'\n", sub);
}

/* ============================================================
 *  Shoot-to-place helpers
 * ============================================================ */

static void q3ide_shoot_frame(void)
{
	vec3_t eye, fwd;
	float p, y;
	int buttons, attacking, hit;

	if (cls.state != CA_ACTIVE)
		return;

	buttons = cl.cmds[cl.cmdNumber & CMD_MASK].buttons;
	attacking = buttons & BUTTON_ATTACK;

	/* Only act on the leading edge of the attack button */
	if (!attacking || q3ide_state.last_attack) {
		q3ide_state.last_attack = attacking;
		/* Expire stale selection */
		if (q3ide_state.selected_win >= 0 && Sys_Milliseconds() - q3ide_state.select_time >= Q3IDE_REPOSITION_MS) {
			Com_Printf("q3ide: selection expired\n");
			q3ide_state.selected_win = -1;
		}
		return;
	}
	q3ide_state.last_attack = attacking;

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
		q3ide_state.selected_win = hit;
		q3ide_state.select_time = Sys_Milliseconds();
		Com_Printf("q3ide: selected [%d] -> shoot surface to move (5s)\n", hit);
		Cbuf_AddText("give ammo\n");
	} else if (q3ide_state.selected_win >= 0 && Sys_Milliseconds() - q3ide_state.select_time < Q3IDE_REPOSITION_MS) {
		/* Selection active, shot missed windows → move to hit surface */
		vec3_t wall_pos, wall_normal;
		if (Q3IDE_WM_TraceWall(eye, fwd, wall_pos, wall_normal)) {
			wall_pos[2] = eye[2]; /* keep at eye height */
			Q3IDE_WM_MoveWindow(q3ide_state.selected_win, wall_pos, wall_normal);
			Com_Printf("q3ide: moved [%d] to (%.0f,%.0f,%.0f)\n", q3ide_state.selected_win, wall_pos[0], wall_pos[1],
			           wall_pos[2]);
		} else {
			/* No wall — move to floating position in front */
			vec3_t float_pos, float_normal;
			float_pos[0] = eye[0] + fwd[0] * 300.0f;
			float_pos[1] = eye[1] + fwd[1] * 300.0f;
			float_pos[2] = eye[2];
			float_normal[0] = -fwd[0];
			float_normal[1] = -fwd[1];
			float_normal[2] = 0.0f;
			Q3IDE_WM_MoveWindow(q3ide_state.selected_win, float_pos, float_normal);
			Com_Printf("q3ide: moved [%d] floating\n", q3ide_state.selected_win);
		}
		q3ide_state.selected_win = -1;
		Cbuf_AddText("give ammo\n");
	}
}

/* ============================================================
 *  Public engine hooks
 * ============================================================ */

void Q3IDE_Init(void)
{
	memset(&q3ide_state, 0, sizeof(q3ide_state));
	q3ide_state.selected_win = -1;

	if (Q3IDE_WM_Init())
		Com_Printf("q3ide: capture ready\n");
	else
		Com_Printf("q3ide: running without capture\n");

	Cmd_AddCommand("q3ide", Q3IDE_Cmd_f);
	q3ide_state.initialized = qtrue;
}

void Q3IDE_OnVidRestart(void)
{
	Q3IDE_WM_InvalidateShaders();
}

void Q3IDE_Frame(void)
{
	if (!q3ide_state.initialized)
		return;

	/* Fire nextdemo after map settles (~60 frames) */
	if (!q3ide_state.autoexec_done && cls.state == CA_ACTIVE) {
		if (++q3ide_state.autoexec_delay > 60) {
			cvar_t *cmd = Cvar_Get("nextdemo", "", 0);
			if (cmd && cmd->string[0]) {
				Com_Printf("q3ide: auto: %s\n", cmd->string);
				Cbuf_AddText(cmd->string);
				Cbuf_AddText("\n");
				Cvar_Set("nextdemo", "");
			}
			q3ide_state.autoexec_done = qtrue;
		}
	}

	q3ide_shoot_frame();
	Q3IDE_WM_PollFrames();
}

void Q3IDE_AddPolysToScene(void)
{
	if (!q3ide_state.initialized)
		return;
	Q3IDE_WM_AddPolys();
}

void Q3IDE_Shutdown(void)
{
	if (!q3ide_state.initialized)
		return;
	Q3IDE_WM_Shutdown();
	Cmd_RemoveCommand("q3ide");
	q3ide_state.initialized = qfalse;
	Com_Printf("q3ide: shutdown\n");
}
