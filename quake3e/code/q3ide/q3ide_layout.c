/*
 * q3ide_layout.c — Room layout engine.
 *
 * Uses AAS nav-mesh area geometry (q3ide_aas.c) for exact wall positions —
 * no raycasting approximation. Only remaining trace is the object probe:
 * a short outward sweep to clear decorations/trim mounted on wall faces.
 */

#include "q3ide_layout.h"
#include "q3ide_aas.h"
#include "q3ide_wm.h"
#include "q3ide_wm_internal.h"
#include "q3ide_log.h"
#include "../qcommon/qcommon.h"
#include "../client/client.h"
#include <math.h>
#include <string.h>

/* Minimum clearance from wall surface (after object probe). */
#define Q3IDE_WALL_OFFSET 4.0f

/* ── Placement queue — filled by q3ide_room_layout, drained 1/frame ── */
typedef struct {
	unsigned int id;
	vec3_t pos;
	vec3_t normal;
	float world_w, world_h;
	qboolean is_display;
} q3ide_pending_t;

#define Q3IDE_QUEUE_MAX 32
static q3ide_pending_t g_queue[Q3IDE_QUEUE_MAX];
static int g_queue_head = 0; /* next slot to drain */
static int g_queue_tail = 0; /* next slot to fill */

static void q3ide_queue_push(unsigned int id, const vec3_t pos, const vec3_t normal, float ww, float wh,
                             qboolean is_display)
{
	int next = (g_queue_tail + 1) % Q3IDE_QUEUE_MAX;
	if (next == g_queue_head)
		return; /* full — drop */
	g_queue[g_queue_tail].id = id;
	VectorCopy(pos, g_queue[g_queue_tail].pos);
	VectorCopy(normal, g_queue[g_queue_tail].normal);
	g_queue[g_queue_tail].world_w = ww;
	g_queue[g_queue_tail].world_h = wh;
	g_queue[g_queue_tail].is_display = is_display;
	g_queue_tail = next;
}

void q3ide_layout_queue_reset(void)
{
	g_queue_head = g_queue_tail = 0;
}

