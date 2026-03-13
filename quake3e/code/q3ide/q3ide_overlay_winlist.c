/*
 * q3ide_overlay_winlist.c — Window list panel (header + per-window health entries).
 * Area label: q3ide_overlay_lamps.c.  Footer (alert + legend): q3ide_overlay_lamps.c.
 *
 * Layout: bottom-right corner of the left monitor screen.
 * Entries grow upward from the anchor.  Footer (lamp legend) and WINDOWS
 * header appear above the entries.
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
extern void q3ide_ovl_str(float ox, float oy, float oz, const float *rx, const float *ux, const char *s, byte r, byte g,
                          byte b);
extern void q3ide_ovl_str_sm(float ox, float oy, float oz, const float *rx, const float *ux, const char *s, byte r,
                             byte g, byte b);
extern void q3ide_ovl_pixel_pos(const refdef_t *fd, float px, float py, float out[3]);

/* q3ide_overlay_lamps.c */
extern void Q3IDE_DrawWinListFooter(float ox, float oy, float oz, const float *rx, const float *ux, float sm_lh,
                                    int wrow_start);

/* Window list panel: bottom-right corner, entries grow upward.
 * Header: "WINDOWS: O/A/Z/W"
 *   O = displayed (active windows)
 *   A = all macOS windows SCK knows about
 *   Z = idle count (yellow if > 0)
 *   W = failed (red if > 0)
 * Four health lamps per entry (left to right):
 *   Lamp 1 (ever_failed) : green / red
 *   Lamp 2 (failing_now) : green / red
 *   Lamp 3 (stream dead) : green / red
 *   Lamp 4 (stream idle) : green / yellow
 */
