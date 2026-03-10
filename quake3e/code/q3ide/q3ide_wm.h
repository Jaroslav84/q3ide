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
/* skip_clamp=qtrue: layout engine already measured fit; skip q3ide_clamp_window_size */
qboolean Q3IDE_WM_Attach(unsigned int id, vec3_t origin, vec3_t normal, float ww, float wh, qboolean do_start,
                         qboolean skip_clamp);

/*
 * Ray-test all active windows; returns index of closest hit or -1.
 * skip_idx: exclude this index (-1 = none), for cycling through stacked windows.
 */
int Q3IDE_WM_TraceWindowHit(vec3_t start, vec3_t dir, int skip_idx);

/* Move an active window to a new position (shoot-to-move).
 * skip_clamp=qtrue: skip q3ide_clamp_window_size (use when size is already correct). */
void Q3IDE_WM_MoveWindow(int idx, vec3_t origin, vec3_t normal, qboolean skip_clamp);

/* Find slot index of a window by capture id; returns -1 if not found. */
int Q3IDE_WM_FindById(unsigned int cid);

/* Simple commands (in q3ide_wm.c) */
void Q3IDE_WM_CmdList(void);
void Q3IDE_WM_CmdDetachAll(void);
void Q3IDE_WM_CmdStatus(void);

/* Detach exactly one window by capture id; prints result. (in q3ide_cmd.c) */
qboolean Q3IDE_WM_DetachById(unsigned int capture_id);

/* Complex commands (in q3ide_cmd.c) */
void Q3IDE_WM_CmdAttach(void);
void Q3IDE_WM_CmdDesktop(void);

/* Poll for new/closed macOS windows and auto-attach/detach. (in q3ide_cmd.c) */
void Q3IDE_WM_PollChanges(void);
/* Drain change list fetched by background poll thread — call from main thread. */
void Q3IDE_WM_DrainPendingChanges(void);

/* Called from Q3IDE_Frame with current player eye position */
void Q3IDE_WM_UpdatePlayerPos(float px, float py, float pz);

/* Update hover state for a specific window (called from interaction system) */
void Q3IDE_WM_SetHover(int idx, float hover_t);

/* Set the display label for a window (called after attach). */
void Q3IDE_WM_SetLabel(unsigned int capture_id, const char *label);

#endif /* Q3IDE_WM_H */
