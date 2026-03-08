/*
 * spatial/window/manager.h — Window entity lifecycle management for q3ide.
 *
 * Manages the global window array: creation, destruction, lookup, and updates.
 * All functions are engine-agnostic and thread-safe (caller must synchronize).
 */

#ifndef Q3IDE_SPATIAL_WINDOW_MANAGER_H
#define Q3IDE_SPATIAL_WINDOW_MANAGER_H

#include "entity.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ─── Initialization ──────────────────────────────────────────────────────── */

/*
 * Clear all window slots and reset the manager state.
 */
void win_manager_init(void);

/* ─── Creation & Destruction ──────────────────────────────────────────────── */

/*
 * Create a new window entity.
 *
 * Args:
 *   capture_id: macOS window ID from capture system
 *   origin:     world-space position [3]
 *   normal:     surface normal [3], will be normalized
 *   ww, wh:     world-space dimensions
 *
 * Returns: slot index [0..Q3IDE_MAX_WINDOWS-1] on success, -1 if full.
 */
int win_manager_create(unsigned int capture_id, float origin[3], float normal[3],
                       float ww, float wh);

/*
 * Destroy window at index: mark inactive and clear all fields.
 * Safe to call on already-inactive slots.
 */
void win_manager_destroy(int idx);

/*
 * Find and destroy window by capture_id.
 * Returns 1 if found and destroyed, 0 if not found.
 */
int win_manager_destroy_by_id(unsigned int capture_id);

/* ─── Query ───────────────────────────────────────────────────────────────── */

/*
 * Find window index by capture_id.
 * Returns [0..Q3IDE_MAX_WINDOWS-1] on hit, -1 if not found.
 */
int win_manager_find(unsigned int capture_id);

/*
 * Get window pointer by index.
 * Returns NULL if index out of range or slot inactive.
 */
window_entity_t *win_manager_get(int idx);

/*
 * Get the entire window array.
 * Use win_manager_count() to learn how many are active.
 */
window_entity_t *win_manager_all(void);

/*
 * Count of active windows (status != WIN_STATUS_INACTIVE).
 */
int win_manager_count(void);

/* ─── Status & Distance Updates ───────────────────────────────────────────── */

/*
 * Update the status field of a window.
 */
void win_manager_update_status(int idx, win_status_t status);

/*
 * Update player distance and compute fps_throttle and skip_counter.
 * Call once per frame for each active window.
 *
 * Args:
 *   idx:  window index
 *   dist: distance from player to window (units), or -1 if unknown/off-screen
 *
 * Sets:
 *   window->player_dist = dist
 *   window->fps_throttle = computed FPS from distance
 *   window->skip_counter = 0 (reset on each update)
 */
void win_manager_update_player_dist(int idx, float dist);

/* ─── FPS Throttle Computation ────────────────────────────────────────────── */

/*
 * Compute target FPS based on distance from player to window.
 *
 * Rules:
 *   dist < 200:    60 fps (full capture rate)
 *   dist < 500:    30 fps
 *   dist < 1000:   15 fps
 *   dist >= 1000:  5 fps (nearly stopped)
 *   dist = -1:     5 fps (off-screen or unknown)
 *
 * Returns target FPS value.
 */
int win_manager_get_target_fps(float dist);

#ifdef __cplusplus
}
#endif

#endif /* Q3IDE_SPATIAL_WINDOW_MANAGER_H */
