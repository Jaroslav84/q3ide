/*
 * q3ide_wall_cache.h — Wall pre-scan cache for the placement system.
 *
 * Built once per area transition from AAS wall data.
 * Provides pre-computed window slots for the placement queue.
 */
#pragma once
#include "q3ide_params.h"
#include "../qcommon/q_shared.h"

typedef struct {
	vec3_t position; /* slot center in world space, offset Q3IDE_WALL_OFFSET from surface */
	vec3_t normal;   /* facing direction (points into room) */
	float width;     /* window width allocated to this slot */
	float height;    /* wall floor-to-ceiling height */
	int window_idx;  /* -1 = free, else q3ide_wm.wins[] index */
} q3ide_wall_slot_t;

typedef struct {
	vec3_t center;        /* wall centroid (for distance sort) */
	vec3_t normal;        /* wall facing direction */
	float width;          /* total wall width */
	float height;         /* floor-to-ceiling */
	float dist_to_player; /* cached at build time */
	int slot_count;       /* total slots pre-computed */
	int slots_used;       /* occupied slots */
	q3ide_wall_slot_t slots[Q3IDE_MAX_WALL_SLOTS];
} q3ide_cached_wall_t;

typedef struct {
	q3ide_cached_wall_t walls[Q3IDE_MAX_CACHED_WALLS];
	int wall_count;
	int area_id;
	qboolean valid;
} q3ide_wall_cache_t;

extern q3ide_wall_cache_t g_wall_cache;

/* Build cache from AAS walls in player's current area. */
void Q3IDE_WallCache_Build(const vec3_t eye, int area_id);

/* Invalidate cache (called before rebuild on area transition). */
void Q3IDE_WallCache_Invalidate(void);

/* Console dump: walls + slot positions + occupancy. */
void Q3IDE_WallCache_Dump(void);
