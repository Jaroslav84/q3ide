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
extern void q3ide_ovl_str(float ox, float oy, float oz, const float *rx, const float *ux, const char *s, byte r,
                           byte g, byte b);
extern void q3ide_ovl_str_sm(float ox, float oy, float oz, const float *rx, const float *ux, const char *s, byte r,
                              byte g, byte b);

/* ── Area label ──────────────────────────────────────────────────────── */

void Q3IDE_DrawAreaLabel(float ox, float oy, float oz, const float *rx, const float *ux, float kb_bot)
{
	if (cls.state != CA_ACTIVE)
		return;
	{
		int  leafnum = CM_PointLeafnum(cl.snap.ps.origin);
		char room_buf[32];
		float area_u = kb_bot - 0.12f;
		float alx    = ox + ux[0] * area_u;
		float aly    = oy + ux[1] * area_u;
		float alz    = oz + ux[2] * area_u;
		Com_sprintf(room_buf, sizeof(room_buf), "area %d cls %d", CM_LeafArea(leafnum), CM_LeafCluster(leafnum));
		q3ide_ovl_str(alx, aly, alz, rx, ux, room_buf, 100, 200, 255);
	}
}

/* ── Window-list footer: failure alert + three-column lamp legend ─────── */

/* wl_bot: bottom anchor of the window list (items grow upward).
 * Footer renders BELOW wl_bot: legend at wl_bot-sm_lh, alert above topmost entry. */
void Q3IDE_DrawWinListFooter(float ox, float oy, float oz, const float *rx, const float *ux, float wl_bot, float sm_lh,
                              int wrow)
{
	int wi;

	/* Stream failure alert — above the topmost entry */
	{
		int dead = 0;
		for (wi = 0; wi < Q3IDE_MAX_WIN; wi++) {
			q3ide_win_t *w = &q3ide_wm.wins[wi];
			if (w->active && w->owns_stream && !w->stream_active)
				dead++;
		}
		if (dead > 0) {
			char  alert[32];
			float al_u = wl_bot + (float)(wrow + 1) * sm_lh;
			float alx  = ox + ux[0] * al_u;
			float aly  = oy + ux[1] * al_u;
			float alz  = oz + ux[2] * al_u;
			Com_sprintf(alert, sizeof(alert), "! %d STREAM%s DEAD", dead, dead == 1 ? "" : "S");
			q3ide_ovl_str(alx, aly, alz, rx, ux, alert, 255, 50, 50);
		}
	}

	/* Lamp legend — below header, at the very bottom.
	 * Each star has a -90° rotated label above it (rot_rx=-ux, rot_ux=rx). */
	{
		float leg_u  = wl_bot - sm_lh;
		float lbl_u  = leg_u + Q3IDE_OVL_LABEL_ABOVE; /* anchor above star */
		float cw     = Q3IDE_OVL_CHAR_W * Q3IDE_OVL_SMALL_SCALE;
		float lx     = ox + ux[0] * leg_u;
		float ly     = oy + ux[1] * leg_u;
		float lz     = oz + ux[2] * leg_u;
		float rot_rx[3], rot_ux[3];
		int   k;
		for (k = 0; k < 3; k++) {
			rot_rx[k] = -ux[k]; /* -90°: label steps downward */
			rot_ux[k] = rx[k];
		}

		/* Lamp 1: ever_failed — star + "evr" above */
		q3ide_ovl_char_sm(lx, ly, lz, rx, ux, '*', 40, 200, 70);
		{
			float ax = ox + rx[0] * 0.0f + ux[0] * lbl_u;
			float ay = oy + rx[1] * 0.0f + ux[1] * lbl_u;
			float az = oz + rx[2] * 0.0f + ux[2] * lbl_u;
			const char *s = "evr";
			for (; *s; s++) {
				q3ide_ovl_char_sm(ax, ay, az, rot_rx, rot_ux, (unsigned char)*s, 120, 120, 120);
				ax -= ux[0] * cw; ay -= ux[1] * cw; az -= ux[2] * cw;
			}
		}

		/* Lamp 2: failing_now — star + "now" above */
		{
			float l2x = lx + rx[0] * Q3IDE_OVL_CHAR_W * 1.6f;
			float l2y = ly + rx[1] * Q3IDE_OVL_CHAR_W * 1.6f;
			float l2z = lz + rx[2] * Q3IDE_OVL_CHAR_W * 1.6f;
			q3ide_ovl_char_sm(l2x, l2y, l2z, rx, ux, '*', 40, 200, 70);
			{
				float ax = l2x + rx[0] * 0.0f + ux[0] * (lbl_u - leg_u);
				float ay = l2y + rx[1] * 0.0f + ux[1] * (lbl_u - leg_u);
				float az = l2z + rx[2] * 0.0f + ux[2] * (lbl_u - leg_u);
				const char *s = "now";
				for (; *s; s++) {
					q3ide_ovl_char_sm(ax, ay, az, rot_rx, rot_ux, (unsigned char)*s, 120, 120, 120);
					ax -= ux[0] * cw; ay -= ux[1] * cw; az -= ux[2] * cw;
				}
			}
		}

		/* Lamp 3: stream state — three colored stars + "stm" above */
		{
			float l3x = lx + rx[0] * Q3IDE_OVL_CHAR_W * 3.2f;
			float l3y = ly + rx[1] * Q3IDE_OVL_CHAR_W * 3.2f;
			float l3z = lz + rx[2] * Q3IDE_OVL_CHAR_W * 3.2f;
			float ax  = l3x + rx[0] * Q3IDE_OVL_CHAR_W * 0.7f;
			float ay  = l3y + rx[1] * Q3IDE_OVL_CHAR_W * 0.7f;
			float az  = l3z + rx[2] * Q3IDE_OVL_CHAR_W * 0.7f;
			float bx  = ax + rx[0] * Q3IDE_OVL_CHAR_W * 0.7f;
			float by_ = ay + rx[1] * Q3IDE_OVL_CHAR_W * 0.7f;
			float bz  = az + rx[2] * Q3IDE_OVL_CHAR_W * 0.7f;
			q3ide_ovl_char_sm(l3x, l3y, l3z, rx, ux, '*', 40, 220, 80);
			q3ide_ovl_char_sm(ax, ay, az, rx, ux, '*', 255, 200, 0);
			q3ide_ovl_char_sm(bx, by_, bz, rx, ux, '*', 255, 30, 30);
			{
				/* "stm" above the middle star */
				float mx = ax + ux[0] * (lbl_u - leg_u);
				float my = ay + ux[1] * (lbl_u - leg_u);
				float mz = az + ux[2] * (lbl_u - leg_u);
				const char *s = "stm";
				for (; *s; s++) {
					q3ide_ovl_char_sm(mx, my, mz, rot_rx, rot_ux, (unsigned char)*s, 120, 120, 120);
					mx -= ux[0] * cw; my -= ux[1] * cw; mz -= ux[2] * cw;
				}
			}
		}
	}
}