void Q3IDE_DrawWinList(const void *refdef_ptr, unsigned long long now)
{
	const refdef_t *fd = (const refdef_t *) refdef_ptr;
	const float rx[3] = {-fd->viewaxis[1][0], -fd->viewaxis[1][1], -fd->viewaxis[1][2]};
	const float ux[3] = {fd->viewaxis[2][0], fd->viewaxis[2][1], fd->viewaxis[2][2]};
	float anchor[3];
	float ox, oy, oz;
	int n_active, wi, wrow;
	float sm_lh;

	q3ide_ovl_pixel_pos(fd, (float) fd->width - Q3IDE_OVL_WL_RIGHT_MARGIN_PX - Q3IDE_OVL_WL_CONTENT_PX,
	                    (float) fd->height - Q3IDE_OVL_WINLIST_BOTTOM_PX, anchor);
	ox = anchor[0];
	oy = anchor[1];
	oz = anchor[2];

	n_active = q3ide_wm.num_active;
	sm_lh = (n_active > 10) ? Q3IDE_OVL_SM_LINE_H * 0.65f : Q3IDE_OVL_SM_LINE_H;
	wrow = 0;

	for (wi = 0; wi < Q3IDE_MAX_WIN; wi++) {
		q3ide_win_t *w = &q3ide_wm.wins[wi];
		char entry[22];
		float row_u, lx, ly, lz;
		qboolean failing_now;

		if (!w->active)
			continue;

		Q_strncpyz(entry, w->label, sizeof(entry));
		if (strlen(w->label) > 20) {
			entry[19] = '~';
			entry[20] = '\0';
		}

		row_u = (float) (wrow + 1) * sm_lh;
		lx = ox + ux[0] * row_u;
		ly = oy + ux[1] * row_u;
		lz = oz + ux[2] * row_u;

		failing_now =
		    (w->owns_stream && !w->stream_active) || (w->last_throttle_ms > 0 && (now - w->last_throttle_ms) < 2000ULL);

		/* Lamp 1: ever_failed — green / red */
		q3ide_ovl_char(lx, ly, lz, rx, ux, '*', w->ever_failed ? 255 : 40, w->ever_failed ? 50 : 200,
		               w->ever_failed ? 50 : 70);
		{
			float l2x = lx + rx[0] * Q3IDE_OVL_CHAR_W * 1.6f;
			float l2y = ly + rx[1] * Q3IDE_OVL_CHAR_W * 1.6f;
			float l2z = lz + rx[2] * Q3IDE_OVL_CHAR_W * 1.6f;
			float l3x = lx + rx[0] * Q3IDE_OVL_CHAR_W * 3.2f;
			float l3y = ly + rx[1] * Q3IDE_OVL_CHAR_W * 3.2f;
			float l3z = lz + rx[2] * Q3IDE_OVL_CHAR_W * 3.2f;
			float l4x = lx + rx[0] * Q3IDE_OVL_CHAR_W * 4.8f;
			float l4y = ly + rx[1] * Q3IDE_OVL_CHAR_W * 4.8f;
			float l4z = lz + rx[2] * Q3IDE_OVL_CHAR_W * 4.8f;
			float llx = lx + rx[0] * Q3IDE_OVL_CHAR_W * 6.4f;
			float lly = ly + rx[1] * Q3IDE_OVL_CHAR_W * 6.4f;
			float llz = lz + rx[2] * Q3IDE_OVL_CHAR_W * 6.4f;
			qboolean stream_fail, stream_idle;

			/* Lamp 2: failing_now — green / red */
			q3ide_ovl_char(l2x, l2y, l2z, rx, ux, '*', failing_now ? 255 : 40, failing_now ? 30 : 200,
			               failing_now ? 30 : 70);

			/* Lamp 3: stream dead — green / red */
			stream_fail = w->owns_stream && !w->stream_active;
			q3ide_ovl_char(l3x, l3y, l3z, rx, ux, '*', stream_fail ? 255 : 40, stream_fail ? 30 : 200,
			               stream_fail ? 30 : 70);

			/* Lamp 4: stream idle — green / yellow */
			stream_idle = w->stream_active && w->last_frame_ms > 0 && (now - w->last_frame_ms) >= Q3IDE_IDLE_TIMEOUT_MS;
			q3ide_ovl_char(l4x, l4y, l4z, rx, ux, '*', stream_idle ? 255 : 40, stream_idle ? 200 : 220,
			               stream_idle ? 0 : 80);

			if (w->owns_stream && !w->stream_active)
				q3ide_ovl_str_sm(llx, lly, llz, rx, ux, entry, 255, 80, 40);
			else
				q3ide_ovl_str_sm(llx, lly, llz, rx, ux, entry, 210, 210, 210);
		}
		wrow++;
	}

	Q3IDE_DrawWinListFooter(ox, oy, oz, rx, ux, sm_lh, wrow);

	/* WINDOWS header: topmost element, above footer */
	{
		int n_all, n_idle, n_fail, _si;
		char pfx[48], zsuf[8], fsuf[8];
		float hdr_u, lx, ly, lz, zx, zy, zz, fx, fy, fz;

		n_all = q3ide_wm.macos_win_count;
		n_idle = 0;
		n_fail = 0;
		for (_si = 0; _si < Q3IDE_MAX_WIN; _si++) {
			const q3ide_win_t *_w = &q3ide_wm.wins[_si];
			if (!_w->active)
				continue;
			if (_w->owns_stream && !_w->stream_active)
				n_fail++;
			if (_w->stream_active && _w->last_frame_ms > 0 && (now - _w->last_frame_ms) >= Q3IDE_IDLE_TIMEOUT_MS)
				n_idle++;
		}

		hdr_u = (float) (wrow + 7) * sm_lh;
		lx = ox + ux[0] * hdr_u;
		ly = oy + ux[1] * hdr_u;
		lz = oz + ux[2] * hdr_u;

		Com_sprintf(pfx, sizeof(pfx), "WINDOWS: %d/%d/", n_active, n_all);
		q3ide_ovl_str(lx, ly, lz, rx, ux, pfx, 255, 255, 255);

		Com_sprintf(zsuf, sizeof(zsuf), "%d/", n_idle);
		zx = lx + rx[0] * (float) strlen(pfx) * Q3IDE_OVL_CHAR_W;
		zy = ly + rx[1] * (float) strlen(pfx) * Q3IDE_OVL_CHAR_W;
		zz = lz + rx[2] * (float) strlen(pfx) * Q3IDE_OVL_CHAR_W;
		if (n_idle > 0)
			q3ide_ovl_str(zx, zy, zz, rx, ux, zsuf, 255, 200, 0);
		else
			q3ide_ovl_str(zx, zy, zz, rx, ux, zsuf, 255, 255, 255);

		Com_sprintf(fsuf, sizeof(fsuf), "%d", n_fail);
		fx = zx + rx[0] * (float) strlen(zsuf) * Q3IDE_OVL_CHAR_W;
		fy = zy + rx[1] * (float) strlen(zsuf) * Q3IDE_OVL_CHAR_W;
		fz = zz + rx[2] * (float) strlen(zsuf) * Q3IDE_OVL_CHAR_W;
		if (n_fail > 0)
			q3ide_ovl_str(fx, fy, fz, rx, ux, fsuf, 255, 50, 50);
		else
			q3ide_ovl_str(fx, fy, fz, rx, ux, fsuf, 255, 255, 255);
	}
}
