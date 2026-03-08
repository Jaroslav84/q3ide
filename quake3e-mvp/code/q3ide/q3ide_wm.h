/*
 * q3ide_wm.h — Q3IDE window manager internal API.
 *
 * Private header shared between q3ide_wm.c and q3ide_hooks.c.
 * Not included by engine files.
 */

#ifndef Q3IDE_WM_H
#define Q3IDE_WM_H

#include "../qcommon/q_shared.h"

/* Initialise dylib and window state. Returns qtrue on success. */
qboolean Q3IDE_WM_Init(void);

/* Detach all windows and unload dylib. */
void Q3IDE_WM_Shutdown(void);

/* Poll ring buffers and upload new frames to GPU scratch slots. */
void Q3IDE_WM_PollFrames(void);

/* Invalidate shader handles after vid_restart. */
void Q3IDE_WM_InvalidateShaders(void);

/* Add active window quads to scene (call before RenderScene). */
void Q3IDE_WM_AddPolys(void);

/* Primitives used by command handlers in q3ide_hooks.c */
qboolean Q3IDE_WM_TraceWall(vec3_t start, vec3_t dir, vec3_t out_pos, vec3_t out_normal);
qboolean Q3IDE_WM_Attach(unsigned int id, vec3_t origin, vec3_t normal, float ww, float wh,
                         qboolean do_start);

/*
 * Ray-test all active windows; returns index of closest hit or -1.
 * Used for shoot-to-select.
 */
int Q3IDE_WM_TraceWindowHit(vec3_t start, vec3_t dir);

/* Move an active window to a new position (shoot-to-move). */
void Q3IDE_WM_MoveWindow(int idx, vec3_t origin, vec3_t normal);

/* Simple commands (in q3ide_wm.c) */
void Q3IDE_WM_CmdList(void);
void Q3IDE_WM_CmdDetachAll(void);
void Q3IDE_WM_CmdStatus(void);

/* Complex commands (in q3ide_cmd.c) */
void Q3IDE_WM_CmdAttach(void);
void Q3IDE_WM_CmdDesktop(void);

#endif /* Q3IDE_WM_H */
