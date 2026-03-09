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
#include "q3ide_log.h"
#include "q3ide_wm.h"
#include "q3ide_wm_internal.h"
#include "q3ide_interaction.h"
#include "../qcommon/qcommon.h"
#include "../client/client.h"
#include <math.h>

/* Direct server playerState access — bypasses game module, no sv_cheats required.
 * SV_GameClientNum returns pointer to game's authoritative playerState_t for client N. */
extern playerState_t *SV_GameClientNum(int num);

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
	float last_yaw, last_pitch; /* for mouse delta calculation */
	/* raw button state saved before BUTTON_ATTACK suppression */
	int raw_buttons;
	int last_health; /* detect respawn (health 0 → >0) */
	/* portal teleport */
	int portal_cooldown; /* frames left after teleport, 0 = ready */
	/* grapple-to-window teleport */
	int grapple_tele_cooldown; /* frames left after teleport, 0 = ready */
} q3ide_state;

/* ============================================================
 *  Portal teleport
 * ============================================================ */

/*
 * q3ide_portal_frame — walk-through teleport.
 *
 * Fires when player enters the portal's trigger volume (96u deep, portal-sized).
 * Teleports by directly writing the server-authoritative playerState — bypasses
 * the game module entirely (no sv_cheats dependency).
 * EF_TELEPORT_BIT toggle signals cgame to skip position interpolation.
 * Destination overrideable via cvars: q3ide_tele_x/y/z/yaw.
 */
static void q3ide_portal_frame(void)
{
	vec3_t pos, diff, origin, normal, right;
	float dist, lx, ly, hw, hh, nx, ny, len, ww, wh;
	int in_bounds;
	cvar_t *tx, *ty, *tz, *tyaw;

	if (!Q3IDE_WM_MirrorActive() || cls.state != CA_ACTIVE)
		return;

	if (q3ide_state.portal_cooldown > 0) {
		q3ide_state.portal_cooldown--;
		return;
	}

	Q3IDE_WM_GetMirrorOrigin(origin, normal, &ww, &wh);

	VectorCopy(cl.snap.ps.origin, pos);
	VectorSubtract(pos, origin, diff);
	dist = DotProduct(diff, normal);

	/* Build right vector perpendicular to normal in XY plane */
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
	hw = ww * 0.5f;
	hh = wh * 0.5f;
	/* Fire when player's body intersects the portal face (dist < 24 = player half-width).
	 * in_bounds uses portal face dimensions so only the energy visual triggers, not the air around it. */
	in_bounds = (fabsf(lx) < hw && fabsf(ly) < hh + 24.0f);

	if (fabsf(dist) < 24.0f && in_bounds) {
		char cmd[128];
		tx   = Cvar_Get("q3ide_tele_x",   "188",  0);
		ty   = Cvar_Get("q3ide_tele_y",   "-360", 0);
		tz   = Cvar_Get("q3ide_tele_z",   "20",   0);
		tyaw = Cvar_Get("q3ide_tele_yaw", "90",   0);
		Com_sprintf(cmd, sizeof(cmd), "setviewpos %.0f %.0f %.0f %.0f",
		            tx->value, ty->value, tz->value, tyaw->value);
		Q3IDE_LOGI("portal FIRE dist=%.1f cmd=[%s]", dist, cmd);
		CL_AddReliableCommand(cmd, qfalse);
		q3ide_state.portal_cooldown = 180; /* 3s at 60fps */
	}
}

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
	else if (!Q_stricmp(sub, "snap"))
		Q3IDE_WM_CmdSnap();
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
	} else if (!Q_stricmp(sub, "entities")) {
		/* List BSP entities within 640 units (20m) of current player position */
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
						Com_Printf("  [%.0fu] %-32s (%.0f,%.0f,%.0f)%s%s\n", (int) d, classname, ox, oy, oz,
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
		/* Show mirror/portal state and player proximity */
		if (!Q3IDE_WM_MirrorActive()) {
			Com_Printf("q3ide portal: NOT ACTIVE (mirror not placed yet)\n");
		} else {
			vec3_t origin, normal, pos, diff, right;
			float ww, wh, dist, lx, ly, nx, ny, len;
			Q3IDE_WM_GetMirrorOrigin(origin, normal, &ww, &wh);
			Com_Printf("q3ide portal: origin=(%.0f,%.0f,%.0f) normal=(%.2f,%.2f,%.2f) size=%.0fx%.0f\n", origin[0],
			           origin[1], origin[2], normal[0], normal[1], normal[2], ww, wh);
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
				Com_Printf("  player=(%.0f,%.0f,%.0f) dist=%.1f lx=%.1f ly=%.1f hw=%.0f hh=%.0f\n", pos[0], pos[1],
				           pos[2], dist, lx, ly, ww * 0.5f, wh * 0.5f);
				Com_Printf("  in_bounds=%d trigger=%d cooldown=%d\n",
				           (fabsf(lx) < ww * 0.5f && fabsf(ly) < wh * 0.5f + 24.0f), (fabsf(dist) < 24.0f),
				           q3ide_state.portal_cooldown);
			}
		}
	} else if (!Q_stricmp(sub, "laser")) {
		extern qboolean q3ide_laser_active;
		Com_Printf("q3ide laser: active=%d\n", q3ide_laser_active);
	} else
		Com_Printf("q3ide: unknown sub-command '%s'\n", sub);
}

