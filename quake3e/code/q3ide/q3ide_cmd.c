/* q3ide_cmd.c — Reflow queue, ReflowTick, and DetachById. */

#include "q3ide_wm.h"
#include "q3ide_layout.h"
#include "q3ide_log.h"
#include "q3ide_wm_internal.h"
#include "../qcommon/qcommon.h"
#include "../client/client.h"
#include <string.h>

/* App filtering — defined in q3ide_cmd_query.c */
extern const char *q3ide_terminal_apps[];
extern const char *q3ide_browser_apps[];
extern qboolean q3ide_match(const char *app, const char **list);

extern qboolean q3ide_is_attached(unsigned int id);

/* ── Reflow queue — FIFO, drained 1 per frame by Q3IDE_WM_ReflowTick ── */
typedef struct {
	int slot;
	vec3_t pos;
	vec3_t normal;
	float world_w, world_h;
} q3ide_reflow_entry_t;

#define Q3IDE_REFLOW_MAX 32
static q3ide_reflow_entry_t g_reflow[Q3IDE_REFLOW_MAX];
static int g_reflow_head = 0, g_reflow_tail = 0;

void q3ide_reflow_push(int slot, const vec3_t pos, const vec3_t normal, float ww, float wh)
{
	int next = (g_reflow_tail + 1) % Q3IDE_REFLOW_MAX;
	if (next == g_reflow_head)
		return; /* full — drop */
	g_reflow[g_reflow_tail].slot = slot;
	VectorCopy(pos, g_reflow[g_reflow_tail].pos);
	VectorCopy(normal, g_reflow[g_reflow_tail].normal);
	g_reflow[g_reflow_tail].world_w = ww;
	g_reflow[g_reflow_tail].world_h = wh;
	g_reflow_tail = next;
}

qboolean Q3IDE_WM_ReflowTick(void)
{
	q3ide_reflow_entry_t *e;
	q3ide_win_t *w;
	if (g_reflow_head == g_reflow_tail)
		return qfalse;
	e = &g_reflow[g_reflow_head];
	g_reflow_head = (g_reflow_head + 1) % Q3IDE_REFLOW_MAX;
	if (e->slot < 0 || e->slot >= Q3IDE_MAX_WIN || !q3ide_wm.wins[e->slot].active)
		return qtrue; /* stale — skip */
	w = &q3ide_wm.wins[e->slot];
	w->world_w = e->world_w;
	w->world_h = e->world_h;
	Q3IDE_WM_MoveWindow(e->slot, e->pos, e->normal, qtrue);
	Q3IDE_LOGI("reflow tick: slot=%d id=%u size=%.0fx%.0f pos=(%.0f,%.0f,%.0f)", e->slot, w->capture_id, e->world_w,
	           e->world_h, e->pos[0], e->pos[1], e->pos[2]);
	return qtrue;
}

qboolean Q3IDE_WM_DetachById(unsigned int cid)
{
	int i;
	for (i = 0; i < Q3IDE_MAX_WIN; i++) {
		q3ide_win_t *w = &q3ide_wm.wins[i];
		if (!w->active || w->capture_id != cid)
			continue;
		if (q3ide_wm.cap_stop)
			q3ide_wm.cap_stop(q3ide_wm.cap, cid);
		q3ide_wm.slot_mask &= ~(1ULL << w->scratch_slot);
		memset(w, 0, sizeof(q3ide_win_t));
		q3ide_wm.num_active--;
		Q3IDE_LOGI("detached wid=%u", cid);
		return qtrue;
	}
	Q3IDE_LOGI("wid=%u not found", cid);
	return qfalse;
}

/* CmdAttach — q3ide_cmd_attach.c */
/* Reflow     — q3ide_cmd_reflow.c */
/* PollChanges, Desktop, Snap — q3ide_cmd_query.c */
void Q3IDE_WM_PollChanges(void);
void Q3IDE_WM_CmdDesktop(void);
void Q3IDE_WM_CmdSnap(void);
