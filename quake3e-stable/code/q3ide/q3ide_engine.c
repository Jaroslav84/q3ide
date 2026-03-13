/*
 * q3ide_engine.c — Public engine hook implementations.
 * Per-frame logic: q3ide_frame.c.
 * Contains Q3IDE_Init, Q3IDE_Shutdown, Q3IDE_AddPolysToScene, input hooks.
 */

#include "q3ide_engine_hooks.h"
#include "q3ide_hotkey.h"
#include "q3ide_log.h"
#include "q3ide_params.h"
#include "q3ide_win_mngr.h"
#include "q3ide_win_mngr_internal.h"
#include "q3ide_view_modes.h"
#include "q3ide_aas.h"
#include "q3ide_map_skin_browser.h"
#include "../qcommon/qcommon.h"
#include "../client/client.h"
#include <math.h>

extern playerState_t *SV_GameClientNum(int num);

/* Shoot-to-place state — referenced by input.c, scene.c, console.c, frame.c */
q3ide_hooks_state_t q3ide_state;
int q3ide_selected_win = -1;
int q3ide_select_time = 0;
int q3ide_last_attack = 0;
int q3ide_aimed_win = -1; /* window under crosshair this frame, -1 = none */

extern void Q3IDE_Cmd_f(void); /* command dispatcher in q3ide_console.c */

/* Shoot-to-place — q3ide_shoot_to_place.c */
extern void q3ide_shoot_frame(void);

/* ============================================================
 *  Public engine hooks
 * ============================================================ */

void Q3IDE_Init(void)
{
	memset(&q3ide_state, 0, sizeof(q3ide_state));
	q3ide_selected_win = -1;

	Q3IDE_Log_Init();

	if (Q3IDE_WM_Init())
		Q3IDE_LOGI("capture ready");
	else
		Q3IDE_LOGW("running without capture");

	Q3IDE_ViewModes_Init();
	Q3IDE_MMenu_Init();

	Cmd_AddCommand("q3ide", Q3IDE_Cmd_f);
	q3ide_state.initialized = qtrue;
}

void Q3IDE_OnVidRestart(void)
{
	const char *mapname;
	Q3IDE_WM_InvalidateShaders();
	q3ide_state.autoexec_done = qfalse;
	q3ide_state.autoexec_delay = 0;

	/* Pre-warm scratch images so q3ide/winN shaders compile successfully.
	 * map *scratchN in the shader file fails if tr.scratchImage[N] doesn't
	 * exist yet (R_FindImageFile returns NULL → stage parse fails → shader=0).
	 * Upload a 1×1 black pixel to each slot now, before any lazy shader
	 * compilation can occur. */
	if (re.UploadCinematic) {
		static byte q3ide_black[4] = {0, 0, 0, 255};
		int i;
		for (i = 0; i < Q3IDE_MAX_WIN; i++)
			re.UploadCinematic(1, 1, 1, 1, q3ide_black, i, qtrue, 0x80E1 /* GL_BGRA */);
	}

	mapname = Cvar_VariableString("mapname");
	if (mapname && mapname[0])
		Q3IDE_AAS_Load(mapname);
}

/* Q3IDE_Frame — q3ide_hooks_frame_main.c */
extern void Q3IDE_Frame(void);

void Q3IDE_AddPolysToScene(void)
{
	if (!q3ide_state.initialized)
		return;
	Q3IDE_WM_AddPolys();
}

qboolean Q3IDE_ConsumesInput(void)
{
	return Q3IDE_MMenu_IsOpen();
}

/* Key event handling — q3ide_key_events.c */
extern qboolean Q3IDE_OnKeyEvent(int key, qboolean down);

static void q3ide_render_bye(void)
{
	static const float white[4] = {1.0f, 1.0f, 1.0f, 1.0f};
	int cx, cy;

	if (!re.BeginFrame || !re.EndFrame || !re.SetColor)
		return;

	re.BeginFrame(STEREO_CENTER);
	re.SetColor(NULL);

	cx = cls.glconfig.vidWidth / 2 - 4 * BIGCHAR_WIDTH;
	cy = cls.glconfig.vidHeight / 2 - BIGCHAR_HEIGHT / 2;
	SCR_DrawBigString(cx, cy, "bye", 1.0f, qfalse);

	re.SetColor(white);
	re.EndFrame(NULL, NULL);

	Sys_Sleep(800);
}

void Q3IDE_Shutdown(void)
{
	if (!q3ide_state.initialized)
		return;
	q3ide_render_bye();
	Q3IDE_ViewModes_Shutdown();
	Q3IDE_WM_Shutdown();
	Cmd_RemoveCommand("q3ide");
	q3ide_state.initialized = qfalse;
	Q3IDE_LOGI("shutdown");
	Q3IDE_Log_Shutdown();
}
