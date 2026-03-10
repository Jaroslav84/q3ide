/*
 * q3ide_overlay.c — Left-monitor keybinding cheat sheet + stream alert panel.
 *
 * Small, macOS-style keybinding panel anchored to the far top-left of the
 * left monitor viewport. Rendered as 3D billboard bigchars quads so it
 * always faces the left monitor camera regardless of player orientation.
 * Called from Q3IDE_MultiMonitorRender for i==0 (left monitor) only.
 */

#include "q3ide_engine_hooks.h"
#include "q3ide_params.h"
#include "q3ide_win_mngr.h"
#include "q3ide_win_mngr_internal.h"
#include "q3ide_interaction.h"
#include "../qcommon/qcommon.h"
#include "../qcommon/cm_public.h"
#include "../client/client.h"
#include <string.h>

/* Character cell — sized for Q3IDE_OVL_DIST units from camera at 90° FOV.
 * Rule: apparent_px = Q3IDE_OVL_CHAR_W / Q3IDE_OVL_DIST * screen_half_px.
 * At D=10, CHAR_W=0.14 → ~13px per char on 1920px. */

static qhandle_t g_ovl_chars;

static void q3ide_ovl_init(void)
{
	if (!g_ovl_chars)
		g_ovl_chars = re.RegisterShader("gfx/2d/bigchars");
}

/*
 * Render one character as a billboard quad at world (ox,oy,oz).
 * rx = camera-right axis, ux = camera-up axis.
 */
static void q3ide_ovl_char(float ox, float oy, float oz, const float *rx, const float *ux, int ch, byte r, byte g,
                           byte b)
{
	polyVert_t v[4];
	float col = (float) (ch & 15);
	float row = (float) (ch >> 4);
	float u0 = col / 16.0f, u1 = (col + 1.0f) / 16.0f;
	float t0 = row / 16.0f, t1 = (row + 1.0f) / 16.0f;
	static const float rsc[4] = {0.0f, Q3IDE_OVL_CHAR_W, Q3IDE_OVL_CHAR_W, 0.0f};
	static const float usc[4] = {0.0f, 0.0f, -Q3IDE_OVL_CHAR_H, -Q3IDE_OVL_CHAR_H};
	const float uv_s[4] = {u0, u1, u1, u0};
	const float uv_t[4] = {t0, t0, t1, t1};
	int i;

	for (i = 0; i < 4; i++) {
		v[i].xyz[0] = ox + rx[0] * rsc[i] + ux[0] * usc[i];
		v[i].xyz[1] = oy + rx[1] * rsc[i] + ux[1] * usc[i];
		v[i].xyz[2] = oz + rx[2] * rsc[i] + ux[2] * usc[i];
		v[i].st[0] = uv_s[i];
		v[i].st[1] = uv_t[i];
		v[i].modulate.rgba[0] = r;
		v[i].modulate.rgba[1] = g;
		v[i].modulate.rgba[2] = b;
		v[i].modulate.rgba[3] = 255;
	}
	re.AddPolyToScene(g_ovl_chars, 4, v, 1);
}

/* Render string at (ox,oy,oz), stepping right by Q3IDE_OVL_CHAR_W per char. */
static void q3ide_ovl_str(float ox, float oy, float oz, const float *rx, const float *ux, const char *s, byte r, byte g,
                          byte b)
{
	int i = 0;
	for (; *s; s++, i++)
		q3ide_ovl_char(ox + rx[0] * Q3IDE_OVL_CHAR_W * i, oy + rx[1] * Q3IDE_OVL_CHAR_W * i,
		               oz + rx[2] * Q3IDE_OVL_CHAR_W * i, rx, ux, (unsigned char) *s, r, g, b);
}

