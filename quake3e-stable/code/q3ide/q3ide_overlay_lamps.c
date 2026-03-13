/*
 * q3ide_overlay_lamps.c — Area label + window-list footer (failure alert + lamp legend).
 * Called from Q3IDE_DrawAreaLabel / Q3IDE_DrawWinListFooter.
 */

#include "q3ide_engine_hooks.h"
#include "q3ide_params.h"
#include "q3ide_win_mngr.h"
#include "q3ide_win_mngr_internal.h"
#include "../qcommon/qcommon.h"
#include "../qcommon/cm_public.h"
#include "../client/client.h"
#include <string.h>

extern void q3ide_ovl_char(float ox, float oy, float oz, const float *rx, const float *ux, int ch, byte r, byte g,
                           byte b);
extern void q3ide_ovl_char_sm(float ox, float oy, float oz, const float *rx, const float *ux, int ch, byte r, byte g,
                              byte b);
extern void q3ide_ovl_str(float ox, float oy, float oz, const float *rx, const float *ux, const char *s, byte r, byte g,
                          byte b);
extern void q3ide_ovl_str_sm(float ox, float oy, float oz, const float *rx, const float *ux, const char *s, byte r,
                             byte g, byte b);

/* ── Area label ──────────────────────────────────────────────────────── */

void Q3IDE_DrawAreaLabel(float ox, float oy, float oz, const float *rx, const float *ux, float kb_bot)
{
	if (cls.state != CA_ACTIVE)
		return;
	{
		int leafnum = CM_PointLeafnum(cl.snap.ps.origin);
		char room_buf[32];
		float area_u = kb_bot - Q3IDE_OVL_SECTION_PAD_WU;
		float alx = ox + ux[0] * area_u;
		float aly = oy + ux[1] * area_u;
		float alz = oz + ux[2] * area_u;
		Com_sprintf(room_buf, sizeof(room_buf), "area %d cls %d", CM_LeafArea(leafnum), CM_LeafCluster(leafnum));
		q3ide_ovl_str(alx, aly, alz, rx, ux, room_buf, 100, 200, 255);
	}
}

/* Draw a short string rotated -90° (reads bottom-to-top) using small glyph scale.
 * Characters advance upward (ux direction); glyph body faces right (-rx as height). */
static void ovl_str_vert(float ox, float oy, float oz, const float *rx, const float *ux, const char *s, byte r, byte g,
                         byte b)
{
	float vx[3] = {ux[0], ux[1], ux[2]};
	float vy[3] = {-rx[0], -rx[1], -rx[2]};
	q3ide_ovl_str_sm(ox, oy, oz, vx, vy, s, r, g, b);
}

/* ── Window-list footer: failure alert + four-column lamp legend ──────── */
/*
 * ox,oy,oz    — anchor (bottom of the window list; same origin as Q3IDE_DrawWinList)
 * sm_lh       — small line height (same value used for entries)
 * wrow_start  — number of entry rows already drawn (footer begins above them)
 *
 * Layout above wrow_start (rows counted from anchor upward):
 *   wrow+1    — legend stars
 *   wrow+1.5  — legend labels base (text extends further up, -90° rotated)
 *   wrow+5    — stream failure alert (only when dead > 0)
 */