qboolean q3ide_layout_tick(void)
{
	q3ide_pending_t *p;
	int existing;
	if (g_queue_head == g_queue_tail)
		return qfalse; /* empty */
	p = &g_queue[g_queue_head];
	g_queue_head = (g_queue_head + 1) % Q3IDE_QUEUE_MAX;

	/* Reuse: if already attached, just move — zero stream cost. */
	existing = Q3IDE_WM_FindById(p->id);
	if (existing >= 0) {
		q3ide_wm.wins[existing].world_w = p->world_w;
		q3ide_wm.wins[existing].world_h = p->world_h;
		Q3IDE_WM_MoveWindow(existing, p->pos, p->normal, qtrue); /* layout already measured fit */
		Q3IDE_LOGI("layout tick: moved id=%u pos=(%.0f,%.0f,%.0f)", p->id, p->pos[0], p->pos[1], p->pos[2]);
		return qtrue;
	}

	/* New window — full attach (expensive, staggered 1/frame). */
	if (p->is_display) {
		if (!q3ide_wm.cap_start_disp ||
		    q3ide_wm.cap_start_disp(q3ide_wm.cap, p->id, Q3IDE_CAPTURE_FPS) != 0)
			return qtrue; /* skip, still consumed */
		Q3IDE_WM_Attach(p->id, p->pos, p->normal, p->world_w, p->world_h, qfalse, qtrue);
	} else {
		Q3IDE_WM_Attach(p->id, p->pos, p->normal, p->world_w, p->world_h, qtrue, qtrue);
	}
	Q3IDE_LOGI("layout tick: attached id=%u pos=(%.0f,%.0f,%.0f)", p->id, p->pos[0], p->pos[1], p->pos[2]);
	return qtrue;
}
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
static void q3ide_measure_wall(const vec3_t contact, const vec3_t normal, q3ide_wall_t *wall, const vec3_t eye)
{
	vec3_t start, end;
	static vec3_t mins = {0, 0, 0}, maxs = {0, 0, 0};
	/* Small box for LOS: passes over very thin protrusions (< 4u) */
	static vec3_t lmins = {-4, -4, -4}, lmaxs = {4, 4, 4};
	float right_dist, left_dist, ceil_dist = 0, floor_dist = 0, wall_h;

	VectorCopy(contact, wall->contact);
	VectorCopy(normal, wall->normal);
	q3ide_layout_right(normal, wall->right);

	/* start: 2u off the wall at contact height (eye level).
	 * Measuring at contact[2] keeps the horizontal sweep truly horizontal so
	 * BSP corners block the LOS check properly — diagonal sweeps at 2/3 height
	 * slip past corner geometry and end up measuring into adjacent rooms. */
	start[0] = contact[0] + normal[0] * Q3IDE_WALL_OFFSET;
	start[1] = contact[1] + normal[1] * Q3IDE_WALL_OFFSET;
	start[2] = contact[2]; /* eye level — horizontal sweep */

	/* Floor / ceil: measured separately for TV sizing (not for width sweep). */
	{
		trace_t htr;
		vec3_t hend;
		vec3_t hstart;
		hstart[0] = start[0];
		hstart[1] = start[1];
		hstart[2] = contact[2];
		hend[0] = hstart[0];
		hend[1] = hstart[1];
		hend[2] = hstart[2] + 512.0f;
		CM_BoxTrace(&htr, hstart, hend, mins, maxs, 0, CONTENTS_SOLID, qfalse);
		wall->ceil_z = hstart[2] + htr.fraction * 512.0f;
		hend[2] = hstart[2] - 512.0f;
		CM_BoxTrace(&htr, hstart, hend, mins, maxs, 0, CONTENTS_SOLID, qfalse);
		wall->floor_z = hstart[2] - htr.fraction * 512.0f;
	}

	/* Width: find the continuous wall span in ±right.
	 * Step outward; stop when player can no longer see the position (corner) OR wall is gone. */
	{
		const float step_sz = 16.0f;
		const float max_reach = 512.0f;
		float d, back_end[3], los_tgt[3];
		trace_t btr, los;

		right_dist = 0.0f;
		for (d = step_sz; d <= max_reach; d += step_sz) {
			end[0] = start[0] + wall->right[0] * d;
			end[1] = start[1] + wall->right[1] * d;
			end[2] = start[2];
			/* LOS from player eye — stops at real corners (not thin protrusions) */
			los_tgt[0] = end[0] + normal[0] * 4.0f;
			los_tgt[1] = end[1] + normal[1] * 4.0f;
			los_tgt[2] = end[2];
			CM_BoxTrace(&los, eye, los_tgt, lmins, lmaxs, 0, CONTENTS_SOLID, qfalse);
			if (!los.startsolid && los.fraction < 0.85f)
				break; /* corner hides this spot from player */
			/* Back-trace: confirm wall still exists here */
			back_end[0] = end[0] - normal[0] * 24.0f;
			back_end[1] = end[1] - normal[1] * 24.0f;
			back_end[2] = end[2];
			CM_BoxTrace(&btr, end, back_end, mins, maxs, 0, CONTENTS_SOLID, qfalse);
			if (btr.fraction >= 1.0f)
				break; /* gap / doorway */
			right_dist = d;
		}

		left_dist = 0.0f;
		for (d = step_sz; d <= max_reach; d += step_sz) {
			end[0] = start[0] - wall->right[0] * d;
			end[1] = start[1] - wall->right[1] * d;
			end[2] = start[2];
			los_tgt[0] = end[0] + normal[0] * 4.0f;
			los_tgt[1] = end[1] + normal[1] * 4.0f;
			los_tgt[2] = end[2];
			CM_BoxTrace(&los, eye, los_tgt, lmins, lmaxs, 0, CONTENTS_SOLID, qfalse);
			if (!los.startsolid && los.fraction < 0.85f)
				break;
			back_end[0] = end[0] - normal[0] * 24.0f;
			back_end[1] = end[1] - normal[1] * 24.0f;
			back_end[2] = end[2];
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

	/* floor_z/ceil_z already measured in the preamble above. Sanity check. */
	wall_h = wall->ceil_z - wall->floor_z;
	if (wall_h < 48.0f) {
		wall->floor_z = start[2] - 96.0f;
		wall->ceil_z = start[2] + 96.0f;
	}
	(void) ceil_dist;
	(void) floor_dist;
}

void q3ide_room_scan(vec3_t eye, q3ide_room_t *out)
{
	q3ide_aas_wall_t aas_walls[Q3IDE_LAYOUT_MAX_WALLS];
	int n, i;

	out->n = 0;

	/* Primary path: AAS-derived exact wall geometry. */
	n = Q3IDE_AAS_GetAreaWalls(eye, aas_walls, Q3IDE_LAYOUT_MAX_WALLS);
	if (n > 0) {
		float radius = Cvar_VariableValue("q3ide_placement_radius");
		if (radius < 64.0f)
			radius = 1200.0f; /* default ~30m */
		for (i = 0; i < n && out->n < Q3IDE_LAYOUT_MAX_WALLS; i++) {
			q3ide_wall_t *w = &out->walls[out->n];
			vec3_t los_tgt;
			trace_t los_tr;
			static vec3_t lmins = {-4, -4, -4}, lmaxs = {4, 4, 4};
			/* Skip walls outside the placement radius */
			{
				vec3_t _d;
				VectorSubtract(eye, aas_walls[i].centroid, _d);
				if (VectorLength(_d) > radius)
					continue;
			}
			/* LOS check: trace from eye to a point 2u off the wall surface.
			 * If blocked by solid geometry the wall is in a different room — skip. */
			los_tgt[0] = aas_walls[i].centroid[0] + aas_walls[i].normal[0] * 2.0f;
			los_tgt[1] = aas_walls[i].centroid[1] + aas_walls[i].normal[1] * 2.0f;
			los_tgt[2] = aas_walls[i].centroid[2];
			CM_BoxTrace(&los_tr, eye, los_tgt, lmins, lmaxs, 0, CONTENTS_SOLID, qfalse);
			if (!los_tr.startsolid && los_tr.fraction < 0.9f)
				continue; /* occluded — not in our room */
			VectorCopy(aas_walls[i].centroid, w->contact);
			VectorCopy(aas_walls[i].normal, w->normal);
			VectorCopy(aas_walls[i].right, w->right);
			w->width = aas_walls[i].width;
			w->left_dist = aas_walls[i].left_dist;
			w->floor_z = aas_walls[i].floor_z;
			w->ceil_z = aas_walls[i].ceil_z;
			Q3IDE_LOGI("wall[%d] w=%.0f(L=%.0f R=%.0f) floor=%.0f ceil=%.0f n=(%.2f,%.2f,%.2f)", out->n, w->width,
			           w->left_dist, w->width - w->left_dist, w->floor_z, w->ceil_z, w->normal[0], w->normal[1],
			           w->normal[2]);
			out->n++;
		}
		Com_Printf("q3ide: room scan (AAS) found %d wall(s)\n", out->n);
		return;
	}

	/* Fallback: raycast scan if AAS not loaded or area not found. */
	{
		int j;
		float yaw = cl.snap.ps.viewangles[YAW] * (float) M_PI / 180.0f;
		for (i = 0; i < Q3IDE_LAYOUT_SCAN_DIRS && out->n < Q3IDE_LAYOUT_MAX_WALLS; i++) {
			vec3_t dir, wp, wn;
			qboolean dup = qfalse;
			float yt = yaw + (float) i * (2.0f * (float) M_PI / Q3IDE_LAYOUT_SCAN_DIRS);
			dir[0] = cosf(yt);
			dir[1] = sinf(yt);
			dir[2] = 0.0f;
			if (!Q3IDE_WM_TraceWall(eye, dir, wp, wn))
				continue;
			if (fabsf(wn[2]) > 0.4f)
				continue;
			for (j = 0; j < out->n; j++)
				if (DotProduct(wn, out->walls[j].normal) > 0.85f) {
					dup = qtrue;
					break;
				}
			if (dup)
				continue;
			q3ide_measure_wall(wp, wn, &out->walls[out->n], eye);
			out->n++;
		}
		Com_Printf("q3ide: room scan (raycast fallback) found %d wall(s)\n", out->n);
	}
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

/* TV area score if `k` windows are placed on `wall`.
 * Returns the area (w*h) of each TV — larger is better.
 * Used by the size-maximizing greedy assignment. */
static float q3ide_tv_score(const q3ide_wall_t *wall, int k, float aspect)
{
	const float margin = 24.0f, gap = 16.0f;
	float wall_h = wall->ceil_z - wall->floor_z;
	float usable = wall->width - 2.0f * margin;
	float u_per_win = (k > 1) ? (usable - (float)(k - 1) * gap) / (float)k : usable;
	float tv_h = wall_h * 0.85f;
	float tv_w;
	if (tv_h < 70.0f)
		tv_h = 70.0f;
	tv_w = tv_h * aspect;
	if (u_per_win > 0.0f && tv_w > u_per_win) {
		tv_w = u_per_win;
		tv_h = (aspect > 0.0f) ? tv_w / aspect : tv_w;
	}
	return tv_w * tv_h;
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

	/* Optimal greedy: assign each window to the wall that gives the largest TV.
	 * Score = TV area if we add this window to that wall.
	 * Walls too short (height < 96u) or too narrow are skipped.
	 * Max slots per wall = usable_width / (70u + gap). */
	{
		const float margin = 24.0f, gap = 16.0f;
		int max_slots[Q3IDE_LAYOUT_MAX_WALLS];

		for (wi = 0; wi < sorted_room.n; wi++) {
			float wall_h = sorted_room.walls[wi].ceil_z - sorted_room.walls[wi].floor_z;
			float usable = sorted_room.walls[wi].width - 2.0f * margin;
			if (wall_h < 96.0f || usable < 48.0f) {
				max_slots[wi] = 0;
				continue;
			}
			max_slots[wi] = (int)(usable / (70.0f + gap));
			if (max_slots[wi] < 1)
				max_slots[wi] = 1;
		}

		for (i = 0; i < n; i++) {
			int best = -1;
			float best_score = -1.0f;
			float asp = (aspects && aspects[i] > 0.0f) ? aspects[i] : 16.0f / 9.0f;
			for (wi = 0; wi < sorted_room.n; wi++) {
				float score;
				if (assigned_cnt[wi] >= max_slots[wi])
					continue;
				score = q3ide_tv_score(&sorted_room.walls[wi], assigned_cnt[wi] + 1, asp);
				if (score > best_score) {
					best_score = score;
					best = wi;
				}
			}
			if (best < 0)
				break;
			assigned[best][assigned_cnt[best]++] = i;
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
			float x_off, tv_w, tv_hj, clear;
			vec3_t pos, wall_surf, probe_end, wnorm;
			trace_t ptr;
			static vec3_t pmins = {-4, -4, -4}, pmaxs = {4, 4, 4};
			VectorCopy(wall->normal, wnorm);

			x_off = -wall->left_dist + margin + ((float) j + 0.5f) * step;
			tv_w = tv_ws[j];
			tv_hj = tv_hs[j];

			/* AAS face centroid + right offset gives exact on-plane position. */
			wall_surf[0] = wall->contact[0] + wall->right[0] * x_off;
			wall_surf[1] = wall->contact[1] + wall->right[1] * x_off;
			wall_surf[2] = center_z;

			/* Object probe: sweep outward 48u from wall plane; clear any
			 * mounted decoration (trim, ledges, torches, signs) by 4u.
			 * This is the ONLY trace needed — AAS gives exact wall position. */
			probe_end[0] = wall_surf[0] + wall->normal[0] * 48.0f;
			probe_end[1] = wall_surf[1] + wall->normal[1] * 48.0f;
			probe_end[2] = wall_surf[2];
			CM_BoxTrace(&ptr, wall_surf, probe_end, pmins, pmaxs, 0, CONTENTS_SOLID, qfalse);
			clear = ptr.fraction * 48.0f + 4.0f;
			if (clear < Q3IDE_WALL_OFFSET)
				clear = Q3IDE_WALL_OFFSET;
			/* Stagger each window 0.5u further than the previous to prevent
			 * Z-fighting between coplanar quads on the same wall. */
			clear += (float) j * 0.5f;

			pos[0] = wall_surf[0] + wall->normal[0] * clear;
			pos[1] = wall_surf[1] + wall->normal[1] * clear;
			pos[2] = center_z;

			/* Push to queue — drained one per frame by q3ide_layout_tick(). */
			q3ide_queue_push(ids[ii], pos, wnorm, tv_w, tv_hj, is_display && is_display[ii]);
			Q3IDE_LOGI("layout: queued id=%u wall=%d x_off=%.0f size=%.0fx%.0f pos=(%.0f,%.0f,%.0f)", ids[ii], wi,
			           x_off, tv_w, tv_hj, pos[0], pos[1], pos[2]);
			placed++;
		}
	}
	return placed;
}
