/*
 * q3ide_layout.c — Room layout engine.
 *
 * Scans room walls, measures real width/height via ray traces, then sizes
 * and distributes windows as TV-mounted displays (66% of wall height).
 */

#include "q3ide_layout.h"
#include "q3ide_wm.h"
#include "q3ide_wm_internal.h"
#include "q3ide_log.h"
#include "../qcommon/qcommon.h"
#include "../client/client.h"
#include <math.h>
#include <string.h>

#define Q3IDE_LAYOUT_SCAN_DIRS 24 /* every 15° */
#define Q3IDE_WALL_OFFSET 2.0f

/* Compute horizontal right vector from a wall normal (inline from q3ide_geom.c) */
static void q3ide_layout_right(const vec3_t normal, vec3_t right)
{
	float nx = normal[0], ny = normal[1];
	float len = sqrtf(nx * nx + ny * ny);
	if (len > 0.01f) {
		right[0] = -ny / len;
		right[1] = nx / len;
		right[2] = 0.0f;
	} else {
		right[0] = 1.0f;
		right[1] = 0.0f;
		right[2] = 0.0f;
	}
}

/* Measure one wall: fills right, width, floor_z, ceil_z.
 * Width is measured at contact height (always on the wall surface).
 * floor_z/ceil_z are used only for TV size calculation. */
static void q3ide_measure_wall(const vec3_t contact, const vec3_t normal, q3ide_wall_t *wall)
{
	trace_t tr;
	vec3_t start, end;
	static vec3_t mins = {0, 0, 0}, maxs = {0, 0, 0};
	float right_dist, left_dist, ceil_dist, floor_dist, wall_h;

	VectorCopy(contact, wall->contact);
	VectorCopy(normal, wall->normal);
	q3ide_layout_right(normal, wall->right);

	/* start: 2u off the wall at contact height (guaranteed on wall surface) */
	start[0] = contact[0] + normal[0] * Q3IDE_WALL_OFFSET;
	start[1] = contact[1] + normal[1] * Q3IDE_WALL_OFFSET;
	start[2] = contact[2] + normal[2] * Q3IDE_WALL_OFFSET;

	/* Width: find the continuous wall span in ±right.
	 * Step outward and back-trace each position; stop at the first gap. */
	{
		const float step_sz = 16.0f;
		const float max_reach = 512.0f;
		float d, back_end[3];
		trace_t btr;

		right_dist = 0.0f;
		for (d = step_sz; d <= max_reach; d += step_sz) {
			back_end[0] = start[0] + wall->right[0] * d - wall->normal[0] * 24.0f;
			back_end[1] = start[1] + wall->right[1] * d - wall->normal[1] * 24.0f;
			back_end[2] = start[2];
			end[0] = start[0] + wall->right[0] * d;
			end[1] = start[1] + wall->right[1] * d;
			end[2] = start[2];
			CM_BoxTrace(&btr, end, back_end, mins, maxs, 0, CONTENTS_SOLID, qfalse);
			if (btr.fraction >= 1.0f)
				break;
			right_dist = d;
		}

		left_dist = 0.0f;
		for (d = step_sz; d <= max_reach; d += step_sz) {
			back_end[0] = start[0] - wall->right[0] * d - wall->normal[0] * 24.0f;
			back_end[1] = start[1] - wall->right[1] * d - wall->normal[1] * 24.0f;
			back_end[2] = start[2];
			end[0] = start[0] - wall->right[0] * d;
			end[1] = start[1] - wall->right[1] * d;
			end[2] = start[2];
			CM_BoxTrace(&btr, end, back_end, mins, maxs, 0, CONTENTS_SOLID, qfalse);
			if (btr.fraction >= 1.0f)
				break;
			left_dist = d;
		}
	}

	wall->left_dist = left_dist;
	wall->width = right_dist + left_dist;
	if (wall->width < 64.0f)
		wall->width = 64.0f;
	if (wall->width > 2048.0f)
		wall->width = 2048.0f;

	/* Height: trace ±Z for TV sizing */
	end[0] = start[0];
	end[1] = start[1];
	end[2] = start[2] + 512.0f;
	CM_BoxTrace(&tr, start, end, mins, maxs, 0, CONTENTS_SOLID, qfalse);
	ceil_dist = tr.fraction * 512.0f;

	end[2] = start[2] - 512.0f;
	CM_BoxTrace(&tr, start, end, mins, maxs, 0, CONTENTS_SOLID, qfalse);
	floor_dist = tr.fraction * 512.0f;

	wall->ceil_z = start[2] + ceil_dist;
	wall->floor_z = start[2] - floor_dist;

	wall_h = wall->ceil_z - wall->floor_z;
	if (wall_h < 48.0f) {
		wall->floor_z = start[2] - 96.0f;
		wall->ceil_z = start[2] + 96.0f;
	}
	/* contact[2] stays at actual scan height — placement uses this Z */
}

