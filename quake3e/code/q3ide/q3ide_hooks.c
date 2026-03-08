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
#include "q3ide_wm_internal.h"
#include "q3ide_interaction.h"
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
	int selected_win;           /* wins[] index, -1 = none selected */
	int select_time;            /* Sys_Milliseconds() when selected */
	int last_attack;            /* previous frame BUTTON_ATTACK state */
	int last_buttons;           /* previous frame button state for edge detection */
	float last_yaw, last_pitch; /* for mouse delta calculation */
	/* raw button state saved before BUTTON_ATTACK suppression */
	int raw_buttons;
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
	else if (!Q_stricmp(sub, "istate")) {
		static const char *mode_names[] = {"FPS", "POINTER", "KEYBOARD"};
		Com_Printf("q3ide istate: mode=%s focused_win=%d hover_t=%.2f dist=%.0f focused_uv=(%.3f,%.3f) "
		           "pointer_uv=(%.3f,%.3f)\n",
		           mode_names[q3ide_interaction.mode], q3ide_interaction.focused_win, q3ide_interaction.hover_t,
		           q3ide_interaction.focused_dist, q3ide_interaction.focused_uv[0], q3ide_interaction.focused_uv[1],
		           q3ide_interaction.pointer_uv[0], q3ide_interaction.pointer_uv[1]);
		if (q3ide_interaction.focused_win >= 0 && q3ide_interaction.focused_win < Q3IDE_MAX_WIN) {
			q3ide_win_t *fw = &q3ide_wm.wins[q3ide_interaction.focused_win];
			Com_Printf("  win: origin=(%.0f,%.0f,%.0f) normal=(%.2f,%.2f,%.2f) world_w=%.0f world_h=%.0f\n",
			           fw->origin[0], fw->origin[1], fw->origin[2], fw->normal[0], fw->normal[1], fw->normal[2],
			           fw->world_w, fw->world_h);
		}
		if (q3ide_state.selected_win >= 0) {
			int ms_left = Q3IDE_REPOSITION_MS - (Sys_Milliseconds() - q3ide_state.select_time);
			Com_Printf("  reposition: win=%d time_left=%dms\n", q3ide_state.selected_win, ms_left > 0 ? ms_left : 0);
		} else {
			Com_Printf("  reposition: idle\n");
		}
	} else
		Com_Printf("q3ide: unknown sub-command '%s'\n", sub);
}

/* ============================================================
 *  Shoot-to-place helpers
 * ============================================================ */

/* Rapid-fire weapons: machinegun=2, lightning=6, plasmagun=8 */
static qboolean q3ide_is_rapid_fire(int weapon)
{
	return weapon == 2 || weapon == 6 || weapon == 8;
}

static void q3ide_shoot_frame(void)
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
	rapid_hold = (attacking && q3ide_state.selected_win >= 0 && q3ide_is_rapid_fire(cl.snap.ps.weapon) &&
	              Sys_Milliseconds() - q3ide_state.select_time < Q3IDE_REPOSITION_MS);

	/* Only act on the leading edge of the attack button (unless rapid-fire hold) */
	if (!rapid_hold && (!attacking || q3ide_state.last_attack)) {
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
		q3ide_state.select_time = Sys_Milliseconds(); /* restart 5s window — keep shooting to keep moving */
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

	Q3IDE_Interaction_Init();

	Cmd_AddCommand("q3ide", Q3IDE_Cmd_f);
	q3ide_state.initialized = qtrue;
	Com_Printf("q3ide: Key bindings:\n");
	Com_Printf("  bind <key> \"set q3ide_pointer 1\"     (enter Pointer mode — aim at window first)\n");
	Com_Printf("  bind <key> \"set q3ide_use_key 1\"     (enter Keyboard mode from Pointer)\n");
	Com_Printf("  bind <key> \"set q3ide_escape 1\"      (exit Pointer/Keyboard modes)\n");
}

void Q3IDE_OnVidRestart(void)
{
	Q3IDE_WM_InvalidateShaders();
}

