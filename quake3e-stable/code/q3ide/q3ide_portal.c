/*
 * q3ide_portal.c — Portal teleport state and per-frame logic.
 * Engine lifecycle hooks: q3ide_engine.c.  Per-frame tick: q3ide_frame.c.
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
#include "q3ide_aas.h"
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

/* Shoot-to-place state — exported for q3ide_hooks_input.c */
int q3ide_selected_win = -1; /* wins[] index, -1 = none selected */
int q3ide_select_time = 0;   /* Sys_Milliseconds() when selected */
int q3ide_last_attack = 0;   /* previous frame BUTTON_ATTACK state */

q3ide_hooks_state_t q3ide_state;

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
/*
 * Two-portal straight-line loop: both portals face north (normal=(0,1,0)).
 * Player faces south (yaw=270) and runs through both in sequence:
 *   ... → portal_A → portal_B → portal_A → portal_B → ...
 * Separate cooldown per portal so each can fire independently.
 *
 * Layout (Y axis, player runs south = decreasing Y):
 *   spawn (-974) → portal_A (-1100) → arrive (-1200) → portal_B (-1280) → arrive (-900) → repeat
 */
static void q3ide_portal_tele(playerState_t *sps, float x, float y, float z, float yaw, const char *tag)
{
	Q3IDE_LOGI("%s -> (%.0f,%.0f,%.0f) yaw=%.0f", tag, x, y, z, yaw);
	sps->origin[0] = x;
	sps->origin[1] = y;
	sps->origin[2] = z;
	sps->velocity[0] = sps->velocity[1] = sps->velocity[2] = 0.0f;
	sps->eFlags ^= EF_TELEPORT_BIT;
	sps->viewangles[YAW] = yaw;
	sps->viewangles[PITCH] = 0.0f;
	VectorCopy(sps->origin, cl.snap.ps.origin);
	VectorClear(cl.snap.ps.velocity);
	cl.snap.ps.eFlags = sps->eFlags;
}

/* Portal frame logic removed — mirror portal gone. Reserved for future use. */
__attribute__((unused)) static void q3ide_portal_frame(void)
{
}

/* Console command dispatcher — q3ide_hooks_cmd.c */
extern void Q3IDE_Cmd_f(void);

/* Grapple + public engine hooks — defined in q3ide_hooks_grapple.c / q3ide_hooks_frame.c */
