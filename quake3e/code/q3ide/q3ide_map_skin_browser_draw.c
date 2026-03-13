/* q3ide_map_skin_browser_draw.c — Map/skin browser 3D billboard rendering. */

#include "q3ide_map_skin_browser.h"
#include "q3ide_engine_hooks.h"
#include "q3ide_log.h"
#include "q3ide_params.h"
#include "../client/client.h"
#include "../client/snd_public.h"
#include "../qcommon/qcommon.h"
#include "../qcommon/q_shared.h"
#include <string.h>
#include "q3ide_main_menu_data.h"

/* Overlay helpers — q3ide_overlay.c */
extern void q3ide_ovl_init(void);
extern void q3ide_ovl_str_sc(float ox, float oy, float oz, const float *rx, const float *ux, const char *s, float scale,
                             byte r, byte g, byte b);
extern void q3ide_ovl_pixel_pos(const refdef_t *fd, float px, float py, float out[3]);
extern qhandle_t g_ovl_chars;

/* State — q3ide_map_skin_browser.c */
extern int g_open;
extern int g_root_sel;
extern int g_map_sel, g_map_scroll;
extern int g_skin_sel, g_skin_scroll;
extern qhandle_t g_bg_shader;

#define MENU_SCALE   1.8f
#define TITLE_SCALE  2.2f
#define CW           (Q3IDE_OVL_CHAR_W * MENU_SCALE)
#define LH           (Q3IDE_OVL_CHAR_H * MENU_SCALE * 1.3f)
#define VISIBLE_ROWS 14

/*
 * Draw a solid-colour background rectangle in world space.
 * top_left is the world origin; rx/ux are right/up axes.
 */
static void draw_bg_rect(float ox, float oy, float oz, const float *rx, const float *ux, float w, float h, byte r,
                         byte g, byte b, byte a)
{
	polyVert_t v[4];
	int i;
	const float rs[4] = {0.0f, w, w, 0.0f};
	const float us[4] = {0.0f, 0.0f, -h, -h};

	for (i = 0; i < 4; i++) {
		v[i].xyz[0] = ox + rx[0] * rs[i] + ux[0] * us[i];
		v[i].xyz[1] = oy + rx[1] * rs[i] + ux[1] * us[i];
		v[i].xyz[2] = oz + rx[2] * rs[i] + ux[2] * us[i];
		v[i].st[0] = 0.5f;
		v[i].st[1] = 0.5f;
		v[i].modulate.rgba[0] = r;
		v[i].modulate.rgba[1] = g;
		v[i].modulate.rgba[2] = b;
		v[i].modulate.rgba[3] = a;
	}
	if (!g_bg_shader)
		g_bg_shader = re.RegisterShader("q3ide/bg");
	if (g_bg_shader)
		re.AddPolyToScene(g_bg_shader, 4, v, 1);
}

/* Per-colour ROW helpers — COL_* expand to 3 tokens which confuse argument
 * counting in a generic macro, so each colour gets its own wrapper. */
#define _ROW(n, s, sc, r, g, b)                                                                                        \
	q3ide_ovl_str_sc(ox + ux[0] * (-(n) * LH), oy + ux[1] * (-(n) * LH), oz + ux[2] * (-(n) * LH), rx, ux, (s), (sc),  \
	                 (r), (g), (b))
#define ROW_Y(n, s, sc) _ROW(n, s, sc, 255, 220, 30)
#define ROW_W(n, s, sc) _ROW(n, s, sc, 255, 255, 255)
#define ROW_H(n, s, sc) _ROW(n, s, sc, 100, 200, 255)
#define ROW_D(n, s, sc) _ROW(n, s, sc, 80, 80, 80)

