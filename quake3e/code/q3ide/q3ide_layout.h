/*
 * q3ide_layout.h — Room layout engine: wall measurement + TV-proportion placement.
 */

#ifndef Q3IDE_LAYOUT_H
#define Q3IDE_LAYOUT_H
#include "../qcommon/q_shared.h"

#define Q3IDE_LAYOUT_MAX_WALLS 12
#define Q3IDE_LAYOUT_SCAN_DIRS 24 /* every 15° — used by raycast fallback */

/* Measured wall descriptor */
typedef struct {
	vec3_t contact;  /* origin on wall surface (2u off wall, at ray-hit point) */
	vec3_t normal;   /* wall normal, pointing into room */
	vec3_t right;    /* horizontal basis (perp to normal, horizontal) */
	float width;     /* usable wall width = right_dist + left_dist, capped 64–2048 */
	float left_dist; /* measured span to the LEFT of contact (in -right direction) */
	float floor_z;   /* world Z of floor at this wall */
	float ceil_z;    /* world Z of ceiling at this wall */
} q3ide_wall_t;

/* Room — set of unique walls around player */
typedef struct {
	q3ide_wall_t walls[Q3IDE_LAYOUT_MAX_WALLS];
	int n;
} q3ide_room_t;

/* Scan room walls from eye position */
void q3ide_room_scan(vec3_t eye, q3ide_room_t *out);

/* Compute layout and enqueue placements — does NOT call Attach yet.
 * Returns number of windows queued. */
int q3ide_room_layout(const q3ide_room_t *room, unsigned int *ids, float *aspects, int *is_display, int n);

/* Flush all pending placements (call before re-layout to discard stale positions). */
void q3ide_layout_queue_reset(void);

/* Process one pending placement per call. Call from Q3IDE_Frame.
 * Returns qtrue if a window was placed, qfalse if queue empty. */
qboolean q3ide_layout_tick(void);

#endif /* Q3IDE_LAYOUT_H */