/* ============================================================
 *  Grapple-to-window teleport
 * ============================================================ */

/*
 * q3ide_grapple_window_frame — teleport player in front of a window when
 * the grapple hook attaches to its surface.
 *
 * Checks every frame if PMF_GRAPPLE_PULL is set and grapplePoint is on an
 * active window plane.  On hit, issues a setviewpos command that snaps the
 * player to the optimal viewing position facing the window.
 *
 * Viewing distance = max(world_w, world_h) * 0.9, clamped [120, 350] units.
 * The player's z is set so the window center is near eye height (viewheight ~26u).
 */
static void q3ide_grapple_window_frame(void)
{
	int i;
	q3ide_win_t *win;
	vec3_t gp, diff, right;
	float dist_to_plane, lx, ly, hw, hh, nx, ny, len, view_dist;
	float best_dist;
	int best_win;
	char cmd[160];

	if (cls.state != CA_ACTIVE)
		return;

	if (q3ide_state.grapple_tele_cooldown > 0) {
		q3ide_state.grapple_tele_cooldown--;
		return;
	}

	/* Only teleport when grapple hook is actively pulling */
	if (!(cl.snap.ps.pm_flags & PMF_GRAPPLE_PULL))
		return;

	VectorCopy(cl.snap.ps.grapplePoint, gp);

	/* Find the closest window whose surface contains grapplePoint */
	best_dist = 48.0f; /* max tolerated distance from window plane */
	best_win  = -1;

	for (i = 0; i < Q3IDE_MAX_WIN; i++) {
		win = &q3ide_wm.wins[i];
		if (!win->active || win->status == Q3IDE_WIN_STATUS_INACTIVE)
			continue;

		/* Distance from grapplePoint to window plane */
		VectorSubtract(gp, win->origin, diff);
		dist_to_plane = fabsf(DotProduct(diff, win->normal));
		if (dist_to_plane >= best_dist)
			continue;

		/* Build right vector perpendicular to normal in XY plane */
		nx  = win->normal[0];
		ny  = win->normal[1];
		len = sqrtf(nx * nx + ny * ny);
		if (len < 0.01f)
			continue;
		right[0] = -ny / len;
		right[1]  = nx / len;
		right[2]  = 0.0f;

		/* Check if grapplePoint projects inside window bounds */
		lx = DotProduct(diff, right);
		ly = diff[2];
		hw = win->world_w * 0.5f;
		hh = win->world_h * 0.5f;

		if (fabsf(lx) < hw && fabsf(ly) < hh) {
			best_dist = dist_to_plane;
			best_win  = i;
		}
	}

	if (best_win < 0)
		return;

	win = &q3ide_wm.wins[best_win];

	/* Optimal viewing distance: fit the whole window in typical ~90° FOV */
	view_dist = (win->world_w > win->world_h ? win->world_w : win->world_h) * 0.9f;
	if (view_dist < 120.0f) view_dist = 120.0f;
	if (view_dist > 350.0f) view_dist = 350.0f;

	/* Destination: stand in front of window.  Z: window center - viewheight so
	 * the window center lands at eye level. */
	float tx  = win->origin[0] + win->normal[0] * view_dist;
	float ty  = win->origin[1] + win->normal[1] * view_dist;
	float tz  = win->origin[2] - 26.0f; /* window center at eye level */
	float yaw = atan2f(-win->normal[1], -win->normal[0]) * 180.0f / (float) M_PI;

	Com_sprintf(cmd, sizeof(cmd), "setviewpos %.0f %.0f %.0f %.0f", tx, ty, tz, yaw);
	Q3IDE_LOGI("grapple tele win[%d] view_dist=%.0f cmd=[%s]", best_win, view_dist, cmd);
	CL_AddReliableCommand(cmd, qfalse);
	q3ide_state.grapple_tele_cooldown = 120; /* 2s grace at 60fps */
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

	Q3IDE_Log_Init();

	if (Q3IDE_WM_Init())
		Q3IDE_LOGI("capture ready");
	else
		Q3IDE_LOGW("running without capture");

	Q3IDE_Interaction_Init();

	Cmd_AddCommand("q3ide", Q3IDE_Cmd_f);
	q3ide_state.initialized = qtrue;
	Com_Printf("q3ide: Interaction: dwell 150ms on window → hover highlight (no movement lock)\n");
	Com_Printf("q3ide: Key bindings:\n");
	Com_Printf("  bind L \"set q3ide_lock 1\"            (lock into Pointer Mode on highlighted window)\n");
	Com_Printf("  bind <key> \"set q3ide_use_key 1\"     (enter Keyboard mode from Pointer)\n");
	Com_Printf("  bind <key> \"set q3ide_escape 1\"      (exit Pointer/Keyboard modes)\n");
}

