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
#include "q3ide_params.h"

/* Interaction modes */
typedef enum {
	Q3IDE_MODE_FPS = 0,  /* Normal FPS gameplay */
	Q3IDE_MODE_POINTER,  /* Mouse controls Window cursor */
	Q3IDE_MODE_KEYBOARD, /* All keys route to captured app */
} q3ide_interaction_mode_t;

/* Per-frame state (updated by Q3IDE_Interaction_Frame) */
typedef struct {
	q3ide_interaction_mode_t mode;
	int focused_win;              /* wins[] index under crosshair, -1=none */
	float focused_uv[2];          /* UV coords on focused window */
	float focused_dist;           /* distance to focused window */
	vec3_t focused_hit_pos;       /* 3D world position of hit */
	float dwell_start_ms;         /* when dwell began (-1 = not dwelling) */
	float hover_t;                /* 0..1 eased hover progress */
	float pointer_uv[2];          /* current pointer position in UV [0..1] */
	sfxHandle_t pain_sfx[3];      /* Sarge pain sounds for window hits */
	char hovered_entity_name[64]; /* name of game entity under crosshair, "" = none */
} q3ide_interaction_state_t;

/* Initialise interaction state */
void Q3IDE_Interaction_Init(void);

/* Called each frame from Q3IDE_Frame.
 * attacking:   1 if BUTTON_ATTACK pressed this frame (leading edge).
 * use_key:     1 if USE key pressed (Enter → Keyboard mode from Pointer).
 * escape:      1 if Escape pressed.
 * lock_key:    1 if L key pressed — enters Pointer Mode when crosshair is on a highlighted window.
 * mouse_dx/dy: raw mouse delta this frame (used in Pointer Mode).
 *
 * Pointer Mode is entered by pressing lock_key while crosshair is on a window.
 * Dwell (Q3IDE_DWELL_MS) only shows the hover highlight — does NOT block movement.
 */
void Q3IDE_Interaction_Frame(qboolean attacking, qboolean use_key, qboolean escape, qboolean lock_key, float mouse_dx,
                             float mouse_dy);

/* Get current mode */
q3ide_interaction_mode_t Q3IDE_Interaction_GetMode(void);

/* Returns 1 if in Pointer or Keyboard mode (input consumed by q3ide) */
qboolean Q3IDE_Interaction_ConsumesInput(void);

/* Called from CL_KeyEvent when a key goes up/down.
 * Returns qtrue if q3ide consumed the key (caller should not process it). */
qboolean Q3IDE_Interaction_OnKeyEvent(int key, qboolean down);

extern q3ide_interaction_state_t q3ide_interaction;

#endif /* Q3IDE_INTERACTION_H */
