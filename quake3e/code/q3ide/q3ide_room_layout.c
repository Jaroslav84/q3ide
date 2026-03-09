/* q3ide_room_layout.c — greedy wall assignment + no-overlap TV placement. */
#include "q3ide_layout.h"
#include "q3ide_aas.h"
#include "q3ide_wm.h"
#include "q3ide_wm_internal.h"
#include "q3ide_log.h"
#include "../qcommon/qcommon.h"
#include "../client/client.h"
#include <math.h>
#include <string.h>

extern void q3ide_queue_push(unsigned int id, const vec3_t pos, const vec3_t normal, float ww, float wh,
                             qboolean is_display);
extern void q3ide_sort_walls_by_facing(q3ide_room_t *room, float yaw);
extern float q3ide_tv_score(const q3ide_wall_t *wall, int k, float aspect);

#define Q3IDE_WALL_OFFSET 3.0f
#define Q3IDE_LAYOUT_GAP 16.0f
#define Q3IDE_LAYOUT_MARGIN 24.0f
#define Q3IDE_MIN_TV_H 64.0f /* below this → look for another wall */

/* ── helpers ─────────────────────────────────────────────────────── */

static float ideal_tv_w(float tv_h, float asp)
{
	return tv_h * asp;
}

/* ── public entry point ──────────────────────────────────────────── */

int q3ide_room_layout(const q3ide_room_t *room, unsigned int *ids, float *aspects, const int *is_display, int n)
{
	/* Per-wall window list */
	int assigned[Q3IDE_LAYOUT_MAX_WALLS][Q3IDE_MAX_WIN];
	int assigned_cnt[Q3IDE_LAYOUT_MAX_WALLS];
	float assigned_ws[Q3IDE_LAYOUT_MAX_WALLS][Q3IDE_MAX_WIN]; /* ideal widths */
	float wall_tv_h[Q3IDE_LAYOUT_MAX_WALLS];
	float wall_usable[Q3IDE_LAYOUT_MAX_WALLS];

	q3ide_room_t sorted_room;
	int i, wi, placed = 0;

	if (!room || room->n == 0 || n == 0)
		return 0;

	{
		float yaw = cl.snap.ps.viewangles[YAW] * (float) M_PI / 180.0f;
		sorted_room = *room;
		q3ide_sort_walls_by_facing(&sorted_room, yaw);
	}

	memset(assigned_cnt, 0, sizeof(assigned_cnt));

	/* Pre-compute per-wall tv_h and usable width */
	for (wi = 0; wi < sorted_room.n; wi++) {
		float wall_h = sorted_room.walls[wi].ceil_z - sorted_room.walls[wi].floor_z;
		float tv_h = wall_h * 0.85f;
		if (tv_h < Q3IDE_MIN_TV_H)
			tv_h = Q3IDE_MIN_TV_H;
		wall_tv_h[wi] = tv_h;
		wall_usable[wi] = sorted_room.walls[wi].width - 2.0f * Q3IDE_LAYOUT_MARGIN;
	}

	/* Greedy assignment: try walls in score order, accept first that fits above MIN_TV_H */
	for (i = 0; i < n; i++) {
		float asp = (aspects && aspects[i] > 0.0f) ? aspects[i] : 16.0f / 9.0f;
		int best = -1;
		float best_score = -1.0f;

		/* Find highest-scoring wall that can accommodate this window above MIN_TV_H */
		for (wi = 0; wi < sorted_room.n; wi++) {
			float wall_h = sorted_room.walls[wi].ceil_z - sorted_room.walls[wi].floor_z;
			float score;
			if (wall_h < 96.0f || wall_usable[wi] < 32.0f)
				continue;
			if (assigned_cnt[wi] >= Q3IDE_MAX_WIN)
				continue;
			score = q3ide_tv_score(&sorted_room.walls[wi], assigned_cnt[wi] + 1, asp);
			if (score > best_score) {
				best_score = score;
				best = wi;
			}
		}

		if (best < 0) {
			Q3IDE_LOGI("layout: id=%u no wall fits above min — will float", ids[i]);
			continue;
		}

		assigned_ws[best][assigned_cnt[best]] = ideal_tv_w(wall_tv_h[best], asp);
		assigned[best][assigned_cnt[best]] = i;
		assigned_cnt[best]++;
	}

	/* Per-wall: compute final sizes + cumulative no-overlap placement */
	for (wi = 0; wi < sorted_room.n; wi++) {
		const q3ide_wall_t *wall = &sorted_room.walls[wi];
		int cnt = assigned_cnt[wi];
		float tv_h = wall_tv_h[wi];
		float usable = wall_usable[wi];
		float tv_ws[Q3IDE_MAX_WIN], tv_hs[Q3IDE_MAX_WIN];
		float total_w, center_z, x_cursor;
		int j;

		if (!cnt)
			continue;

		for (j = 0; j < cnt; j++) {
			int ii = assigned[wi][j];
			float asp = (aspects && aspects[ii] > 0.0f) ? aspects[ii] : 16.0f / 9.0f;
			tv_ws[j] = tv_h * asp;
			tv_hs[j] = tv_h;
		}

		/* No scale-to-fit: windows keep their natural size, row may overflow wall */
		total_w = (float) (cnt - 1) * Q3IDE_LAYOUT_GAP;
		for (j = 0; j < cnt; j++)
			total_w += tv_ws[j];
		center_z = (wall->floor_z + wall->ceil_z) * 0.5f;
		x_cursor = -wall->left_dist + Q3IDE_LAYOUT_MARGIN + (usable - total_w) * 0.5f;
		if (x_cursor < -wall->left_dist + Q3IDE_LAYOUT_MARGIN)
			x_cursor = -wall->left_dist + Q3IDE_LAYOUT_MARGIN;

		for (j = 0; j < cnt; j++) {
			int ii = assigned[wi][j];
			float x_off, clear;
			vec3_t pos, wall_surf, probe_end;
			trace_t ptr;
			static vec3_t pmins = {-4, -4, -4}, pmaxs = {4, 4, 4};

			x_off = x_cursor + tv_ws[j] * 0.5f;
			wall_surf[0] = wall->contact[0] + wall->right[0] * x_off;
			wall_surf[1] = wall->contact[1] + wall->right[1] * x_off;
			wall_surf[2] = center_z;
			probe_end[0] = wall_surf[0] + wall->normal[0] * 48.0f;
			probe_end[1] = wall_surf[1] + wall->normal[1] * 48.0f;
			probe_end[2] = wall_surf[2];
			CM_BoxTrace(&ptr, wall_surf, probe_end, pmins, pmaxs, 0, CONTENTS_SOLID, qfalse);
			clear = ptr.fraction * 48.0f + 4.0f;
			if (clear < Q3IDE_WALL_OFFSET)
				clear = Q3IDE_WALL_OFFSET;
			clear += (float) j * 0.5f;
			pos[0] = wall_surf[0] + wall->normal[0] * clear;
			pos[1] = wall_surf[1] + wall->normal[1] * clear;
			pos[2] = center_z;
			q3ide_queue_push(ids[ii], pos, wall->normal, tv_ws[j], tv_hs[j], is_display && is_display[ii]);
			Q3IDE_LOGI("layout: queued id=%u wall=%d x_off=%.0f size=%.0fx%.0f"
			           " pos=(%.0f,%.0f,%.0f)",
			           ids[ii], wi, x_off, tv_ws[j], tv_hs[j], pos[0], pos[1], pos[2]);
			placed++;
			x_cursor += tv_ws[j] + Q3IDE_LAYOUT_GAP;
		}
	}
	return placed;
}
