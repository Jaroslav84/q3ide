/*
 * spatial/placement.c — Placement algorithm implementations.
 *
 * Engine-agnostic. Uses engine adapter for world queries.
 */

#include "placement.h"
#include "../engine/adapter.h"
#include "../q3ide/q3ide_params.h"

#include <math.h>

/* ─── Window size calculation ─────────────────────────────────────────── */

void q3ide_calc_window_size(int pixel_width, int pixel_height,
                            float *world_width, float *world_height)
{
	float aspect;
	float w;

	if (pixel_width <= 0 || pixel_height <= 0) {
		*world_width = Q3IDE_WINDOW_DEFAULT_WIDTH;
		*world_height = Q3IDE_WINDOW_DEFAULT_HEIGHT;
		return;
	}

	aspect = (float)pixel_width / (float)pixel_height;
	w = Q3IDE_WINDOW_DEFAULT_WIDTH;

	/* Clamp */
	if (w < Q3IDE_WINDOW_MIN_WIDTH) w = Q3IDE_WINDOW_MIN_WIDTH;
	if (w > Q3IDE_WINDOW_MAX_WIDTH) w = Q3IDE_WINDOW_MAX_WIDTH;

	*world_width = w;
	*world_height = w / aspect;
}

/* ─── Wall placement ──────────────────────────────────────────────────── */

int q3ide_calc_wall_placement(q3ide_window_t *window)
{
	float origin[3];
	float angles[3];
	float direction[3];
	float hit_pos[3];
	float hit_normal[3];
	float deg_to_rad;

	if (!q3ide_adapter) {
		return 0;
	}

	/* Get spawn position and view direction */
	q3ide_adapter->get_spawn_origin(origin);
	q3ide_adapter->get_view_angles(angles);

	/* Convert yaw to direction vector (pitch=0 for wall trace) */
	deg_to_rad = 3.14159265358979323846f / 180.0f;
	direction[0] = cosf(angles[1] * deg_to_rad);
	direction[1] = sinf(angles[1] * deg_to_rad);
	direction[2] = 0.0f;  /* Horizontal trace for wall */

	/* Trace to find wall */
	if (!Q3IDE_TRACE_WALL(origin, direction, hit_pos, hit_normal)) {
		return 0;
	}

	/* Offset the quad slightly from the wall surface */
	window->origin[0] = hit_pos[0] + hit_normal[0] * Q3IDE_WALL_OFFSET;
	window->origin[1] = hit_pos[1] + hit_normal[1] * Q3IDE_WALL_OFFSET;
	window->origin[2] = hit_pos[2] + hit_normal[2] * Q3IDE_WALL_OFFSET;

	window->normal[0] = hit_normal[0];
	window->normal[1] = hit_normal[1];
	window->normal[2] = hit_normal[2];

	/* Calculate size based on captured window dimensions */
	q3ide_calc_window_size(window->tex_width, window->tex_height,
	                       &window->width, &window->height);

	return 1;
}
