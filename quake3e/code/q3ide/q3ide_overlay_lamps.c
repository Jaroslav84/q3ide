/*
 * q3ide_overlay_lamps.c — Area label + window-list footer (failure alert + lamp legend).
 * Called from Q3IDE_DrawAreaLabel / Q3IDE_DrawWinListFooter.
 */

#include "q3ide_engine_hooks.h"
#include "q3ide_params.h"
#include "q3ide_params_theme.h"
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

void Q3IDE_DrawAreaLabel(float ox, float oy, float oz, const float *rx, const float *ux)
{
	if (cls.state != CA_ACTIVE)
		return;
	{
		int leafnum = CM_PointLeafnum(cl.snap.ps.origin);
		char room_buf[32];
		Com_sprintf(room_buf, sizeof(room_buf), "area %d cls %d", CM_LeafArea(leafnum), CM_LeafCluster(leafnum));
		q3ide_ovl_str(ox, oy, oz, rx, ux, room_buf, Q3IDE_CLR_AREA_LABEL);
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
			char alert[32];
			float al_u = wl_bot + (float) (wrow + 1) * sm_lh;
			float alx = ox + ux[0] * al_u;
			float aly = oy + ux[1] * al_u;
			float alz = oz + ux[2] * al_u;
			Com_sprintf(alert, sizeof(alert), "! %d STREAM%s DEAD", dead, dead == 1 ? "" : "S");
			q3ide_ovl_str(alx, aly, alz, rx, ux, alert, Q3IDE_CLR_ALERT_RED);
		}
	}

	/* Lamp legend — one row below the window list.
	 * Labels are +90° rotated (rot_rx=ux → steps upward) and start 3 chars
	 * below the star so the text ends at star level with no overlap above. */
	{
		float leg_u = wl_bot - sm_lh;
		float cw = Q3IDE_OVL_CHAR_W * Q3IDE_OVL_SMALL_SCALE;
		float lbl_off = -3.0f * cw; /* start below star; text ascends to star level */
		float lx = ox + ux[0] * leg_u;
		float ly = oy + ux[1] * leg_u;
		float lz = oz + ux[2] * leg_u;
		float rot_rx[3], rot_ux[3];
		int k;
		for (k = 0; k < 3; k++) {
			rot_rx[k] = ux[k]; /* +90°: label steps upward */
			rot_ux[k] = -rx[k];
		}

		/* Lamp 1: ever_failed — star + "evr" in same column, ascending from below */
		q3ide_ovl_char_sm(lx, ly, lz, rx, ux, '*', Q3IDE_CLR_LAMP_OK);
		{
			float ax = lx + ux[0] * lbl_off;
			float ay = ly + ux[1] * lbl_off;
			float az = lz + ux[2] * lbl_off;
			const char *s = "evr";
			for (; *s; s++, ax += ux[0] * cw, ay += ux[1] * cw, az += ux[2] * cw)
				q3ide_ovl_char_sm(ax, ay, az, rot_rx, rot_ux, (unsigned char) *s, Q3IDE_CLR_LEGEND_GRAY);
		}

		/* Lamp 2: failing_now — star + "now" in same column */
		{
			float l2x = lx + rx[0] * Q3IDE_OVL_CHAR_W * 1.6f;
			float l2y = ly + rx[1] * Q3IDE_OVL_CHAR_W * 1.6f;
			float l2z = lz + rx[2] * Q3IDE_OVL_CHAR_W * 1.6f;
			q3ide_ovl_char_sm(l2x, l2y, l2z, rx, ux, '*', Q3IDE_CLR_LAMP_OK);
			{
				float ax = l2x + ux[0] * lbl_off;
				float ay = l2y + ux[1] * lbl_off;
				float az = l2z + ux[2] * lbl_off;
				const char *s = "now";
				for (; *s; s++, ax += ux[0] * cw, ay += ux[1] * cw, az += ux[2] * cw)
					q3ide_ovl_char_sm(ax, ay, az, rot_rx, rot_ux, (unsigned char) *s, Q3IDE_CLR_LEGEND_GRAY);
			}
		}

		/* Lamp 3: stream state — three colored stars + "stm" in same column */
		{
			float l3x = lx + rx[0] * Q3IDE_OVL_CHAR_W * 3.2f;
			float l3y = ly + rx[1] * Q3IDE_OVL_CHAR_W * 3.2f;
			float l3z = lz + rx[2] * Q3IDE_OVL_CHAR_W * 3.2f;
			float ctx = l3x + rx[0] * Q3IDE_OVL_CHAR_W * 0.7f; /* centre star */
			float cty = l3y + rx[1] * Q3IDE_OVL_CHAR_W * 0.7f;
			float ctz = l3z + rx[2] * Q3IDE_OVL_CHAR_W * 0.7f;
			q3ide_ovl_char_sm(l3x, l3y, l3z, rx, ux, '*', Q3IDE_CLR_LAMP_ACTIVE);
			q3ide_ovl_char_sm(ctx, cty, ctz, rx, ux, '*', Q3IDE_CLR_LAMP_IDLE);
			q3ide_ovl_char_sm(ctx + rx[0] * Q3IDE_OVL_CHAR_W * 0.7f, cty + rx[1] * Q3IDE_OVL_CHAR_W * 0.7f,
			                  ctz + rx[2] * Q3IDE_OVL_CHAR_W * 0.7f, rx, ux, '*', Q3IDE_CLR_LAMP_DEAD);
			{
				float mx = ctx + ux[0] * lbl_off;
				float my = cty + ux[1] * lbl_off;
				float mz = ctz + ux[2] * lbl_off;
				const char *s = "stm";
				for (; *s; s++, mx += ux[0] * cw, my += ux[1] * cw, mz += ux[2] * cw)
					q3ide_ovl_char_sm(mx, my, mz, rot_rx, rot_ux, (unsigned char) *s, Q3IDE_CLR_LEGEND_GRAY);
			}
		}
	}
}
