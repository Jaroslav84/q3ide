/*
 * spatial/window/entity.h — Window entity data model for q3ide.
 *
 * Engine-agnostic representation of a captured Desktop Window in the game world.
 * Uses VisionOS terminology and plain C types (no Quake dependencies).
 *
 * This is the canonical data model for all window-related operations.
 * The spatial layer NEVER calls engine internals — it goes through
 * the engine adapter (engine/adapter.h).
 */

#ifndef Q3IDE_SPATIAL_WINDOW_ENTITY_H
#define Q3IDE_SPATIAL_WINDOW_ENTITY_H

#ifdef __cplusplus
extern "C" {
#endif

/* ─── Window Status States ────────────────────────────────────────────────── */

typedef enum {
	WIN_STATUS_INACTIVE = 0,    /* Not yet spawned or destroyed */
	WIN_STATUS_SPAWNING,        /* Scale-in animation in progress */
	WIN_STATUS_ACTIVE,          /* Live, updating textures */
	WIN_STATUS_IDLE,            /* Captured but not updating (off-screen) */
	WIN_STATUS_ERROR,           /* Capture failed or stalled */
} win_status_t;

/* ─── Window Style ────────────────────────────────────────────────────────── */

typedef enum {
	WIN_STYLE_ANCHORED = 0,     /* Wall-attached (MVP) */
	WIN_STYLE_FLOATING,         /* Free-floating in space (Post-MVP) */
} win_style_t;

/* ─── Window Entity ───────────────────────────────────────────────────────── */

typedef struct {
	/* ─── Identity ─── */
	int                      active;             /* 1 = in-use slot, 0 = inactive */
	unsigned int             capture_id;         /* macOS window ID */
	char                     title[256];         /* Window title from OS */
	char                     app_name[128];      /* Application name */

	/* ─── Status & Style ─── */
	win_status_t             status;             /* Current state */
	win_style_t              style;              /* Anchored or floating */

	/* ─── Texture ─── */
	int                      scratch_slot;       /* GPU texture slot [0..15] */
	int                      tex_w, tex_h;       /* Texture dimensions in pixels */

	/* ─── Placement ─── */
	float                    origin[3];          /* World-space position */
	float                    normal[3];          /* Surface normal (facing player) */
	float                    world_w, world_h;   /* World-space dimensions */

	/* ─── Frame Stats ─── */
	unsigned long long       frames;             /* Total frames received */
	unsigned long long       first_frame_time_ms; /* Timestamp of first frame (ms) */
	unsigned long long       last_frame_time_ms;  /* Timestamp of last frame (ms) */

	/* ─── Distance & Throttle ─── */
	float                    player_dist;        /* Distance from player to window */
	int                      fps_throttle;       /* Target FPS based on distance */
	int                      skip_counter;       /* Frames skipped for FPS throttle */

	/* ─── Batch 2: Interaction ─── */
	int                      hover_active;       /* 1 = crosshair hovering on this window */
	float                    hover_t;            /* [0..1] hover animation progress */
	float                    dwell_start_ms;     /* -1 = no dwell, else Sys_Milliseconds */

	/* ─── Future Batches (stubbed) ─── */
	int                      space_id;           /* Batch 7: which Space this belongs to */
	int                      visibility;         /* Batch 14: 0=private, 1=public, 2=team */
	char                     agent_id[64];       /* Batch 12: agent owner, empty=none */
} window_entity_t;

/* ─── Limits ──────────────────────────────────────────────────────────────── */

#define Q3IDE_MAX_WINDOWS  16

#ifdef __cplusplus
}
#endif

#endif /* Q3IDE_SPATIAL_WINDOW_ENTITY_H */