/* Small variant — Q3IDE_OVL_SMALL_SCALE scale for secondary info */
static void q3ide_ovl_str_sm(float ox, float oy, float oz, const float *rx, const float *ux, const char *s, byte r,
                             byte g, byte b)
{
	float cw = Q3IDE_OVL_CHAR_W * Q3IDE_OVL_SMALL_SCALE;
	float ch = Q3IDE_OVL_CHAR_H * Q3IDE_OVL_SMALL_SCALE;
	int i = 0;
	for (; *s; s++, i++) {
		polyVert_t v[4];
		int ch_idx = (unsigned char) *s;
		float col = (float) (ch_idx & 15);
		float row = (float) (ch_idx >> 4);
		float u0 = col / 16.0f, u1 = (col + 1.0f) / 16.0f;
		float t0 = row / 16.0f, t1 = (row + 1.0f) / 16.0f;
		float bx = ox + rx[0] * cw * i;
		float by = oy + rx[1] * cw * i;
		float bz = oz + rx[2] * cw * i;
		static const float rsc[4] = {0.0f, 1.0f, 1.0f, 0.0f};
		static const float usc[4] = {0.0f, 0.0f, -1.0f, -1.0f};
		int k;
		for (k = 0; k < 4; k++) {
			v[k].xyz[0] = bx + rx[0] * cw * rsc[k] + ux[0] * ch * usc[k];
			v[k].xyz[1] = by + rx[1] * cw * rsc[k] + ux[1] * ch * usc[k];
			v[k].xyz[2] = bz + rx[2] * cw * rsc[k] + ux[2] * ch * usc[k];
			v[k].st[0] = (k == 0 || k == 3) ? u0 : u1;
			v[k].st[1] = (k < 2) ? t0 : t1;
			v[k].modulate.rgba[0] = r;
			v[k].modulate.rgba[1] = g;
			v[k].modulate.rgba[2] = b;
			v[k].modulate.rgba[3] = 255;
		}
		re.AddPolyToScene(g_ovl_chars, 4, v, 1);
	}
}

/*
 * Draw keybinding cheat sheet for the left monitor.
 * refdef_ptr is the (const refdef_t *) for the left viewport.
 * Must be called BEFORE re.RenderScene so polys enter the scene.
 */
