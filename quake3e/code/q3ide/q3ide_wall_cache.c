/* NOT_USED_YET
 * q3ide_wall_cache.c — Wall pre-scan cache: build + sort + dump.
 * AAS wall extraction: q3ide_aas_walls.c.  Placement: q3ide_placement.c.
 */

#include "q3ide_wall_cache.h"
#include "q3ide_aas.h"
#include "q3ide_log.h"
#include "../qcommon/qcommon.h"
#include <math.h>
#include <string.h>

/* Approximate ideal window width for slot spacing: 16:9 at Q3IDE_IDEAL_WINDOW_SIZE diagonal. */
#define SLOT_WIN_W ((float) Q3IDE_IDEAL_WINDOW_SIZE * Q3IDE_WALL_SLOT_ASPECT) /* ≈87u for 100u diagonal */

q3ide_wall_cache_t g_wall_cache;

void Q3IDE_WallCache_Invalidate(void)
{
	memset(&g_wall_cache, 0, sizeof(g_wall_cache));
}

/*
 * Compute slot positions for a wall.
 * Slots are evenly spaced, centered on the usable wall width.
 * right[] and centroid[] come from the AAS face data.
 */
static void build_slots(q3ide_cached_wall_t *w, const q3ide_aas_wall_t *raw)
{
	float usable_w, span, step, start_r, cr, center_z;
	int j;

	usable_w = raw->width - 2.0f * (float) Q3IDE_WALL_MARGIN;
	if (usable_w < SLOT_WIN_W)
		usable_w = SLOT_WIN_W; /* at least one slot regardless */

	w->slot_count = (int) (usable_w / (SLOT_WIN_W + (float) Q3IDE_WINDOW_GAP));
	if (w->slot_count < 1)
		w->slot_count = 1;
	if (w->slot_count > Q3IDE_MAX_WALL_SLOTS)
		w->slot_count = Q3IDE_MAX_WALL_SLOTS;
	w->slots_used = 0;

	span = w->slot_count * SLOT_WIN_W + (w->slot_count - 1) * (float) Q3IDE_WINDOW_GAP;
	/* right-axis coordinate of centroid (right[2] == 0 always for horizontal basis) */
	cr = raw->right[0] * raw->centroid[0] + raw->right[1] * raw->centroid[1];
	start_r = cr - span * 0.5f + SLOT_WIN_W * 0.5f;
	step = SLOT_WIN_W + (float) Q3IDE_WINDOW_GAP;
	center_z = (raw->floor_z + raw->ceil_z) * 0.5f;

	for (j = 0; j < w->slot_count; j++) {
		float r = start_r + j * step;
		float dr = r - cr; /* offset from centroid along right axis */
		q3ide_wall_slot_t *sl = &w->slots[j];

		sl->position[0] = raw->centroid[0] + dr * raw->right[0] + raw->normal[0] * (float) Q3IDE_WALL_OFFSET;
		sl->position[1] = raw->centroid[1] + dr * raw->right[1] + raw->normal[1] * (float) Q3IDE_WALL_OFFSET;
		sl->position[2] = center_z;
		sl->normal[0] = raw->normal[0];
		sl->normal[1] = raw->normal[1];
		sl->normal[2] = raw->normal[2];
		sl->width = SLOT_WIN_W;
		sl->height = w->height;
		sl->window_idx = -1;
	}
}

void Q3IDE_WallCache_Build(const vec3_t eye, int area_id)
{
	q3ide_aas_wall_t raw[Q3IDE_MAX_CACHED_WALLS];
	int nraw, i, j;

	memset(&g_wall_cache, 0, sizeof(g_wall_cache));
	g_wall_cache.area_id = area_id;

	nraw = Q3IDE_AAS_GetAreaWalls(eye, raw, Q3IDE_MAX_CACHED_WALLS);

	for (i = 0; i < nraw; i++) {
		q3ide_cached_wall_t *w;
		float h = raw[i].ceil_z - raw[i].floor_z;
		float dx, dy, dz;

		if (h < (float) Q3IDE_MIN_WALL_HEIGHT)
			continue;
		if (raw[i].width < (float) Q3IDE_MIN_WALL_WIDTH)
			continue;
		if (g_wall_cache.wall_count >= Q3IDE_MAX_CACHED_WALLS)
			break;

		w = &g_wall_cache.walls[g_wall_cache.wall_count++];
		w->center[0] = raw[i].centroid[0];
		w->center[1] = raw[i].centroid[1];
		w->center[2] = (raw[i].floor_z + raw[i].ceil_z) * 0.5f;
		w->normal[0] = raw[i].normal[0];
		w->normal[1] = raw[i].normal[1];
		w->normal[2] = raw[i].normal[2];
		w->width = raw[i].width;
		w->height = h;
		dx = w->center[0] - eye[0];
		dy = w->center[1] - eye[1];
		dz = w->center[2] - eye[2];
		w->dist_to_player = sqrtf(dx * dx + dy * dy + dz * dz);

		build_slots(w, &raw[i]);
	}

	/* Insertion sort by dist_to_player (N is small — up to Q3IDE_MAX_CACHED_WALLS) */
	for (i = 1; i < g_wall_cache.wall_count; i++) {
		q3ide_cached_wall_t tmp = g_wall_cache.walls[i];
		for (j = i - 1; j >= 0 && g_wall_cache.walls[j].dist_to_player > tmp.dist_to_player; j--)
			g_wall_cache.walls[j + 1] = g_wall_cache.walls[j];
		g_wall_cache.walls[j + 1] = tmp;
	}

	g_wall_cache.valid = qtrue;
	Q3IDE_LOGI("placement: area %d — %d wall(s) cached", area_id, g_wall_cache.wall_count);
}

void Q3IDE_WallCache_Dump(void)
{
	int i, j;

	if (!g_wall_cache.valid) {
		Com_Printf("q3ide walls: no cache (walk to a new area to trigger scan)\n");
		return;
	}
	Com_Printf("q3ide walls: area=%d walls=%d\n", g_wall_cache.area_id, g_wall_cache.wall_count);
	for (i = 0; i < g_wall_cache.wall_count; i++) {
		q3ide_cached_wall_t *w = &g_wall_cache.walls[i];
		Com_Printf("  [%d] pos=(%.0f,%.0f,%.0f) n=(%.2f,%.2f) w=%.0f h=%.0f dist=%.0f slots=%d/%d\n", i, w->center[0],
		           w->center[1], w->center[2], w->normal[0], w->normal[1], w->width, w->height, w->dist_to_player,
		           w->slots_used, w->slot_count);
		for (j = 0; j < w->slot_count; j++) {
			Com_Printf("    slot[%d] pos=(%.0f,%.0f,%.0f) win=%d\n", j, w->slots[j].position[0],
			           w->slots[j].position[1], w->slots[j].position[2], w->slots[j].window_idx);
		}
	}
}
