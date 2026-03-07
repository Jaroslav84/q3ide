/*
 * q3ide_placement.c — Wall tracing and quad placement for Quake3e.
 *
 * Uses Quake3e's BSP trace system to find walls and places textured
 * quads in the game world for displaying captured windows.
 *
 * Build: compiled as part of the Quake3e client binary.
 */

#include "q3ide_placement.h"
#include <string.h>
#include <math.h>

/* ─── Constants ───────────────────────────────────────────────────────── */

#define Q3IDE_MAX_SURFACES   16
#define Q3IDE_TRACE_DIST     4096.0f  /* Max trace distance in game units */

/* ─── Surface slot ────────────────────────────────────────────────────── */

typedef struct {
	int     active;
	float   origin[3];     /* Center of the quad */
	float   normal[3];     /* Wall normal (facing player) */
	float   width;
	float   height;
	int     tex_id;        /* Bound texture ID */
	float   transform[16]; /* 4x4 column-major transform */
} q3ide_surface_slot_t;

static q3ide_surface_slot_t q3ide_surfaces[Q3IDE_MAX_SURFACES];
static int q3ide_place_initialized = 0;

/* ─── Helpers ─────────────────────────────────────────────────────────── */

static void vec3_copy(const float *src, float *dst)
{
	dst[0] = src[0];
	dst[1] = src[1];
	dst[2] = src[2];
}

static void vec3_ma(const float *origin, float scale,
                    const float *dir, float *out)
{
	out[0] = origin[0] + scale * dir[0];
	out[1] = origin[1] + scale * dir[1];
	out[2] = origin[2] + scale * dir[2];
}

static void mat4_identity(float *m)
{
	memset(m, 0, 16 * sizeof(float));
	m[0] = m[5] = m[10] = m[15] = 1.0f;
}

/* Build a transform matrix from position + normal (facing direction) */
static void mat4_from_placement(float *m,
                                const float *origin,
                                const float *normal,
                                float width, float height)
{
	float right[3], up[3];

	/* Generate a coordinate frame from the normal */
	/* Simple approach: cross with world up to get right, then cross again for up */
	float world_up[3] = { 0.0f, 0.0f, 1.0f };

	/* Handle case where normal is nearly parallel to world up */
	if (fabsf(normal[2]) > 0.99f) {
		world_up[0] = 1.0f;
		world_up[1] = 0.0f;
		world_up[2] = 0.0f;
	}

	/* right = cross(world_up, normal) */
	right[0] = world_up[1] * normal[2] - world_up[2] * normal[1];
	right[1] = world_up[2] * normal[0] - world_up[0] * normal[2];
	right[2] = world_up[0] * normal[1] - world_up[1] * normal[0];

	/* Normalize right */
	float len = sqrtf(right[0]*right[0] + right[1]*right[1] + right[2]*right[2]);
	if (len > 0.001f) {
		right[0] /= len;
		right[1] /= len;
		right[2] /= len;
	}

	/* up = cross(normal, right) */
	up[0] = normal[1] * right[2] - normal[2] * right[1];
	up[1] = normal[2] * right[0] - normal[0] * right[2];
	up[2] = normal[0] * right[1] - normal[1] * right[0];

	/* Build column-major 4x4 matrix */
	/* Column 0: right (scaled by width) */
	m[0]  = right[0] * width;
	m[1]  = right[1] * width;
	m[2]  = right[2] * width;
	m[3]  = 0.0f;

	/* Column 1: up (scaled by height) */
	m[4]  = up[0] * height;
	m[5]  = up[1] * height;
	m[6]  = up[2] * height;
	m[7]  = 0.0f;

	/* Column 2: normal */
	m[8]  = normal[0];
	m[9]  = normal[1];
	m[10] = normal[2];
	m[11] = 0.0f;

	/* Column 3: translation (origin) */
	m[12] = origin[0];
	m[13] = origin[1];
	m[14] = origin[2];
	m[15] = 1.0f;
}

/* ─── Init / Shutdown ─────────────────────────────────────────────────── */

void q3ide_place_init(void)
{
	memset(q3ide_surfaces, 0, sizeof(q3ide_surfaces));
	q3ide_place_initialized = 1;
}

