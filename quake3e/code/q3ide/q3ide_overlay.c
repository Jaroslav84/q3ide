/*
 * q3ide_overlay.c — Left-monitor QWERTY keyboard UI + stream alert panel.
 * Keyboard is rebuilt ≤2 Hz into a glyph cache; replayed cheap every frame.
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

/* Forward declarations for helpers defined below */
static void q3ide_ovl_str(float ox, float oy, float oz, const float *rx, const float *ux, const char *s, byte r,
                          byte g, byte b);

/* ── HUD message banner ─────────────────────────────────────────── */

static char               g_hud_msg[64];
static unsigned long long g_hud_msg_expire_ms;

void Q3IDE_SetHudMsg(const char *msg, int duration_ms)
{
	Q_strncpyz(g_hud_msg, msg, sizeof(g_hud_msg));
	g_hud_msg_expire_ms = (unsigned long long) Sys_Milliseconds() + (unsigned long long) duration_ms;
}

void Q3IDE_DrawHudMsg(const void *refdef_ptr)
{
	const refdef_t *fd = (const refdef_t *) refdef_ptr;
	unsigned long long now_ms;
	int len;
	float ox, oy, oz, total_w;
	const float rx[3] = {-fd->viewaxis[1][0], -fd->viewaxis[1][1], -fd->viewaxis[1][2]};
	const float ux[3] = {fd->viewaxis[2][0], fd->viewaxis[2][1], fd->viewaxis[2][2]};

	if (!g_hud_msg[0])
		return;
	if (fd->rdflags & RDF_NOWORLDMODEL)
		return;

	now_ms = (unsigned long long) Sys_Milliseconds();
	if (now_ms >= g_hud_msg_expire_ms) {
		g_hud_msg[0] = '\0';
		return;
	}

	q3ide_ovl_init();
	if (!g_ovl_chars || !re.AddPolyToScene)
		return;

	len = (int) strlen(g_hud_msg);
	/* Centre horizontally: push left by half the string width */
	total_w = (float) len * Q3IDE_OVL_CHAR_W;
	ox = fd->vieworg[0] + fd->viewaxis[0][0] * Q3IDE_OVL_DIST - rx[0] * total_w * 0.5f +
	     fd->viewaxis[2][0] * Q3IDE_OVL_DIST * 0.42f;
	oy = fd->vieworg[1] + fd->viewaxis[0][1] * Q3IDE_OVL_DIST - rx[1] * total_w * 0.5f +
	     fd->viewaxis[2][1] * Q3IDE_OVL_DIST * 0.42f;
	oz = fd->vieworg[2] + fd->viewaxis[0][2] * Q3IDE_OVL_DIST - rx[2] * total_w * 0.5f +
	     fd->viewaxis[2][2] * Q3IDE_OVL_DIST * 0.42f;

	q3ide_ovl_str(ox, oy, oz, rx, ux, g_hud_msg, 255, 220, 80); /* amber */
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

/* Layout tables + glyph cache statics + classify/label helpers */
#include "q3ide_overlay_keys.h"

static void q3ide_rebuild_keyboard_cache(void)
{
	static const float ktw     = 10.0f * Q3IDE_OVL_KEY_CELL;
	static const float lbl_off = 10.0f * Q3IDE_OVL_KEY_CELL * 0.5f + 0.5f;
	int i, j;

	g_glyph_count = 0;

	for (i = 0; i < 4; i++) {
		float br = -ktw * 0.5f + krow_indent[i] * Q3IDE_OVL_KEY_CELL;
		float bu = -(float) i * Q3IDE_OVL_KEY_ROW_H;
		for (j = 0; j < krow_len[i]; j++) {
			float kr = br + (float) j * Q3IDE_OVL_KEY_CELL;
			key_state_t ks = classify(krows[i][j].keynum);
			if (ks == KEY_Q3IDE) {
				float x;
				ovl_emit(kr, bu, (unsigned char) krows[i][j].display, 220, 220, 220);
				for (x = kr + Q3IDE_OVL_KEY_CELL; x < lbl_off - Q3IDE_OVL_CHAR_W; x += Q3IDE_OVL_CHAR_W * 0.9f)
					ovl_emit(x, bu, '-', 70, 70, 70);
				ovl_emit_str(lbl_off, bu, q3ide_label(krows[i][j].keynum), 160, 160, 160);
			} else if (ks == KEY_QUAKE) {
				ovl_emit(kr, bu, (unsigned char) krows[i][j].display, 190, 155, 25);
			} else {
				ovl_emit(kr, bu, (unsigned char) krows[i][j].display, 25, 25, 25);
			}
		}
	}

	for (i = 0; i < 4; i++) {
		float kr = kextras[i].col * Q3IDE_OVL_KEY_CELL - ktw * 0.5f;
		float ku = -kextras[i].row * Q3IDE_OVL_KEY_ROW_H;
		key_state_t ks = classify(kextras[i].keynum);
		if (ks == KEY_Q3IDE) {
			float x;
			ovl_emit_str(kr, ku, kextras[i].disp, 220, 220, 220);
			for (x = kr + Q3IDE_OVL_KEY_CELL; x < lbl_off - Q3IDE_OVL_CHAR_W; x += Q3IDE_OVL_CHAR_W * 0.9f)
				ovl_emit(x, ku, '-', 70, 70, 70);
			ovl_emit_str(lbl_off, ku, q3ide_label(kextras[i].keynum), 160, 160, 160);
		} else if (ks == KEY_QUAKE) {
			ovl_emit_str(kr, ku, kextras[i].disp, 190, 155, 25);
		} else {
			ovl_emit_str(kr, ku, kextras[i].disp, 25, 25, 25);
		}
	}
}

/*
 * Draw QWERTY keyboard UI + stream panel for the left monitor.
 * Must be called before re.RenderScene so polys enter the scene.
 */
void Q3IDE_DrawLeftOverlay(const void *refdef_ptr)
{
	const refdef_t *fd = (const refdef_t *) refdef_ptr;
	const float rx[3] = {-fd->viewaxis[1][0], -fd->viewaxis[1][1], -fd->viewaxis[1][2]};
	const float ux[3] = {fd->viewaxis[2][0], fd->viewaxis[2][1], fd->viewaxis[2][2]};
	float right_off = -Q3IDE_OVL_DIST * 0.90f; /* far left edge */
	float up_off    =  Q3IDE_OVL_DIST * 0.20f;  /* slightly above center */
	float ox, oy, oz;
	unsigned long long now;
	int gi;
	float wl_base = Q3IDE_OVL_KEY_ROW_H * 7.5f; /* vertical offset to window list */

	q3ide_ovl_init();
	if (!g_ovl_chars || !re.AddPolyToScene)
		return;

	ox = fd->vieworg[0] + fd->viewaxis[0][0] * Q3IDE_OVL_DIST + rx[0] * right_off + fd->viewaxis[2][0] * up_off;
	oy = fd->vieworg[1] + fd->viewaxis[0][1] * Q3IDE_OVL_DIST + rx[1] * right_off + fd->viewaxis[2][1] * up_off;
	oz = fd->vieworg[2] + fd->viewaxis[0][2] * Q3IDE_OVL_DIST + rx[2] * right_off + fd->viewaxis[2][2] * up_off;

	/* Rate-limited keyboard cache rebuild (≤ MAX_LEFT_UI_RENDER_FPS) */
	now = (unsigned long long) Sys_Milliseconds();
	if (g_ovl_dirty || (now - g_ovl_last_build_ms) >= OVL_REBUILD_MS) {
		q3ide_rebuild_keyboard_cache();
		g_ovl_last_build_ms = now;
		g_ovl_dirty         = qfalse;
	}

	/* Replay glyph cache — cheap per-frame path */
	for (gi = 0; gi < g_glyph_count; gi++) {
		const ovl_rel_glyph_t *gp = &g_glyphs[gi];
		float cx = ox + rx[0] * gp->right + ux[0] * gp->up;
		float cy = oy + rx[1] * gp->right + ux[1] * gp->up;
		float cz = oz + rx[2] * gp->right + ux[2] * gp->up;
		q3ide_ovl_char(cx, cy, cz, rx, ux, gp->ch, gp->r, gp->g, gp->b);
	}

	/* Room/area display below keyboard */
	if (cls.state == CA_ACTIVE) {
		int leafnum = CM_PointLeafnum(cl.snap.ps.origin);
		char room_buf[32];
		float rx_off = Q3IDE_OVL_KEY_ROW_H * 6.5f;
		float rlx = ox - ux[0] * rx_off, rly = oy - ux[1] * rx_off, rlz = oz - ux[2] * rx_off;
		Com_sprintf(room_buf, sizeof(room_buf), "area %d cls %d", CM_LeafArea(leafnum), CM_LeafCluster(leafnum));
		q3ide_ovl_str(rlx, rly, rlz, rx, ux, room_buf, 100, 200, 255);
	}

	/* Window list: active capture windows (small text) */
	{
		float sm_lh = Q3IDE_OVL_LINE_H * Q3IDE_OVL_SMALL_SCALE;
		int wrow = 0, wi;
		{ /* Section header */
			char hdr[48];
			int pending = Q3IDE_WM_PendingCount(), streams = 0, nd = 0, _si;
			float lx = ox - ux[0] * wl_base, ly = oy - ux[1] * wl_base, lz = oz - ux[2] * wl_base;
			for (_si = 0; _si < Q3IDE_MAX_WIN; _si++) {
				const q3ide_win_t *_w = &q3ide_wm.wins[_si];
				if (_w->active && _w->owns_stream) {
					if (_w->stream_active) streams++; else nd++;
				}
			}
			if (pending > 0 && nd > 0)
				Com_sprintf(hdr, sizeof(hdr), "WINS %d+%d s%d nd%d", q3ide_wm.num_active, pending, streams, nd);
			else if (pending > 0)
				Com_sprintf(hdr, sizeof(hdr), "WINS %d+%d s%d", q3ide_wm.num_active, pending, streams);
			else if (nd > 0)
				Com_sprintf(hdr, sizeof(hdr), "WINS %d s%d nd%d", q3ide_wm.num_active, streams, nd);
			else
				Com_sprintf(hdr, sizeof(hdr), "WINS %d s%d", q3ide_wm.num_active, streams);
			q3ide_ovl_str(lx, ly, lz, rx, ux, hdr, 255, 255, 255);
		}
		wrow++;

		for (wi = 0; wi < Q3IDE_MAX_WIN; wi++) {
			q3ide_win_t *w = &q3ide_wm.wins[wi];
			char entry[22];
			float row_off, lx, ly, lz;

			if (!w->active)
				continue;

			/* Truncate label to fit entry buffer */
			Q_strncpyz(entry, w->label, sizeof(entry));
			if (strlen(w->label) > 20) { entry[19] = '~'; entry[20] = '\0'; }

			row_off = wl_base + wrow * sm_lh;
			lx = ox - ux[0] * row_off;
			ly = oy - ux[1] * row_off;
			lz = oz - ux[2] * row_off;

			/* ── Two indicator lamps ───────────────────────────────────── *
			 * Lamp 1: ever_failed  — red=yes  green=never had issues       *
			 * Lamp 2: failing now  — red=yes  green=stream healthy          */
			{
				float lamp_cw = Q3IDE_OVL_CHAR_W * Q3IDE_OVL_SMALL_SCALE;
				float l2x = lx + rx[0] * lamp_cw * 1.5f, l2y = ly + rx[1] * lamp_cw * 1.5f, l2z = lz + rx[2] * lamp_cw * 1.5f;
				float llx = lx + rx[0] * lamp_cw * 3.2f, lly = ly + rx[1] * lamp_cw * 3.2f, llz = lz + rx[2] * lamp_cw * 3.2f;
				qboolean failing_now =
				    (w->owns_stream && !w->stream_active) ||
				    (w->last_throttle_ms > 0 && (now - w->last_throttle_ms) < 2000ULL);

				q3ide_ovl_str_sm(lx, ly, lz, rx, ux, "*",
				                 w->ever_failed ? 255 : 50, w->ever_failed ? 50 : 220, w->ever_failed ? 50 : 80);
				q3ide_ovl_str_sm(l2x, l2y, l2z, rx, ux, "*",
				                 failing_now ? 255 : 50, failing_now ? 30 : 220, failing_now ? 30 : 80);
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

	/* Stream failure alert */
	{
		int dead = 0, wi;
		for (wi = 0; wi < Q3IDE_MAX_WIN; wi++) {
			q3ide_win_t *w = &q3ide_wm.wins[wi];
			if (w->active && w->owns_stream && !w->stream_active) dead++;
		}
		if (dead > 0) {
			char alert[32];
			float al_off = wl_base + Q3IDE_OVL_LINE_H * 10.0f;
			float alx = ox - ux[0] * al_off, aly = oy - ux[1] * al_off, alz = oz - ux[2] * al_off;
			Com_sprintf(alert, sizeof(alert), "! %d STREAM%s DEAD", dead, dead == 1 ? "" : "S");
			q3ide_ovl_str(alx, aly, alz, rx, ux, alert, 255, 50, 50);
		}
	}

	/* Hover label: show focused/hovered name at top-right of left monitor */
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
			if (q3ide_interaction.hovered_entity_name[0] && lbl == q3ide_interaction.hovered_entity_name)
				q3ide_ovl_str(hx, hy, hz, rx, ux, lbl, 255, 220, 80);
			else
				q3ide_ovl_str(hx, hy, hz, rx, ux, lbl, 220, 200, 120);
		}
	}
}
