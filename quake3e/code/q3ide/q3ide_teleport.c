/*
 * q3ide_teleport.c — Teleport blocker: detects game-level teleports and
 * restores the player to a pre-trigger position.
 * Per-frame tick: q3ide_frame.c.
 */

#include "q3ide_engine_hooks.h"
#include "q3ide_log.h"
#include "q3ide_params.h"
#include "../qcommon/qcommon.h"
#include "../client/client.h"

extern playerState_t *SV_GameClientNum(int num);

static vec3_t tele_pos[Q3IDE_TELE_HIST];
static float tele_yaw[Q3IDE_TELE_HIST];
static int tele_head;
static int tele_filled;
static int tele_block; /* frames remaining in restore window */
static int tele_last_bit = -1;

void q3ide_teleport_block_frame(void)
{
	int bit = (int) (cl.snap.ps.eFlags & EF_TELEPORT_BIT);
	if (tele_last_bit == -1) {
		tele_last_bit = bit;
		VectorCopy(cl.snap.ps.origin, tele_pos[0]);
		tele_yaw[0] = cl.snap.ps.viewangles[YAW];
		tele_filled = 1;
		return;
	}

	if (bit != tele_last_bit && tele_block == 0) {
		Q3IDE_LOGI("teleport blocked");
		tele_block = 30;
	}
	tele_last_bit = bit;

	if (tele_block > 0) {
		/* Restore to position from ~15 frames ago (safely before the trigger) */
		int tail = (tele_filled >= 15) ? (tele_head - 14 + Q3IDE_TELE_HIST) % Q3IDE_TELE_HIST : 0;
		playerState_t *sps = SV_GameClientNum(0);
		if (sps) {
			VectorCopy(tele_pos[tail], sps->origin);
			VectorClear(sps->velocity);
			sps->viewangles[YAW] = tele_yaw[tail];
			VectorCopy(tele_pos[tail], cl.snap.ps.origin);
			VectorClear(cl.snap.ps.velocity);
		}
		tele_block--;
	} else {
		tele_head = (tele_head + 1) % Q3IDE_TELE_HIST;
		VectorCopy(cl.snap.ps.origin, tele_pos[tele_head]);
		tele_yaw[tele_head] = cl.snap.ps.viewangles[YAW];
		if (tele_filled < Q3IDE_TELE_HIST)
			tele_filled++;
	}
}