void Q3IDE_Frame(void)
{
	vec3_t eye;
	int buttons;
	float cur_yaw, cur_pitch, mouse_dx = 0.0f, mouse_dy = 0.0f;
	qboolean attacking = qfalse, use_key = qfalse, escape = qfalse, pointer_in = qfalse;

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

	if (cls.state == CA_ACTIVE) {
		/* Update player position for distance-based rendering */
		VectorCopy(cl.snap.ps.origin, eye);
		eye[2] += cl.snap.ps.viewheight;
		Q3IDE_WM_UpdatePlayerPos(eye[0], eye[1], eye[2]);

		/* Compute mouse delta from angle changes */
		cur_yaw = cl.snap.ps.viewangles[YAW];
		cur_pitch = cl.snap.ps.viewangles[PITCH];
		mouse_dx = (cur_yaw - q3ide_state.last_yaw) * 8.0f;
		mouse_dy = (cur_pitch - q3ide_state.last_pitch) * 8.0f;
		q3ide_state.last_yaw = cur_yaw;
		q3ide_state.last_pitch = cur_pitch;

		/* Detect button state changes.
		 * When ConsumesInput(), BUTTON_ATTACK was cleared from the outgoing
		 * usercmd (in CL_CreateNewCommands) to suppress weapon fire. Read the
		 * raw_buttons saved before that clearing so we can still detect clicks. */
		buttons =
		    Q3IDE_Interaction_ConsumesInput() ? q3ide_state.raw_buttons : cl.cmds[cl.cmdNumber & CMD_MASK].buttons;
		/* Leading edge: use last_attack that q3ide_shoot_frame also reads.
		 * Do NOT pre-update last_attack here — q3ide_shoot_frame() owns it.
		 * If ConsumesInput(), update it ourselves after. */
		attacking = (buttons & BUTTON_ATTACK) && !(q3ide_state.last_attack);
		/* use_key: bind a key to "set q3ide_use_key 1" */
		use_key = (Cvar_VariableIntegerValue("q3ide_use_key") == 1);
		if (use_key)
			Cvar_Set("q3ide_use_key", "0");
		escape = (Cvar_VariableIntegerValue("q3ide_escape") == 1);
		if (escape)
			Cvar_Set("q3ide_escape", "0");
		pointer_in = (Cvar_VariableIntegerValue("q3ide_pointer") == 1);
		if (pointer_in)
			Cvar_Set("q3ide_pointer", "0");

		/* Update interaction state */
		Q3IDE_Interaction_Frame(attacking, use_key, escape, pointer_in, mouse_dx, mouse_dy);

		/* Only call shoot_frame if interaction doesn't consume input.
		 * shoot_frame() manages last_attack internally.
		 * When input is consumed, update last_attack ourselves. */
		if (!Q3IDE_Interaction_ConsumesInput())
			q3ide_shoot_frame();
		else
			q3ide_state.last_attack = buttons & BUTTON_ATTACK;
		/* Note: BUTTON_ATTACK suppression happens in CL_CreateNewCommands
		 * (cl_input.c) via Q3IDE_ConsumesInput(), which runs before
		 * CL_WritePacket. Clearing it here would be too late. */
	}

	Q3IDE_WM_PollFrames();
}

void Q3IDE_AddPolysToScene(void)
{
	if (!q3ide_state.initialized)
		return;
	Q3IDE_WM_AddPolys();
}

qboolean Q3IDE_ConsumesInput(void)
{
	if (!q3ide_state.initialized)
		return qfalse;
	return Q3IDE_Interaction_ConsumesInput();
}

void Q3IDE_SaveRawButtons(int buttons)
{
	q3ide_state.raw_buttons = buttons;
}

qboolean Q3IDE_OnKeyEvent(int key, qboolean down)
{
	if (!q3ide_state.initialized)
		return qfalse;
	return Q3IDE_Interaction_OnKeyEvent(key, down);
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
