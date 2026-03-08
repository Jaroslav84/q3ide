/*
 * q3ide_hooks.h — Q3IDE engine integration.
 *
 * Bridges the Quake3e engine with the macOS ScreenCaptureKit dylib.
 * All hooks guarded by #ifdef USE_Q3IDE in engine source files.
 */

#ifndef Q3IDE_HOOKS_H
#define Q3IDE_HOOKS_H

/* Called from CL_Init() — loads dylib, registers "q3ide" command. */
void Q3IDE_Init( void );

/* Called from CL_Frame() before SCR_UpdateScreen() — polls frames, uploads textures. */
void Q3IDE_Frame( void );

/* Called from CL_Shutdown() — detaches all captures, unloads dylib. */
void Q3IDE_Shutdown( void );

/*
 * Called from CG_R_RENDERSCENE in cl_cgame.c before re.RenderScene().
 * Adds active window quads to the scene poly list.
 */
void Q3IDE_AddPolysToScene( void );

/* Called after renderer re-init (vid_restart or first init). */
void Q3IDE_OnVidRestart( void );

/*
 * Called from CG_R_RENDERSCENE in cl_cgame.c instead of re.RenderScene()
 * when r_multiMonitor is enabled. Renders one viewport per monitor.
 */
void Q3IDE_MultiMonitorRender( const void *refdef_ptr );

#endif /* Q3IDE_HOOKS_H */