void q3ide_room_scan(vec3_t eye, q3ide_room_t *out)
{
	int i, j;
	float yaw;

	out->n = 0;
	yaw = cl.snap.ps.viewangles[YAW] * (float) M_PI / 180.0f;

	for (i = 0; i < Q3IDE_LAYOUT_SCAN_DIRS && out->n < Q3IDE_LAYOUT_MAX_WALLS; i++) {
		vec3_t dir, wp, wn;
		float yt = yaw + (float) i * (2.0f * (float) M_PI / (float) Q3IDE_LAYOUT_SCAN_DIRS);
		qboolean dup = qfalse;

		dir[0] = cosf(yt);
		dir[1] = sinf(yt);
		dir[2] = 0.0f;

		if (!Q3IDE_WM_TraceWall(eye, dir, wp, wn))
			continue;

		/* Skip non-vertical surfaces (ramps, slopes, ceilings) */
		if (fabsf(wn[2]) > 0.4f)
			continue;

		/* Dedup: same wall normal within 0.85 cosine threshold */
		for (j = 0; j < out->n; j++) {
			if (DotProduct(wn, out->walls[j].normal) > 0.85f) {
				dup = qtrue;
				break;
			}
		}
		if (dup)
			continue;

		q3ide_measure_wall(wp, wn, &out->walls[out->n]);
		Q3IDE_LOGI("wall[%d] w=%.0f(L=%.0f R=%.0f) floor=%.0f ceil=%.0f n=(%.2f,%.2f,%.2f)", out->n,
		           out->walls[out->n].width, out->walls[out->n].left_dist,
		           out->walls[out->n].width - out->walls[out->n].left_dist, out->walls[out->n].floor_z,
		           out->walls[out->n].ceil_z, out->walls[out->n].normal[0], out->walls[out->n].normal[1],
		           out->walls[out->n].normal[2]);
		out->n++;
	}
	Com_Printf("q3ide: room scan found %d wall(s)\n", out->n);
}

/* Sort walls by how directly they face the player.
 * Score = dot(wall_normal, -player_fwd): walls the player looks at score highest.
 * Descending: most-facing wall first (player sees it naturally). */
static void q3ide_sort_walls_by_facing(q3ide_room_t *room, float yaw)
{
	float scores[Q3IDE_LAYOUT_MAX_WALLS];
	float fx = cosf(yaw), fy = sinf(yaw);
	int i, j;
	q3ide_wall_t tmp;
	float ts;

	for (i = 0; i < room->n; i++)
		scores[i] = -(room->walls[i].normal[0] * fx + room->walls[i].normal[1] * fy);

	for (i = 1; i < room->n; i++) {
		tmp = room->walls[i];
		ts = scores[i];
		j = i - 1;
		while (j >= 0 && scores[j] < ts) {
			room->walls[j + 1] = room->walls[j];
			scores[j + 1] = scores[j];
			j--;
		}
		room->walls[j + 1] = tmp;
		scores[j + 1] = ts;
	}
}

