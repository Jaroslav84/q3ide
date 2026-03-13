/* q3ide_missile_tracking.c — ET_MISSILE entity tracking for shoot-to-place. */

#include "q3ide_engine_hooks.h"
#include "q3ide_log.h"
#include "q3ide_win_mngr.h"
#include "q3ide_win_mngr_internal.h"
#include "q3ide_params.h"
#include "../qcommon/qcommon.h"
#include "../client/client.h"
#include <math.h>

/* Shoot-to-place state — shared with q3ide_engine.c and q3ide_shoot_to_place.c */
extern int q3ide_selected_win; /* wins[] index, -1 = none selected */
extern int q3ide_select_time;  /* Sys_Milliseconds() when selected */

/* Placement cooldown — defined in q3ide_shoot_to_place.c */
extern int q3ide_place_cooldown_end;

/* Burst placement timer — shared between missile and hitscan placement */
int q3ide_burst_place_end = 0;


/* ── Missile entity tracking ─────────────────────────────────────────── */

#define ET_MISSILE_VAL 3 /* ET_MISSILE from bg_public.h */
#define MAX_TRACKED    16

typedef struct {
	int number;
	vec3_t pos;
	vec3_t dir; /* normalized travel direction */
} q3ide_missile_t;

static q3ide_missile_t g_prev[MAX_TRACKED];
static int g_prev_n = 0;

/* Place a window at a missile impact point by tracing wall from last position. */
static void q3ide_place_at_impact(const vec3_t pos, const vec3_t dir)
{
	vec3_t wall_pos, wall_normal, from, tdir;

	if (Sys_Milliseconds() < q3ide_burst_place_end)
		return;

	VectorCopy(pos, from);
	VectorCopy(dir, tdir);
	if (!Q3IDE_WM_TraceWall(from, tdir, wall_pos, wall_normal))
		return;

	if (q3ide_selected_win >= 0 && Sys_Milliseconds() - q3ide_select_time < Q3IDE_REPOSITION_MS) {
		Q3IDE_WM_MoveWindow(q3ide_selected_win, wall_pos, wall_normal, qfalse);
		Q3IDE_LOGI("proj moved [%d] to (%.0f,%.0f,%.0f)", q3ide_selected_win, wall_pos[0], wall_pos[1], wall_pos[2]);
		q3ide_select_time = Sys_Milliseconds();
		q3ide_burst_place_end = Sys_Milliseconds() + Q3IDE_BURST_PLACE_MS;
		Cbuf_AddText("give ammo\n");
	} else if (q3ide_selected_win < 0 && Sys_Milliseconds() >= q3ide_place_cooldown_end) {
		/* No PopulateQueue here — that's done at fire time in q3ide_shoot_frame.
		 * Calling it at impact would freeze the game mid-flight. */
		if (Q3IDE_WM_PendingCount() > 0) {
			Q3IDE_WM_AttachNextPending(wall_pos, wall_normal);
			q3ide_place_cooldown_end = Sys_Milliseconds() + Q3IDE_PLACE_COOLDOWN_MS;
			q3ide_burst_place_end = Sys_Milliseconds() + Q3IDE_BURST_PLACE_MS;
			Cbuf_AddText("give ammo\n");
		}
	}
}

/* Evaluate actual world position of a trajectory at cl.serverTime. */
static void q3ide_eval_traj(const trajectory_t *tr, vec3_t out)
{
	float t = (cl.serverTime - tr->trTime) / 1000.0f;
	if (t < 0.0f)
		t = 0.0f;
	out[0] = tr->trBase[0] + tr->trDelta[0] * t;
	out[1] = tr->trBase[1] + tr->trDelta[1] * t;
	/* TR_GRAVITY (grenades): apply parabolic drop, Q3 gravity = 800 u/s² */
	if (tr->trType == TR_GRAVITY)
		out[2] = tr->trBase[2] + tr->trDelta[2] * t - 0.5f * 800.0f * t * t;
	else
		out[2] = tr->trBase[2] + tr->trDelta[2] * t;
}

/* Called every frame: detect disappeared missile entities → wall placement. */
void q3ide_missile_track_frame(void)
{
	q3ide_missile_t cur[MAX_TRACKED];
	int cur_n = 0;
	int i, j, found;
	int local_client = cl.snap.ps.clientNum;

	/* Build current frame's missile list for local player. */
	for (i = 0; i < cl.snap.numEntities && cur_n < MAX_TRACKED; i++) {
		const entityState_t *e = &cl.parseEntities[(cl.snap.parseEntitiesNum + i) & (MAX_PARSE_ENTITIES - 1)];
		float len;
		if (e->eType != ET_MISSILE_VAL)
			continue;
		if (e->clientNum != local_client)
			continue;
		cur[cur_n].number = e->number;
		/* Compute actual world position from trajectory — NOT trBase (spawn point). */
		q3ide_eval_traj(&e->pos, cur[cur_n].pos);
		/* Normalize trDelta for direction; fall back to (1,0,0) if zero. */
		VectorCopy(e->pos.trDelta, cur[cur_n].dir);
		len = VectorLength(cur[cur_n].dir);
		if (len > 0.001f)
			VectorScale(cur[cur_n].dir, 1.0f / len, cur[cur_n].dir);
		else
			cur[cur_n].dir[0] = 1.0f;
		cur_n++;
	}

	/* Any missile present last frame but gone now → impact. */
	for (i = 0; i < g_prev_n; i++) {
		found = 0;
		for (j = 0; j < cur_n; j++) {
			if (cur[j].number == g_prev[i].number) {
				found = 1;
				break;
			}
		}
		if (!found)
			q3ide_place_at_impact(g_prev[i].pos, g_prev[i].dir);
	}

	/* Store current as previous. */
	for (i = 0; i < cur_n; i++)
		g_prev[i] = cur[i];
	g_prev_n = cur_n;
}

void q3ide_missile_tracking_reset(void)
{
	g_prev_n = 0;
}
