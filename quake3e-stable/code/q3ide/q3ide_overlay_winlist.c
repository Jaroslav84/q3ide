/*
 * q3ide_overlay_winlist.c — Window list panel (header + per-window health entries).
 * Area label: q3ide_overlay_lamps.c.  Footer (alert + legend): q3ide_overlay_lamps.c.
 */

#include "q3ide_engine_hooks.h"
#include "q3ide_params.h"
#include "q3ide_win_mngr.h"
#include "q3ide_win_mngr_internal.h"
#include "../qcommon/qcommon.h"
#include "../client/client.h"
#include <string.h>

extern void q3ide_ovl_char(float ox, float oy, float oz, const float *rx, const float *ux, int ch, byte r, byte g,
                            byte b);
extern void q3ide_ovl_str(float ox, float oy, float oz, const float *rx, const float *ux, const char *s, byte r,
                           byte g, byte b);
extern void q3ide_ovl_str_sm(float ox, float oy, float oz, const float *rx, const float *ux, const char *s, byte r,
                              byte g, byte b);

/* q3ide_overlay_lamps.c */
extern void Q3IDE_DrawWinListFooter(float ox, float oy, float oz, const float *rx, const float *ux, float wl_top,
                                    float sm_lh, int wrow);

/* Currently aimed window — defined in q3ide_engine.c */
extern int q3ide_aimed_win;

/* Window name label offset vs. lamp row: negative = above lamps */
#define LABEL_UP_OFFSET 0.095f /* 5px above lamp row */

/* Window list panel below area label.
 * Header: "Windows: O/A/F"
 *   O = displayed (active, placed windows)
 *   A = all macOS windows SCK knows about (stable; cached at init, ±1 on SCK events)
 *   F = failed (no stream, empty, or any error) — shown in red when > 0
 * Three health lamps per entry:
 *   Lamp 1 (ever_failed): green / red
 *   Lamp 2 (failing_now): green / red
 *   Lamp 3 (stream live): green=active  yellow=idle  red=dead
 */