int q3ide_room_layout(const q3ide_room_t *room, unsigned int *ids, float *aspects, int *is_display, int n)
{
	/* Assignment arrays: which items go on which wall */
	int assigned[Q3IDE_LAYOUT_MAX_WALLS][Q3IDE_MAX_WIN];
	int assigned_cnt[Q3IDE_LAYOUT_MAX_WALLS];
	/* mutable copy of room so we can sort */
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

	/* Greedy assignment sorted by player facing.
	 * Wall capacity = how many TVs fit at 85% of wall height (min 70u).
	 * Walls that can't fit even one 70u-tall TV are skipped. */
	{
		const float margin = 24.0f, gap = 16.0f;
		const float min_tv_h = 70.0f;
		float remaining[Q3IDE_LAYOUT_MAX_WALLS];
		int max_slots[Q3IDE_LAYOUT_MAX_WALLS];

		for (wi = 0; wi < sorted_room.n; wi++) {
			const q3ide_wall_t *wall = &sorted_room.walls[wi];
			float wall_h = wall->ceil_z - wall->floor_z;
			float tv_h_nom = wall_h * 0.85f;
			float tv_w_nom;
			float slot, usable;
			if (tv_h_nom < min_tv_h)
				tv_h_nom = min_tv_h;
			tv_w_nom = tv_h_nom * 1.78f; /* 16:9 nominal */
			slot = tv_w_nom + gap;
			usable = wall->width - 2.0f * margin;
			remaining[wi] = usable;
			/* Skip wall if even one TV at min size won't fit */
			if (usable < min_tv_h * 1.78f) {
				max_slots[wi] = 0;
			} else {
				max_slots[wi] = (slot > 0.0f) ? (int) (usable / slot) : 0;
				if (max_slots[wi] < 1)
					max_slots[wi] = 1;
			}
		}

		for (i = 0; i < n; i++) {
			int best = -1;
			for (wi = 0; wi < sorted_room.n; wi++) {
				if (assigned_cnt[wi] >= max_slots[wi])
					continue;
				if (best < 0 || remaining[wi] > remaining[best])
					best = wi;
			}
			if (best < 0)
				break;
			assigned[best][assigned_cnt[best]++] = i;
			remaining[best] -= (sorted_room.walls[best].width - 2.0f * margin) / (float) max_slots[best];
		}
	}

	/* Per-wall layout */
	for (wi = 0; wi < sorted_room.n; wi++) {
		const q3ide_wall_t *wall = &sorted_room.walls[wi];
		int cnt = assigned_cnt[wi];
		float wall_h, tv_h, center_z;
		float total_w, scale, step;
		float tv_ws[Q3IDE_MAX_WIN], tv_hs[Q3IDE_MAX_WIN];
		const float margin = 24.0f, gap = 16.0f;
		int j;

		if (!cnt)
			continue;

		wall_h = wall->ceil_z - wall->floor_z;
		tv_h = wall_h * 0.85f;
		if (tv_h < 70.0f)
			tv_h = 70.0f;
		center_z = (wall->floor_z + wall->ceil_z) * 0.5f;

		/* Compute initial TV sizes per item on this wall */
		for (j = 0; j < cnt; j++) {
			int ii = assigned[wi][j];
			float tv_w;

			tv_ws[j] = tv_h * aspects[ii];
			tv_hs[j] = tv_h;

			/* Fit check: if tv_w > wall.width * 0.9 → scale down */
			tv_w = tv_ws[j];
			if (tv_w > wall->width * 0.9f) {
				tv_w = wall->width * 0.9f;
				tv_hs[j] = tv_w / aspects[ii];
				tv_ws[j] = tv_w;
			}
			/* Floor at 70u tall (~70" diagonal at 16:9) */
			if (tv_hs[j] < 70.0f) {
				tv_hs[j] = 70.0f;
				tv_ws[j] = tv_hs[j] * aspects[ii];
			}
		}

		/* Compute total width of all TVs on this wall */
		total_w = (float) (cnt - 1) * gap;
		for (j = 0; j < cnt; j++)
			total_w += tv_ws[j];

		/* Scale down uniformly if they don't fit */
		if (total_w > wall->width - 2.0f * margin && total_w > 0.0f) {
			scale = (wall->width - 2.0f * margin - (float) (cnt - 1) * gap) / (total_w - (float) (cnt - 1) * gap);
			if (scale > 0.0f && scale < 1.0f) {
				for (j = 0; j < cnt; j++) {
					tv_ws[j] *= scale;
					tv_hs[j] *= scale;
					if (tv_hs[j] < 64.0f) {
						tv_hs[j] = 64.0f;
						tv_ws[j] = tv_hs[j] * aspects[assigned[wi][j]];
					}
				}
			}
		}

		/* Even horizontal distribution using step spacing.
		 * wall->contact is the ray-hit point, NOT the wall center.
		 * The wall spans: contact - right*left_dist  to  contact + right*right_dist.
		 * x_off is relative to contact, so left edge is at -left_dist. */
		step = (wall->width - 2.0f * margin) / (float) cnt;

		for (j = 0; j < cnt; j++) {
			int ii = assigned[wi][j];
			float x_off, tv_w, tv_hj;
			vec3_t pos, vend, wnorm;
			trace_t vtr;
			qboolean ok;
			static vec3_t vmins = {0, 0, 0}, vmaxs = {0, 0, 0};
			VectorCopy(wall->normal, wnorm); /* non-const copy for Attach API */

			x_off = -wall->left_dist + margin + ((float) j + 0.5f) * step;
			tv_w = tv_ws[j];
			tv_hj = tv_hs[j];

			pos[0] = wall->contact[0] + wall->right[0] * x_off;
			pos[1] = wall->contact[1] + wall->right[1] * x_off;
			pos[2] = wall->contact[2]; /* actual wall hit height — guaranteed on surface */

			/* Validate: trace back into wall to confirm geometry exists at this position.
			 * Width was measured at center_z but corners/doorways can still cause gaps. */
			vend[0] = pos[0] - wall->normal[0] * 24.0f;
			vend[1] = pos[1] - wall->normal[1] * 24.0f;
			vend[2] = pos[2] - wall->normal[2] * 24.0f;
			CM_BoxTrace(&vtr, pos, vend, vmins, vmaxs, 0, CONTENTS_SOLID, qfalse);
			if (vtr.fraction >= 1.0f) {
				Q3IDE_LOGE("layout: no wall at x_off=%.0f for id=%u — skipping", x_off, ids[ii]);
				continue;
			}
			/* Snap pos to actual wall surface.
			 * Always use center_z for non-horizontal surfaces so windows are
			 * TV-mounted at wall mid-height regardless of surface angle. */
			pos[0] = vtr.endpos[0] + wall->normal[0] * 2.0f;
			pos[1] = vtr.endpos[1] + wall->normal[1] * 2.0f;
			pos[2] = (fabsf(wall->normal[2]) < 0.7f) ? center_z : vtr.endpos[2] + wall->normal[2] * 2.0f;

			if (is_display && is_display[ii]) {
				if (!q3ide_wm.cap_start_disp ||
				    q3ide_wm.cap_start_disp(q3ide_wm.cap, ids[ii], Q3IDE_CAPTURE_FPS) != 0) {
					Q3IDE_LOGE("layout: disp start failed id=%u", ids[ii]);
					continue;
				}
				ok = Q3IDE_WM_Attach(ids[ii], pos, wnorm, tv_w, tv_hj, qfalse, qtrue);
			} else {
				ok = Q3IDE_WM_Attach(ids[ii], pos, wnorm, tv_w, tv_hj, qtrue, qtrue);
			}

			if (ok) {
				placed++;
				Q3IDE_LOGI("layout: placed id=%u wall=%d x_off=%.0f size=%.0fx%.0f pos=(%.0f,%.0f,%.0f)", ids[ii], wi,
				           x_off, tv_w, tv_hj, pos[0], pos[1], pos[2]);
			} else {
				Q3IDE_LOGE("layout: attach failed id=%u", ids[ii]);
			}
		}
	}
	return placed;
}