void Q3IDE_DrawLeftOverlay(const void *refdef_ptr)
{
	const refdef_t *fd = (const refdef_t *) refdef_ptr;
	/* viewaxis[1] = LEFT in Q3; negate → right. viewaxis[2] = up. */
	const float rx[3] = {-fd->viewaxis[1][0], -fd->viewaxis[1][1], -fd->viewaxis[1][2]};
	const float ux[3] = {fd->viewaxis[2][0], fd->viewaxis[2][1], fd->viewaxis[2][2]};
	/* Far top-left: push left ~78% of 90° half-FOV, lift ~38% */
	float right_off = -Q3IDE_OVL_DIST * 0.78f;
	float up_off = Q3IDE_OVL_DIST * 0.38f;
	float ox, oy, oz;
	int i;

	/* key col (gray-white), label col (dim gray), header (very dim) */
	struct {
		const char *key;
		const char *label;
	} entries[] = {
	    {"Q3IDE", ""},   /* header */
	    {"K", "Laser"},  /* hold K → laser beams */
	    {"L", "Focus"},  /* enter Pointer Mode */
	    {"M1", "Click"}, /* click in Pointer Mode */
	    {"ESC", "Exit"}, /* exit mode */
	};
	int n = (int) (sizeof(entries) / sizeof(entries[0]));

	q3ide_ovl_init();
	if (!g_ovl_chars || !re.AddPolyToScene)
		return;

	/* Panel top-left anchor */
	ox = fd->vieworg[0] + fd->viewaxis[0][0] * Q3IDE_OVL_DIST - fd->viewaxis[1][0] * right_off + fd->viewaxis[2][0] * up_off;
	oy = fd->vieworg[1] + fd->viewaxis[0][1] * Q3IDE_OVL_DIST - fd->viewaxis[1][1] * right_off + fd->viewaxis[2][1] * up_off;
	oz = fd->vieworg[2] + fd->viewaxis[0][2] * Q3IDE_OVL_DIST - fd->viewaxis[1][2] * right_off + fd->viewaxis[2][2] * up_off;

	for (i = 0; i < n; i++) {
		float lx = ox - ux[0] * Q3IDE_OVL_LINE_H * i;
		float ly = oy - ux[1] * Q3IDE_OVL_LINE_H * i;
		float lz = oz - ux[2] * Q3IDE_OVL_LINE_H * i;

		if (i == 0) {
			/* Header: very dim gray */
			q3ide_ovl_str(lx, ly, lz, rx, ux, entries[i].key, 65, 65, 65);
		} else {
			/* Key: medium gray (macOS key cap feel) */
			q3ide_ovl_str(lx, ly, lz, rx, ux, entries[i].key, 190, 190, 190);
			/* Label: dim gray, offset past fixed key column */
			if (entries[i].label[0]) {
				float labx = lx + rx[0] * (Q3IDE_OVL_KEY_W + Q3IDE_OVL_GAP);
				float laby = ly + rx[1] * (Q3IDE_OVL_KEY_W + Q3IDE_OVL_GAP);
				float labz = lz + rx[2] * (Q3IDE_OVL_KEY_W + Q3IDE_OVL_GAP);
				q3ide_ovl_str(labx, laby, labz, rx, ux, entries[i].label, 95, 95, 95);
			}
		}
	}

	/* Room/area display: cluster + area below the keybinding panel */
	if (cls.state == CA_ACTIVE) {
		int leafnum = CM_PointLeafnum(cl.snap.ps.origin);
		int cluster = CM_LeafCluster(leafnum);
		int area = CM_LeafArea(leafnum);
		char room_buf[32];
		float rx_off = n * Q3IDE_OVL_LINE_H + Q3IDE_OVL_LINE_H * 0.5f; /* one gap below last entry */
		float rlx = ox - ux[0] * rx_off;
		float rly = oy - ux[1] * rx_off;
		float rlz = oz - ux[2] * rx_off;
		Com_sprintf(room_buf, sizeof(room_buf), "area %d  cls %d", area, cluster);
		q3ide_ovl_str(rlx, rly, rlz, rx, ux, room_buf, 100, 200, 255);
	}

	/* Window list: active capture windows below area line (small text) */
	{
		float sm_lh = Q3IDE_OVL_LINE_H * Q3IDE_OVL_SMALL_SCALE; /* tighter line pitch for small text */
		float wl_base = n * Q3IDE_OVL_LINE_H + Q3IDE_OVL_LINE_H * 1.8f;
		int wrow = 0;
		int wi;

		/* Section header — "WINS 8 +2 s6" (active + pending, stream count) */
		{
			char hdr[32];
			int pending = Q3IDE_WM_PendingCount();
			int streams = 0, _si;
			for (_si = 0; _si < Q3IDE_MAX_WIN; _si++)
				if (q3ide_wm.wins[_si].active && q3ide_wm.wins[_si].owns_stream &&
				    q3ide_wm.wins[_si].stream_active)
					streams++;
			float lx = ox - ux[0] * wl_base;
			float ly = oy - ux[1] * wl_base;
			float lz = oz - ux[2] * wl_base;
			if (pending > 0)
				Com_sprintf(hdr, sizeof(hdr), "WINS %d+%d s%d", q3ide_wm.num_active, pending, streams);
			else
				Com_sprintf(hdr, sizeof(hdr), "WINS %d s%d", q3ide_wm.num_active, streams);
			q3ide_ovl_str(lx, ly, lz, rx, ux, hdr, 255, 255, 255);
		}
		wrow++;

		for (wi = 0; wi < Q3IDE_MAX_WIN; wi++) {
			q3ide_win_t *w = &q3ide_wm.wins[wi];
			char entry[22];
			int slen, k;
			float lx, ly, lz, row_off;

			if (!w->active)
				continue;

			/* Truncate label to 20 chars */
			slen = (int) strlen(w->label);
			if (slen > 20) {
				for (k = 0; k < 19; k++)
					entry[k] = w->label[k];
				entry[19] = '~';
				entry[20] = '\0';
			} else {
				for (k = 0; k <= slen; k++)
					entry[k] = w->label[k];
			}

			row_off = wl_base + wrow * sm_lh;
			lx = ox - ux[0] * row_off;
			ly = oy - ux[1] * row_off;
			lz = oz - ux[2] * row_off;

			/* ── Two indicator lamps before the label ─────────────────────────
			 * Lamp 1: ever_failed  — red=yes  green=never had issues
			 * Lamp 2: failing now  — red=yes  green=stream healthy right now
			 *   "failing now" = stream dead OR throttled within last 2s
			 * ──────────────────────────────────────────────────────────── */
			{
				unsigned long long now_ov = Sys_Milliseconds();
				float lamp_cw = Q3IDE_OVL_CHAR_W * Q3IDE_OVL_SMALL_SCALE;
				/* Lamp 1 — ever failed */
				float l1x = lx, l1y = ly, l1z = lz;
				/* Lamp 2 — failing now */
				float l2x = lx + rx[0] * lamp_cw * 1.5f;
				float l2y = ly + rx[1] * lamp_cw * 1.5f;
				float l2z = lz + rx[2] * lamp_cw * 1.5f;
				/* Label — shifted right past the two lamps */
				float llx = lx + rx[0] * lamp_cw * 3.2f;
				float lly = ly + rx[1] * lamp_cw * 3.2f;
				float llz = lz + rx[2] * lamp_cw * 3.2f;
				qboolean failing_now =
				    (w->owns_stream && !w->stream_active) ||
				    (w->last_throttle_ms > 0 && (now_ov - w->last_throttle_ms) < 2000ULL);

				if (w->ever_failed)
					q3ide_ovl_str_sm(l1x, l1y, l1z, rx, ux, "*", 255, 50, 50);
				else
					q3ide_ovl_str_sm(l1x, l1y, l1z, rx, ux, "*", 50, 220, 80);

				if (failing_now)
					q3ide_ovl_str_sm(l2x, l2y, l2z, rx, ux, "*", 255, 30, 30);
				else
					q3ide_ovl_str_sm(l2x, l2y, l2z, rx, ux, "*", 50, 220, 80);

				/* Label */
				if (wi == q3ide_interaction.focused_win)
					q3ide_ovl_str_sm(llx, lly, llz, rx, ux, entry, 255, 230, 140);
				else if (w->owns_stream && !w->stream_active)
					q3ide_ovl_str_sm(llx, lly, llz, rx, ux, entry, 255, 80, 40);
				else
					q3ide_ovl_str_sm(llx, lly, llz, rx, ux, entry, 210, 210, 210);
			}
			wrow++;
		}
	}

	/* Stream failure alerts — bright red banner for any dead tunnel streams */
	{
		int dead = 0, wi;
		for (wi = 0; wi < Q3IDE_MAX_WIN; wi++) {
			q3ide_win_t *w = &q3ide_wm.wins[wi];
			if (w->active && w->owns_stream && !w->stream_active)
				dead++;
		}
		if (dead > 0) {
			char alert[32];
			/* Position below the window list, centered-ish */
			float al_off = n * Q3IDE_OVL_LINE_H + Q3IDE_OVL_LINE_H * 10.0f;
			float alx = ox - ux[0] * al_off;
			float aly = oy - ux[1] * al_off;
			float alz = oz - ux[2] * al_off;
			Com_sprintf(alert, sizeof(alert), "! %d STREAM%s DEAD", dead, dead == 1 ? "" : "S");
			q3ide_ovl_str(alx, aly, alz, rx, ux, alert, 255, 50, 50);
		}
	}

	/* Hover label: show hovered window/entity name at top-right of left monitor */
	{
		const char *lbl = NULL;
		int fw = q3ide_interaction.focused_win;
		if (fw >= 0 && fw < Q3IDE_MAX_WIN && q3ide_wm.wins[fw].active && q3ide_wm.wins[fw].label[0])
			lbl = q3ide_wm.wins[fw].label;
		else if (q3ide_interaction.hovered_entity_name[0])
			lbl = q3ide_interaction.hovered_entity_name;
		if (lbl) {
			int len = (int) strlen(lbl);
			float r2 = Q3IDE_OVL_DIST * 0.78f;
			float hx =
			    fd->vieworg[0] + fd->viewaxis[0][0] * Q3IDE_OVL_DIST - fd->viewaxis[1][0] * r2 + fd->viewaxis[2][0] * up_off;
			float hy =
			    fd->vieworg[1] + fd->viewaxis[0][1] * Q3IDE_OVL_DIST - fd->viewaxis[1][1] * r2 + fd->viewaxis[2][1] * up_off;
			float hz =
			    fd->vieworg[2] + fd->viewaxis[0][2] * Q3IDE_OVL_DIST - fd->viewaxis[1][2] * r2 + fd->viewaxis[2][2] * up_off;
			hx -= rx[0] * Q3IDE_OVL_CHAR_W * len;
			hy -= rx[1] * Q3IDE_OVL_CHAR_W * len;
			hz -= rx[2] * Q3IDE_OVL_CHAR_W * len;
			/* amber for game entities, warm white for windows */
			if (q3ide_interaction.hovered_entity_name[0] && lbl == q3ide_interaction.hovered_entity_name)
				q3ide_ovl_str(hx, hy, hz, rx, ux, lbl, 255, 220, 80);
			else
				q3ide_ovl_str(hx, hy, hz, rx, ux, lbl, 220, 200, 120);
		}
	}
}
