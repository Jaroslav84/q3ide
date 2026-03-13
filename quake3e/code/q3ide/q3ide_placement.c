/*
 * q3ide_placement.c — Area transition placement: queue + FPS-gated drain.
 * Wall cache: q3ide_wall_cache.c.  Window manager: q3ide_win_mngr.c.
 */

#include "q3ide_placement.h"
#include "q3ide_wall_cache.h"
#include "q3ide_win_mngr.h"
#include "q3ide_win_mngr_internal.h"
#include "q3ide_log.h"
#include "q3ide_params.h"
#include "../qcommon/qcommon.h"
#include <math.h>
#include <string.h>

typedef struct {
	int win_idx;
	int wall_idx;
	int slot_idx;
	float win_w, win_h;
} q3ide_place_item_t;

static q3ide_place_item_t g_queue[Q3IDE_MAX_WIN];
static int g_queue_head, g_queue_tail;
static int g_active;
static int g_was_paused; /* streams were already paused before placement started */

int Q3IDE_Placement_IsActive(void)
{
	return g_active;
}

/*
 * Compute window size from slot available height and content aspect ratio.
 * Returns diagonal, or 0.0f if wall is too small for minimum window size.
 */
static float compute_win_size(const q3ide_wall_slot_t *slot, float aspect, float *out_w, float *out_h)
{
	float raw_h, win_h, win_w, diag, scale;

	raw_h = slot->height * (float) Q3IDE_WINDOW_WALL_RATIO;
	if (raw_h > (float) Q3IDE_MAX_WINDOW_SIZE)
		raw_h = (float) Q3IDE_MAX_WINDOW_SIZE;
	if (raw_h < (float) Q3IDE_MIN_WINDOW_SIZE)
		return 0.0f; /* wall too short */

	win_h = raw_h;
	win_w = win_h * aspect;
	if (win_w > slot->width) {
		win_w = slot->width;
		win_h = (aspect > 0.001f) ? win_w / aspect : win_w;
	}

	diag = sqrtf(win_w * win_w + win_h * win_h);
	if (diag < (float) Q3IDE_MIN_WINDOW_SIZE)
		return 0.0f;
	if (diag > (float) Q3IDE_MAX_WINDOW_SIZE) {
		scale = (float) Q3IDE_MAX_WINDOW_SIZE / diag;
		win_w *= scale;
		win_h *= scale;
	}

	*out_w = win_w;
	*out_h = win_h;
	return diag;
}

void Q3IDE_Placement_QueueAll(void)
{
	int wi, wall_idx = 0;

	g_queue_head = g_queue_tail = 0;
	g_active = 0;

	if (!g_wall_cache.valid || g_wall_cache.wall_count == 0) {
		Q3IDE_LOGI("placement: no walls in cache — skipping");
		return;
	}

	/* Clear slot occupancy from previous placement */
	{
		int i, j;
		for (i = 0; i < g_wall_cache.wall_count; i++) {
			g_wall_cache.walls[i].slots_used = 0;
			for (j = 0; j < g_wall_cache.walls[i].slot_count; j++)
				g_wall_cache.walls[i].slots[j].window_idx = -1;
		}
	}

	/* Round-robin: assign one window per wall cycling through all walls closest-first */
	for (wi = 0; wi < Q3IDE_MAX_WIN; wi++) {
		q3ide_win_t *w = &q3ide_wm.wins[wi];
		q3ide_cached_wall_t *wall;
		q3ide_wall_slot_t *slot;
		q3ide_place_item_t *item;
		float aspect, win_w, win_h;
		int si, attempts;

		if (!w->active || !w->is_tunnel)
			continue;
		/* Skip Focus3 display captures and overview-managed windows —
		 * they are repositioned by their own view mode logic. */
		if (!w->owns_stream || w->in_overview)
			continue;
		if (g_queue_tail >= Q3IDE_MAX_WIN)
			break;

		/* Find next wall with a free slot (round-robin, closest-first order) */
		attempts = 0;
		while (attempts < g_wall_cache.wall_count) {
			wall = &g_wall_cache.walls[wall_idx % g_wall_cache.wall_count];
			if (wall->slots_used < wall->slot_count)
				break;
			wall_idx++;
			attempts++;
		}
		if (attempts >= g_wall_cache.wall_count)
			break; /* all walls full */

		si = wall->slots_used;
		slot = &wall->slots[si];

		aspect = (w->tex_w > 0 && w->tex_h > 0) ? (float) w->tex_w / (float) w->tex_h : (16.0f / 9.0f);
		if (compute_win_size(slot, aspect, &win_w, &win_h) == 0.0f) {
			wall_idx++; /* slot/wall too short — try next wall */
			continue;
		}

		item = &g_queue[g_queue_tail++];
		item->win_idx = wi;
		item->wall_idx = (int) (wall - g_wall_cache.walls);
		item->slot_idx = si;
		item->win_w = win_w;
		item->win_h = win_h;

		slot->window_idx = wi;
		wall->slots_used++;
		wall_idx++; /* next window goes to next wall (round-robin) */
	}

	if (g_queue_tail > 0) {
		g_active = 1;
		Q3IDE_WM_PauseStreams();
		Q3IDE_LOGI("placement: queued %d window(s) across %d wall(s)", g_queue_tail, g_wall_cache.wall_count);
	}
}

void Q3IDE_Placement_Tick(void)
{
	q3ide_place_item_t *item;
	q3ide_cached_wall_t *wall;
	q3ide_wall_slot_t *slot;
	q3ide_win_t *w;

	if (!g_active || g_queue_head >= g_queue_tail) {
		if (g_active) {
			g_active = 0;
			Q3IDE_WM_ResumeStreams();
			Q3IDE_LOGI("placement: done — all windows placed");
		}
		return;
	}

	item = &g_queue[g_queue_head++];
	w = &q3ide_wm.wins[item->win_idx];
	if (!w->active)
		return; /* window closed while queued */

	wall = &g_wall_cache.walls[item->wall_idx];
	slot = &wall->slots[item->slot_idx];
	w->world_w = item->win_w;
	w->world_h = item->win_h;
	Q3IDE_WM_MoveWindow(item->win_idx, slot->position, slot->normal, qtrue);

	Q3IDE_LOGI("placement: win[%d] → wall[%d] slot[%d] (%.0fx%.0f u)", item->win_idx, item->wall_idx, item->slot_idx,
	           item->win_w, item->win_h);
}
