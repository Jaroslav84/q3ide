/*
 * spatial/placement.h — Placement algorithms for q3ide.
 *
 * Engine-agnostic logic for determining where Windows go in the world.
 * MVP: on spawn, trace forward and place on nearest wall.
 */

#ifndef Q3IDE_SPATIAL_PLACEMENT_H
#define Q3IDE_SPATIAL_PLACEMENT_H

#include "panel.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Calculate placement for a Window on the nearest wall from spawn.
 *
 * Uses the engine adapter's trace_nearest_wall to find a wall,
 * then computes the quad position, normal, and dimensions based
 * on the captured window's aspect ratio.
 *
 * window->origin, normal, width, height are filled on success.
 * Returns 1 on success, 0 if no suitable wall was found.
 */
int q3ide_calc_wall_placement(q3ide_window_t *window);

/*
 * Calculate Window dimensions in game units from pixel dimensions.
 *
 * Maintains aspect ratio, clamped to Q3IDE_WINDOW_MIN/MAX_WIDTH.
 * Output: world_width, world_height.
 */
void q3ide_calc_window_size(int pixel_width, int pixel_height,
                            float *world_width, float *world_height);

#ifdef __cplusplus
}
#endif

#endif /* Q3IDE_SPATIAL_PLACEMENT_H */
