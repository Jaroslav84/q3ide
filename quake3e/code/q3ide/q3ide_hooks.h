/*
 * q3ide_hooks.h — Q3IDE engine integration.
 *
 * Bridges the Quake3e engine with the macOS ScreenCaptureKit dylib.
 * All hooks guarded by #ifdef USE_Q3IDE in engine source files.
 */

#ifndef Q3IDE_HOOKS_H
#define Q3IDE_HOOKS_H

#include "../qcommon/q_shared.h"

/* Called from CL_Init() — loads dylib, registers "q3ide" command. */
void Q3IDE_Init(void);

/* Called from CL_Frame() before SCR_UpdateScreen() — polls frames, uploads textures. */
void Q3IDE_Frame(void);

/* Called from CL_Shutdown() — detaches all captures, unloads dylib. */
void Q3IDE_Shutdown(void);

/*
 * Called from CG_R_RENDERSCENE in cl_cgame.c before re.RenderScene().
 * Adds active window quads to the scene poly list.
 */
void Q3IDE_AddPolysToScene(void);

/* Called after renderer re-init (vid_restart or first init). */
void Q3IDE_OnVidRestart(void);

/* Called from CL_KeyEvent for every key press/release.
 * Returns 1 if q3ide consumed the key (caller should return immediately). */
qboolean Q3IDE_OnKeyEvent(int key, qboolean down);

/* Returns 1 if q3ide is consuming input (Pointer or Keyboard mode).
 * Called from CL_CreateNewCommands to suppress BUTTON_ATTACK. */
qboolean Q3IDE_ConsumesInput(void);

/* Called from CL_CreateNewCommands before BUTTON_ATTACK is cleared.
 * Saves the raw button state so Q3IDE_Frame can still detect attack presses. */
void Q3IDE_SaveRawButtons(int buttons);

/*
 * Called from CG_R_RENDERSCENE in cl_cgame.c instead of re.RenderScene()
 * when r_multiMonitor is enabled. Renders one viewport per monitor.
 */
void Q3IDE_MultiMonitorRender(const void *refdef_ptr);

/*
 * Draw keybinding cheat sheet on the left monitor (called internally by
 * Q3IDE_MultiMonitorRender — not used directly by engine files).
 */
void Q3IDE_DrawLeftOverlay(const void *refdef_ptr);

/* Scan snapshot entities for the one under the crosshair; writes name into
 * q3ide_interaction.hovered_entity_name ("" when nothing found). */
void Q3IDE_UpdateEntityHover(void);

/* Draw red laser beams from player eye to all active windows (hold K). */
void Q3IDE_DrawLasers(const void *refdef_ptr);

/* Internal frame state — shared between q3ide_hooks*.c TUs. */
typedef struct {
	qboolean initialized;
	qboolean autoexec_done;
	int autoexec_delay;
	float last_yaw, last_pitch;
	int raw_buttons;
	int last_health;
	int portal_cooldown;
	int portal2_cooldown;
	int grapple_tele_cooldown;
	int stream_last_area;
	int stream_cooldown;
} q3ide_hooks_state_t;

#endif /* Q3IDE_HOOKS_H */
