/*
 * q3ide_interaction.h — Interaction model for Q3IDE (Batch 2).
 *
 * Implements the three-state interaction model:
 *   FPS Mode → Pointer Mode → Keyboard Passthrough
 *
 * Dwell detection (150ms) triggers Hover Effect.
 * Click in Pointer Mode routes to the captured app.
 * Escape always returns to FPS Mode.
 */
#ifndef Q3IDE_INTERACTION_H
#define Q3IDE_INTERACTION_H

#include "../qcommon/q_shared.h"

/* Interaction modes */
typedef enum {
	Q3IDE_MODE_FPS = 0,  /* Normal FPS gameplay */
	Q3IDE_MODE_POINTER,  /* Mouse controls Window cursor */
	Q3IDE_MODE_KEYBOARD, /* All keys route to captured app */
} q3ide_interaction_mode_t;

/* Per-frame state (updated by Q3IDE_Interaction_Frame) */
typedef struct {
	q3ide_interaction_mode_t mode;
	int focused_win;         /* wins[] index under crosshair, -1=none */
	float focused_uv[2];     /* UV coords on focused window */
	float focused_dist;      /* distance to focused window */
	vec3_t focused_hit_pos;  /* 3D world position of crosshair hit on window */
	float dwell_start_ms;    /* when dwell began (-1 = not dwelling) */
	float hover_t;           /* 0..1 eased hover progress */
	float pointer_uv[2];     /* current pointer position in UV [0..1] */
	qboolean prev_attacking; /* last frame's attack state (rising-edge detection) */
	sfxHandle_t pain_sfx[3]; /* Sarge pain sounds for window hits */
} q3ide_interaction_state_t;

#define Q3IDE_DWELL_MS 150.0f          /* ms to activate hover */
#define Q3IDE_POINTER_MAX_DIST 1000.0f /* max distance for Pointer Mode (~10m) */
#define Q3IDE_EDGE_ZONE_UV 0.05f       /* edge zone in UV fraction (~20px at typical size) */

/* Initialise interaction state */
void Q3IDE_Interaction_Init(void);

/* Called each frame from Q3IDE_Frame.
 * attacking:   1 if BUTTON_ATTACK pressed this frame (leading edge).
 * use_key:     1 if USE key pressed (Enter → Keyboard mode).
 * escape:      1 if Escape pressed.
 * mouse_dx/dy: raw mouse delta this frame (used in Pointer Mode).
 *
 * Pointer Mode is entered automatically after dwelling on a window for
 * Q3IDE_DWELL_MS ms while within Q3IDE_POINTER_MAX_DIST units.
 */
void Q3IDE_Interaction_Frame(qboolean attacking, qboolean use_key, qboolean escape, float mouse_dx, float mouse_dy);

/* Get current mode */
q3ide_interaction_mode_t Q3IDE_Interaction_GetMode(void);

/* Returns 1 if in Pointer or Keyboard mode (input consumed by q3ide) */
qboolean Q3IDE_Interaction_ConsumesInput(void);

/* Called from CL_KeyEvent when a key goes up/down.
 * Returns qtrue if q3ide consumed the key (caller should not process it). */
qboolean Q3IDE_Interaction_OnKeyEvent(int key, qboolean down);

extern q3ide_interaction_state_t q3ide_interaction;

#endif /* Q3IDE_INTERACTION_H */