void Q3IDE_DrawWinList(float ox, float oy, float oz, const float *rx, const float *ux, float kb_bot,
                       unsigned long long now)
{
	float wl_top  = kb_bot - 0.12f - Q3IDE_OVL_LINE_H * 1.6f;
	int   n_active = q3ide_wm.num_active;
	float sm_lh =
	    (n_active > 10) ? Q3IDE_OVL_LINE_H * Q3IDE_OVL_SMALL_SCALE * 0.65f : Q3IDE_OVL_LINE_H * Q3IDE_OVL_SMALL_SCALE;
	int wrow = 0, wi;

	/* Hovered window label — yellow, above the Windows header */
	if (q3ide_aimed_win >= 0 && q3ide_aimed_win < Q3IDE_MAX_WIN && q3ide_wm.wins[q3ide_aimed_win].active) {
		const char *lbl     = q3ide_wm.wins[q3ide_aimed_win].label;
		float       hover_u = wl_top + Q3IDE_OVL_LINE_H;
		float       hx      = ox + ux[0] * hover_u;
		float       hy      = oy + ux[1] * hover_u;
		float       hz      = oz + ux[2] * hover_u;
		q3ide_ovl_str(hx, hy, hz, rx, ux, lbl[0] ? lbl : "(no label)", 255, 220, 0);
	}

	/* Header: "Windows: O/A/F" — F shown in red when > 0 */
	{
		char  pfx[48];
		char  fsuf[8];
		int   n_all, n_fail, _si;
		float lx, ly, lz, fx, fy, fz;

		n_all  = q3ide_wm.macos_win_count;
		n_fail = 0;
		for (_si = 0; _si < Q3IDE_MAX_WIN; _si++) {
			const q3ide_win_t *_w = &q3ide_wm.wins[_si];
			if (_w->active && _w->owns_stream && !_w->stream_active)
				n_fail++;
		}
		/* Header 5px below wl_top reference line */
		lx = ox + ux[0] * (wl_top - 0.09f);
		ly = oy + ux[1] * (wl_top - 0.09f);
		lz = oz + ux[2] * (wl_top - 0.09f);
		Com_sprintf(pfx, sizeof(pfx), "Windows: %d/%d/", n_active, n_all);
		q3ide_ovl_str(lx, ly, lz, rx, ux, pfx, 255, 255, 255);
		fx = lx + rx[0] * (float)strlen(pfx) * Q3IDE_OVL_CHAR_W;
		fy = ly + rx[1] * (float)strlen(pfx) * Q3IDE_OVL_CHAR_W;
		fz = lz + rx[2] * (float)strlen(pfx) * Q3IDE_OVL_CHAR_W;
		Com_sprintf(fsuf, sizeof(fsuf), "%d", n_fail);
		if (n_fail > 0)
			q3ide_ovl_str(fx, fy, fz, rx, ux, fsuf, 255, 50, 50);
		else
			q3ide_ovl_str(fx, fy, fz, rx, ux, fsuf, 255, 255, 255);
	}
	wrow++;

	for (wi = 0; wi < Q3IDE_MAX_WIN; wi++) {
		q3ide_win_t *w = &q3ide_wm.wins[wi];
		char         entry[22];
		float        row_u, lx, ly, lz;
		qboolean     failing_now;

		if (!w->active)
			continue;

		Q_strncpyz(entry, w->label, sizeof(entry));
		if (strlen(w->label) > 20) {
			entry[19] = '~';
			entry[20] = '\0';
		}

		row_u = wl_top - (float)wrow * sm_lh;
		lx    = ox + ux[0] * row_u;
		ly    = oy + ux[1] * row_u;
		lz    = oz + ux[2] * row_u;

		failing_now =
		    (w->owns_stream && !w->stream_active) || (w->last_throttle_ms > 0 && (now - w->last_throttle_ms) < 2000ULL);

		/* Lamp 1: ever_failed — green / red */
		q3ide_ovl_char(lx, ly, lz, rx, ux, '*', w->ever_failed ? 255 : 40, w->ever_failed ? 50 : 200,
		               w->ever_failed ? 50 : 70);
		{
			float    l2x = lx + rx[0] * Q3IDE_OVL_CHAR_W * 1.6f;
			float    l2y = ly + rx[1] * Q3IDE_OVL_CHAR_W * 1.6f;
			float    l2z = lz + rx[2] * Q3IDE_OVL_CHAR_W * 1.6f;
			float    l3x = lx + rx[0] * Q3IDE_OVL_CHAR_W * 3.2f;
			float    l3y = ly + rx[1] * Q3IDE_OVL_CHAR_W * 3.2f;
			float    l3z = lz + rx[2] * Q3IDE_OVL_CHAR_W * 3.2f;
			float    llx = lx + rx[0] * Q3IDE_OVL_CHAR_W * 4.8f + ux[0] * LABEL_UP_OFFSET;
			float    lly = ly + rx[1] * Q3IDE_OVL_CHAR_W * 4.8f + ux[1] * LABEL_UP_OFFSET;
			float    llz = lz + rx[2] * Q3IDE_OVL_CHAR_W * 4.8f + ux[2] * LABEL_UP_OFFSET;
			qboolean stream_idle;

			/* Lamp 2: failing_now — green / red */
			q3ide_ovl_char(l2x, l2y, l2z, rx, ux, '*', failing_now ? 255 : 40, failing_now ? 30 : 200,
			               failing_now ? 30 : 70);

			/* Lamp 3: stream live state — green=active  yellow=idle  red=dead */
			stream_idle = w->stream_active && w->last_frame_ms > 0 && (now - w->last_frame_ms) >= Q3IDE_IDLE_TIMEOUT_MS;
			if (!w->stream_active)
				q3ide_ovl_char(l3x, l3y, l3z, rx, ux, '*', 255, 30, 30); /* red: dead  */
			else if (stream_idle)
				q3ide_ovl_char(l3x, l3y, l3z, rx, ux, '*', 255, 200, 0); /* yellow: idle */
			else
				q3ide_ovl_char(l3x, l3y, l3z, rx, ux, '*', 40, 220, 80); /* green: alive */

			if (w->owns_stream && !w->stream_active)
				q3ide_ovl_str_sm(llx, lly, llz, rx, ux, entry, 255, 80, 40);
			else
				q3ide_ovl_str_sm(llx, lly, llz, rx, ux, entry, 210, 210, 210);
		}
		wrow++;
	}

	Q3IDE_DrawWinListFooter(ox, oy, oz, rx, ux, wl_top, sm_lh, wrow);
}