void Q3IDE_OnVidRestart(void)
{
	Q3IDE_WM_InvalidateShaders();
	/* Re-arm autoexec so the post-map sequence fires on every level load */
	q3ide_state.autoexec_done = qfalse;
	q3ide_state.autoexec_delay = 0;
}

void Q3IDE_Frame(void)
{
	if (!q3ide_state.initialized)
		return;

	/* Fire nextdemo + auto-place mirror after map settles (~60 frames) */
	if (!q3ide_state.autoexec_done && cls.state == CA_ACTIVE) {
		if (++q3ide_state.autoexec_delay > 60) {
			/* Always give grapple hook on every map load */
			Cbuf_AddText("give grappling hook\nweapon 10\n");
			cvar_t *cmd = Cvar_Get("nextdemo", "", 0);
			if (cmd && cmd->string[0]) {
				Com_Printf("q3ide: auto: %s\n", cmd->string);
				Cbuf_AddText(cmd->string);
				Cbuf_AddText("\n");
				Cvar_Set("nextdemo", "");
			}
			q3ide_state.autoexec_done = qtrue;

			/* Place portal on the wall the player is facing — walk into the glow to teleport */
			{
				vec3_t eye, fwd_v, right_v, wall_pos, wall_normal;
				float p, y;
				VectorCopy(cl.snap.ps.origin, eye);
				eye[2] += cl.snap.ps.viewheight;
				p = cl.snap.ps.viewangles[PITCH] * (float) M_PI / 180.0f;
				y = cl.snap.ps.viewangles[YAW] * (float) M_PI / 180.0f;
				fwd_v[0] = cosf(p) * cosf(y);
				fwd_v[1] = cosf(p) * sinf(y);
				fwd_v[2] = -sinf(p);
				/* Right vector (player's right, XY-plane) */
				right_v[0] = sinf(y);
				right_v[1] = -cosf(y);
				right_v[2] = 0.0f;
				/* Trace to the wall; if no wall found, float 120u ahead */
				if (!Q3IDE_WM_TraceWall(eye, fwd_v, wall_pos, wall_normal)) {
					wall_pos[0] = eye[0] + fwd_v[0] * 120.0f;
					wall_pos[1] = eye[1] + fwd_v[1] * 120.0f;
					wall_pos[2] = eye[2];
					wall_normal[0] = -fwd_v[0];
					wall_normal[1] = -fwd_v[1];
					wall_normal[2] = 0.0f;
				}
				/* Eye height + 4u right + 48u off wall toward player */
				wall_pos[2] = eye[2];
				wall_pos[0] += right_v[0] * 4.0f + wall_normal[0] * 48.0f;
				wall_pos[1] += right_v[1] * 4.0f + wall_normal[1] * 48.0f;
				/* 72 wide x 131 tall (40cm/16u shorter) */
				Q3IDE_WM_PlaceMirror(wall_pos, wall_normal, 72.0f, 131.0f);
				q3ide_state.portal_cooldown = 90; /* 1.5s grace — don't fire at spawn */
				Q3IDE_LOGI("portal placed on wall at (%.0f,%.0f,%.0f) n=(%.2f,%.2f) size=72x131", wall_pos[0],
				           wall_pos[1], wall_pos[2], wall_normal[0], wall_normal[1]);
			}
		}
	}

	if (cls.state == CA_ACTIVE) {
		vec3_t eye;
		int buttons;
		float cur_yaw, cur_pitch, mouse_dx, mouse_dy;
		qboolean attacking, use_key, escape, lock_key;

		/* Update player position for distance-based rendering */
		VectorCopy(cl.snap.ps.origin, eye);
		eye[2] += cl.snap.ps.viewheight;
		Q3IDE_WM_UpdatePlayerPos(eye[0], eye[1], eye[2]);

		/* Auto-equip grapple on respawn (health 0→>0) */
		{
			int hp = cl.snap.ps.stats[STAT_HEALTH];
			if (hp > 0 && q3ide_state.last_health <= 0)
				Cbuf_AddText("give grappling hook\nweapon 10\n");
			q3ide_state.last_health = hp;
		}

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
		lock_key = (Cvar_VariableIntegerValue("q3ide_lock") == 1);
		if (lock_key)
			Cvar_Set("q3ide_lock", "0");

		/* Update interaction state + entity hover name */
		Q3IDE_Interaction_Frame(attacking, use_key, escape, lock_key, mouse_dx, mouse_dy);
		Q3IDE_UpdateEntityHover();

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

		q3ide_portal_frame();
		q3ide_grapple_window_frame();
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

qboolean q3ide_laser_active = qfalse;

qboolean Q3IDE_OnKeyEvent(int key, qboolean down)
{
	if (!q3ide_state.initialized)
		return qfalse;
	/* K hold = laser beams; never consumed so game still gets the key */
	if (key == 'k' || key == 'K') {
		q3ide_laser_active = down;
		Com_Printf("q3ide: laser %s (key=%d)\n", down ? "ON" : "OFF", key);
	}
	return Q3IDE_Interaction_OnKeyEvent(key, down);
}

void Q3IDE_Shutdown(void)
{
	if (!q3ide_state.initialized)
		return;
	Q3IDE_WM_Shutdown();
	Cmd_RemoveCommand("q3ide");
	q3ide_state.initialized = qfalse;
	Q3IDE_LOGI("shutdown");
	Q3IDE_Log_Shutdown();
}