void Q3IDE_DrawWinListFooter(float ox, float oy, float oz, const float *rx, const float *ux, float sm_lh,
                             int wrow_start)
{
	int wi;

	/* Stream failure alert */
	{
		int dead = 0;
		for (wi = 0; wi < Q3IDE_MAX_WIN; wi++) {
			q3ide_win_t *w = &q3ide_wm.wins[wi];
			if (w->active && w->owns_stream && !w->stream_active)
				dead++;
		}
		if (dead > 0) {
			char alert[32];
			float al_u = (float) (wrow_start + 5) * sm_lh;
			float alx = ox + ux[0] * al_u;
			float aly = oy + ux[1] * al_u;
			float alz = oz + ux[2] * al_u;
			Com_sprintf(alert, sizeof(alert), "! %d STREAM%s DEAD", dead, dead == 1 ? "" : "S");
			q3ide_ovl_str(alx, aly, alz, rx, ux, alert, 255, 50, 50);
		}
	}

	/* Lamp legend — 4 columns aligning with per-window lamp columns.
	 * Stars on the legend row; labels rotated -90° just above. */
	{
		float leg_u = (float) (wrow_start + 1) * sm_lh; /* star row */
		float lbl_u = leg_u + sm_lh * 0.5f;             /* label base, above stars */
		float lx = ox + ux[0] * leg_u;
		float ly = oy + ux[1] * leg_u;
		float lz = oz + ux[2] * leg_u;

		/* Col 1: ever (ever failed) */
		q3ide_ovl_char_sm(lx, ly, lz, rx, ux, '*', 40, 200, 70);
		{
			float tx = ox + ux[0] * lbl_u;
			float ty = oy + ux[1] * lbl_u;
			float tz = oz + ux[2] * lbl_u;
			ovl_str_vert(tx, ty, tz, rx, ux, "ever", 255, 255, 255);
		}

		/* Col 2: current (failing now) */
		{
			float l2x = lx + rx[0] * Q3IDE_OVL_CHAR_W * 1.6f;
			float l2y = ly + rx[1] * Q3IDE_OVL_CHAR_W * 1.6f;
			float l2z = lz + rx[2] * Q3IDE_OVL_CHAR_W * 1.6f;
			float t2x = ox + rx[0] * Q3IDE_OVL_CHAR_W * 1.6f + ux[0] * lbl_u;
			float t2y = oy + rx[1] * Q3IDE_OVL_CHAR_W * 1.6f + ux[1] * lbl_u;
			float t2z = oz + rx[2] * Q3IDE_OVL_CHAR_W * 1.6f + ux[2] * lbl_u;
			q3ide_ovl_char_sm(l2x, l2y, l2z, rx, ux, '*', 40, 200, 70);
			ovl_str_vert(t2x, t2y, t2z, rx, ux, "current", 255, 255, 255);
		}

		/* Col 3: failed (stream dead) */
		{
			float l3x = lx + rx[0] * Q3IDE_OVL_CHAR_W * 3.2f;
			float l3y = ly + rx[1] * Q3IDE_OVL_CHAR_W * 3.2f;
			float l3z = lz + rx[2] * Q3IDE_OVL_CHAR_W * 3.2f;
			float t3x = ox + rx[0] * Q3IDE_OVL_CHAR_W * 3.2f + ux[0] * lbl_u;
			float t3y = oy + rx[1] * Q3IDE_OVL_CHAR_W * 3.2f + ux[1] * lbl_u;
			float t3z = oz + rx[2] * Q3IDE_OVL_CHAR_W * 3.2f + ux[2] * lbl_u;
			q3ide_ovl_char_sm(l3x, l3y, l3z, rx, ux, '*', 255, 30, 30);
			ovl_str_vert(t3x, t3y, t3z, rx, ux, "failed", 255, 255, 255);
		}

		/* Col 4: idle (stream not sending frames) */
		{
			float l4x = lx + rx[0] * Q3IDE_OVL_CHAR_W * 4.8f;
			float l4y = ly + rx[1] * Q3IDE_OVL_CHAR_W * 4.8f;
			float l4z = lz + rx[2] * Q3IDE_OVL_CHAR_W * 4.8f;
			float t4x = ox + rx[0] * Q3IDE_OVL_CHAR_W * 4.8f + ux[0] * lbl_u;
			float t4y = oy + rx[1] * Q3IDE_OVL_CHAR_W * 4.8f + ux[1] * lbl_u;
			float t4z = oz + rx[2] * Q3IDE_OVL_CHAR_W * 4.8f + ux[2] * lbl_u;
			q3ide_ovl_char_sm(l4x, l4y, l4z, rx, ux, '*', 255, 200, 0);
			ovl_str_vert(t4x, t4y, t4z, rx, ux, "idle", 255, 255, 255);
		}
	}
}
