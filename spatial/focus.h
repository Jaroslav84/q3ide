/*
 * spatial/focus.h — Crosshair focus system for q3ide.
 *
 * The crosshair is the eye-tracking equivalent.
 * When the player aims at an interactive element, it receives
 * Hover Effect (brightness increase + z-lift).
 *
 * Post-MVP: not used in MVP, but the interface is defined
 * for forward compatibility.
 */

#ifndef Q3IDE_SPATIAL_FOCUS_H
#define Q3IDE_SPATIAL_FOCUS_H

#include "panel.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Focus target — what the crosshair is currently aimed at */
typedef struct {
	int                focused;        /* 1 if something is under crosshair */
	int                window_index;   /* Index into window array, or -1 */
	float              hit_point[3];   /* World-space point on the Window */
	float              uv[2];          /* UV coordinates on the Window texture */
	float              distance;       /* Distance from player eye to hit */
} q3ide_focus_t;

/*
 * Update the focus state based on current view.
 *
 * Traces from view origin along view direction to find
 * if the crosshair intersects any q3ide Window surfaces.
 *
 * Post-MVP: triggers Hover Effect on the focused Window.
 */
void q3ide_focus_update(q3ide_focus_t *focus,
                        const q3ide_window_t *windows, int num_windows,
                        const float *view_origin, const float *view_angles);

#ifdef __cplusplus
}
#endif

#endif /* Q3IDE_SPATIAL_FOCUS_H */
