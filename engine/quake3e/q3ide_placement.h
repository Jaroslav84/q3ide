/*
 * q3ide_placement.h — Wall tracing and quad placement for Quake3e.
 *
 * On spawn: cast ray forward, find nearest wall, place textured quad.
 */

#ifndef Q3IDE_PLACEMENT_H
#define Q3IDE_PLACEMENT_H

/* Initialize placement subsystem. */
void q3ide_place_init(void);

/* Shut down placement subsystem. */
void q3ide_place_shutdown(void);

/* Create a surface quad at a world position, facing along normal. */
int q3ide_place_surface_create(float x, float y, float z,
                               float width, float height,
                               const float *normal);

/* Set the texture on a surface. */
void q3ide_place_surface_set_texture(int surface_id, int tex_id);

/* Set the transform matrix for a surface. */
void q3ide_place_surface_set_transform(int surface_id,
                                       const float *matrix4x4);

/* Destroy a surface. */
void q3ide_place_surface_destroy(int surface_id);

/* Trace from origin in direction to find nearest wall.
 * Returns 1 on hit (fills hit_pos and hit_normal), 0 on miss. */
int q3ide_place_trace_wall(const float *origin, const float *direction,
                           float *hit_pos, float *hit_normal);

#endif /* Q3IDE_PLACEMENT_H */