void q3ide_place_shutdown(void)
{
	int i;
	for (i = 0; i < Q3IDE_MAX_SURFACES; i++) {
		if (q3ide_surfaces[i].active) {
			q3ide_place_surface_destroy(i + 1);
		}
	}
	q3ide_place_initialized = 0;
}

/* ─── Surface management ──────────────────────────────────────────────── */

int q3ide_place_surface_create(float x, float y, float z,
                               float width, float height,
                               const float *normal)
{
	int i;
	q3ide_surface_slot_t *slot;

	if (!q3ide_place_initialized) {
		return 0;
	}

	/* Find free slot */
	for (i = 0; i < Q3IDE_MAX_SURFACES; i++) {
		if (!q3ide_surfaces[i].active) {
			break;
		}
	}
	if (i >= Q3IDE_MAX_SURFACES) {
		return 0;
	}

	slot = &q3ide_surfaces[i];
	slot->active = 1;
	slot->origin[0] = x;
	slot->origin[1] = y;
	slot->origin[2] = z;
	slot->width = width;
	slot->height = height;
	slot->tex_id = 0;

	if (normal) {
		vec3_copy(normal, slot->normal);
	} else {
		slot->normal[0] = 0.0f;
		slot->normal[1] = 1.0f;
		slot->normal[2] = 0.0f;
	}

	mat4_from_placement(slot->transform, slot->origin,
	                    slot->normal, slot->width, slot->height);

	/* TODO: Create Quake3e render entity (refEntity_t) for this quad.
	 * Will be submitted via RE_AddRefEntityToScene each frame. */

	return i + 1;  /* Surface IDs are 1-based */
}

void q3ide_place_surface_set_texture(int surface_id, int tex_id)
{
	q3ide_surface_slot_t *slot;

	if (surface_id < 1 || surface_id > Q3IDE_MAX_SURFACES) {
		return;
	}

	slot = &q3ide_surfaces[surface_id - 1];
	if (!slot->active) {
		return;
	}

	slot->tex_id = tex_id;

	/* TODO: Update the refEntity_t's customShader to reference this texture */
}

void q3ide_place_surface_set_transform(int surface_id,
                                       const float *matrix4x4)
{
	q3ide_surface_slot_t *slot;

	if (surface_id < 1 || surface_id > Q3IDE_MAX_SURFACES) {
		return;
	}

	slot = &q3ide_surfaces[surface_id - 1];
	if (!slot->active || !matrix4x4) {
		return;
	}

	memcpy(slot->transform, matrix4x4, 16 * sizeof(float));
}

void q3ide_place_surface_destroy(int surface_id)
{
	q3ide_surface_slot_t *slot;

	if (surface_id < 1 || surface_id > Q3IDE_MAX_SURFACES) {
		return;
	}

	slot = &q3ide_surfaces[surface_id - 1];
	if (!slot->active) {
		return;
	}

	/* TODO: Remove refEntity_t from render scene */

	memset(slot, 0, sizeof(*slot));
}

/* ─── Wall tracing ────────────────────────────────────────────────────── */

int q3ide_place_trace_wall(const float *origin, const float *direction,
                           float *hit_pos, float *hit_normal)
{
	/* TODO: When integrated into Quake3e:
	 *
	 * trace_t trace;
	 * vec3_t end;
	 *
	 * VectorMA(origin, Q3IDE_TRACE_DIST, direction, end);
	 *
	 * // Use engine's CM_BoxTrace or CG_Trace
	 * trap_CM_BoxTrace(&trace, origin, end,
	 *                  NULL, NULL,  // no bounds (point trace)
	 *                  0,           // model 0 = world
	 *                  CONTENTS_SOLID);
	 *
	 * if (trace.fraction < 1.0f) {
	 *     VectorCopy(trace.endpos, hit_pos);
	 *     VectorCopy(trace.plane.normal, hit_normal);
	 *     return 1;
	 * }
	 *
	 * return 0;
	 */

	/* Stub: simulate a wall hit 200 units ahead */
	if (hit_pos) {
		vec3_ma(origin, 200.0f, direction, hit_pos);
	}
	if (hit_normal) {
		/* Normal facing back toward origin */
		hit_normal[0] = -direction[0];
		hit_normal[1] = -direction[1];
		hit_normal[2] = -direction[2];
	}

	return 1;
}
