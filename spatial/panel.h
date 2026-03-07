/*
 * spatial/panel.h — Window data structures for q3ide.
 *
 * Engine-agnostic representation of a captured Window in the game world.
 * Uses VisionOS terminology: this is a "Window", not a "panel".
 *
 * The spatial layer NEVER calls engine internals — it goes through
 * the engine adapter (engine/adapter.h).
 */

#ifndef Q3IDE_SPATIAL_PANEL_H
#define Q3IDE_SPATIAL_PANEL_H

#ifdef __cplusplus
extern "C" {
#endif

/* ─── Window state ────────────────────────────────────────────────────── */

typedef enum {
	Q3IDE_WINDOW_STATE_INACTIVE = 0,
	Q3IDE_WINDOW_STATE_SPAWNING,     /* Scale-in animation (Post-MVP) */
	Q3IDE_WINDOW_STATE_ACTIVE,       /* Live, updating texture */
	Q3IDE_WINDOW_STATE_CLOSING,      /* Fade-out animation (Post-MVP) */
} q3ide_window_state_t;

/* ─── Window placement type ───────────────────────────────────────────── */

typedef enum {
	Q3IDE_PLACEMENT_WALL = 0,   /* [MVP] Attached to a wall surface */
	Q3IDE_PLACEMENT_FLOATING,   /* Post-MVP: floating in space */
} q3ide_placement_type_t;

/* ─── Window (a captured desktop window in the game world) ────────────── */

typedef struct {
	int                      active;

	/* Capture state */
	unsigned int             capture_window_id;  /* macOS window ID */
	char                     title[256];
	char                     app_name[128];

	/* Texture */
	int                      tex_id;       /* Engine texture handle */
	int                      tex_width;
	int                      tex_height;

	/* Placement */
	q3ide_placement_type_t   placement;
	int                      surface_id;   /* Engine surface handle */
	float                    origin[3];    /* World position */
	float                    normal[3];    /* Surface normal */
	float                    width;        /* World-space width */
	float                    height;       /* World-space height */

	/* State */
	q3ide_window_state_t     state;
	float                    state_time;   /* Time in current state */

	/* Stats */
	unsigned long long       frames_received;
	unsigned long long       last_frame_timestamp_ns;
} q3ide_window_t;

/* ─── Maximum concurrent Windows ──────────────────────────────────────── */

#define Q3IDE_MAX_WINDOWS  16  /* MVP only uses 1 */

#ifdef __cplusplus
}
#endif

#endif /* Q3IDE_SPATIAL_PANEL_H */
