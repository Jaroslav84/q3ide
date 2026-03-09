/*
 * q3ide_placement.c — Placement queue: stagger window attach 1/frame.
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

void q3ide_queue_push(unsigned int id, const vec3_t pos, const vec3_t normal, float ww, float wh, qboolean is_display)
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
		if (!q3ide_wm.cap_start_disp || q3ide_wm.cap_start_disp(q3ide_wm.cap, p->id, Q3IDE_CAPTURE_FPS) != 0)
			return qtrue; /* skip, still consumed */
		Q3IDE_WM_Attach(p->id, p->pos, p->normal, p->world_w, p->world_h, qfalse, qtrue);
	} else {
		Q3IDE_WM_Attach(p->id, p->pos, p->normal, p->world_w, p->world_h, qtrue, qtrue);
	}
	Q3IDE_LOGI("layout tick: attached id=%u pos=(%.0f,%.0f,%.0f)", p->id, p->pos[0], p->pos[1], p->pos[2]);
	return qtrue;
}
/* Room layout algorithm and wall measurement — defined in q3ide_layout_algo.c */
void q3ide_room_scan(vec3_t eye, q3ide_room_t *out);
int q3ide_room_layout(const q3ide_room_t *room, unsigned int *ids, float *aspects, const int *is_display, int n);