void Q3IDE_MMenu_Draw(const void *refdef_ptr)
{
	const refdef_t *fd = (const refdef_t *) refdef_ptr;
	float origin[3], rx[3], ux[3];
	float ox, oy, oz;
	int i, row;
	char buf[64];
	float menu_w, menu_h;

	if (!g_open)
		return;
	if (!re.AddPolyToScene)
		return;

	q3ide_ovl_init();

	/* Camera right/up axes */
	rx[0] = -fd->viewaxis[1][0];
	rx[1] = -fd->viewaxis[1][1];
	rx[2] = -fd->viewaxis[1][2];
	ux[0] = fd->viewaxis[2][0];
	ux[1] = fd->viewaxis[2][1];
	ux[2] = fd->viewaxis[2][2];

	/* Anchor: top-left corner of the menu, centred on screen */
	menu_w = CW * 36;
	menu_h = LH * (VISIBLE_ROWS + 5);
	q3ide_ovl_pixel_pos(fd, fd->width * 0.5f, fd->height * 0.5f, origin);
	ox = origin[0] - rx[0] * menu_w * 0.5f + ux[0] * menu_h * 0.5f;
	oy = origin[1] - rx[1] * menu_w * 0.5f + ux[1] * menu_h * 0.5f;
	oz = origin[2] - rx[2] * menu_w * 0.5f + ux[2] * menu_h * 0.5f;

	/* Background */
	draw_bg_rect(ox - CW * 0.5f, oy - CW * 0.5f, oz - CW * 0.5f, rx, ux, menu_w + CW, menu_h + CW, 10, 10, 20, 230);

	row = 0;

	/* Title */
	ROW_Y(row, "== Q3IDE MENU ==", TITLE_SCALE);
	row += 2;

	/* ── Root page ── */
	if (g_open == 1) {
		const char *labels[2] = {"MAPS", "SKINS"};
		for (i = 0; i < 2; i++) {
			float lx = ox + rx[0] * CW * 2 + ux[0] * (-(float) row * LH);
			float ly = oy + rx[1] * CW * 2 + ux[1] * (-(float) row * LH);
			float lz = oz + rx[2] * CW * 2 + ux[2] * (-(float) row * LH);
			if (i == g_root_sel) {
				ROW_Y(row, ">", MENU_SCALE);
				q3ide_ovl_str_sc(lx, ly, lz, rx, ux, labels[i], MENU_SCALE, 255, 220, 30);
			} else {
				q3ide_ovl_str_sc(lx, ly, lz, rx, ux, labels[i], MENU_SCALE, 255, 255, 255);
			}
			row++;
		}
		row++;
		ROW_D(row, "  UP/DOWN  ENTER=SELECT  ESC=CLOSE", MENU_SCALE * 0.6f);
		return;
	}

	/* ── Maps page ── */
	if (g_open == 2) {
		ROW_Y(row, "MAPS", MENU_SCALE);
		row++;
		for (i = g_map_scroll; i < K_MAPS_N && i < g_map_scroll + VISIBLE_ROWS; i++) {
			const map_entry_t *e = &k_maps[i];
			if (e->bsp == NULL) {
				ROW_H(row, e->label ? e->label : "", MENU_SCALE * 0.8f);
			} else {
				const char *lbl = e->label ? e->label : e->bsp;
				if (i == g_map_sel) {
					Q_strncpyz(buf, "> ", sizeof(buf));
					Q_strcat(buf, sizeof(buf), lbl);
					ROW_Y(row, buf, MENU_SCALE);
				} else {
					ROW_W(row, lbl, MENU_SCALE);
				}
			}
			row++;
		}
		row++;
		ROW_D(row, "  UP/DN  ENTER=LOAD  ESC=BACK", MENU_SCALE * 0.6f);
		return;
	}

	/* ── Skins page ── */
	if (g_open == 3) {
		ROW_Y(row, "SKINS", MENU_SCALE);
		row++;
		for (i = g_skin_scroll; i < K_SKINS_N && i < g_skin_scroll + VISIBLE_ROWS; i++) {
			const skin_entry_t *e = &k_skins[i];
			if (e->id == NULL) {
				ROW_H(row, e->label ? e->label : "", MENU_SCALE * 0.8f);
			} else {
				const char *lbl = e->label ? e->label : e->id;
				if (i == g_skin_sel) {
					Q_strncpyz(buf, "> ", sizeof(buf));
					Q_strcat(buf, sizeof(buf), lbl);
					ROW_Y(row, buf, MENU_SCALE);
				} else {
					ROW_W(row, lbl, MENU_SCALE);
				}
			}
			row++;
		}
		row++;
		ROW_D(row, "  UP/DN  ENTER=APPLY  ESC=BACK", MENU_SCALE * 0.6f);
	}
}
