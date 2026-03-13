/*
 * q3ide_main_menu.c — Map/skin switcher (M key).
 * Tab header row at top: [MAPS] / [SKINS], L/R switches.
 * Up from list top — enter header. State persists across open/close.
 */

#include "q3ide_main_menu.h"
#include "q3ide_engine_hooks.h"
#include "q3ide_log.h"
#include "q3ide_params.h"
#include "q3ide_params_theme.h"
#include "../qcommon/qcommon.h"
#include "../client/client.h"
#include "../client/keycodes.h"
#include <string.h>

extern qhandle_t g_ovl_chars;
extern void      q3ide_ovl_init(void);
extern void      q3ide_ovl_str_sc(float, float, float, const float *, const float *, const char *, float, byte, byte,
                                  byte);
extern void      q3ide_ovl_pixel_pos(const refdef_t *, float, float, float[3]);

#include "q3ide_main_menu_data.h"

/* -- Category index -------------------------------------------------------- */

#define MAX_CATS 32
static int g_cat_starts[MAX_CATS];
static int g_n_cats = 0;

static void build_cat_index(void)
{
	int i;
	g_n_cats = 0;
	for (i = 0; i < K_MAPS_N && g_n_cats < MAX_CATS; i++)
		if (k_maps[i].bsp == NULL)
			g_cat_starts[g_n_cats++] = i;
}

static int cat_map_count(int cat)
{
	int end = (cat + 1 < g_n_cats) ? g_cat_starts[cat + 1] : K_MAPS_N;
	return end - (g_cat_starts[cat] + 1);
}

/* -- State ----------------------------------------------------------------- */

static qboolean           g_open        = qfalse;
static qboolean           g_initialized = qfalse;
static int                g_tab         = 0; /* 0=Maps, 1=Skins */
static qboolean           g_in_header   = qfalse;
static int                g_maps_mode   = 0; /* 0=categories, 1=map list */
static int                g_cat_sel     = 0;
static int                g_cat_scroll  = 0;
static int                g_map_sel     = 0;
static int                g_map_scroll  = 0;
static int                g_skin_sel    = 0;
static int                g_skin_scroll = 0;
static int                g_nav_dir     = 0;
static unsigned long long g_nav_pressed  = 0;
static unsigned long long g_nav_last_rep = 0;

static void clamp_cat(void)
{
	if (g_cat_sel < 0)
		g_cat_sel = 0;
	if (g_cat_sel >= g_n_cats)
		g_cat_sel = g_n_cats > 0 ? g_n_cats - 1 : 0;
	if (g_cat_sel < g_cat_scroll)
		g_cat_scroll = g_cat_sel;
	if (g_cat_sel >= g_cat_scroll + Q3IDE_MENU_VISIBLE_ROWS)
		g_cat_scroll = g_cat_sel - Q3IDE_MENU_VISIBLE_ROWS + 1;
	if (g_cat_scroll < 0)
		g_cat_scroll = 0;
}

static void clamp_map(void)
{
	int n = cat_map_count(g_cat_sel);
	if (g_map_sel < 0)
		g_map_sel = 0;
	if (g_map_sel >= n)
		g_map_sel = n > 0 ? n - 1 : 0;
	if (g_map_sel < g_map_scroll)
		g_map_scroll = g_map_sel;
	if (g_map_sel >= g_map_scroll + Q3IDE_MENU_VISIBLE_ROWS)
		g_map_scroll = g_map_sel - Q3IDE_MENU_VISIBLE_ROWS + 1;
	if (g_map_scroll < 0)
		g_map_scroll = 0;
}

static void clamp_skin_scroll(void)
{
	if (g_skin_sel < g_skin_scroll)
		g_skin_scroll = g_skin_sel;
	if (g_skin_sel >= g_skin_scroll + Q3IDE_MENU_VISIBLE_ROWS)
		g_skin_scroll = g_skin_sel - Q3IDE_MENU_VISIBLE_ROWS + 1;
	if (g_skin_scroll < 0)
		g_skin_scroll = 0;
}

/* -- Navigation ------------------------------------------------------------ */

static void menu_nav(int dir)
{
	if (g_in_header) {
		if (dir > 0)
			g_in_header = qfalse;
		return;
	}
	if (g_tab == 0) {
		if (g_maps_mode == 0) {
			g_cat_sel += dir;
			if (g_cat_sel < 0) {
				g_cat_sel   = 0;
				g_in_header = qtrue;
				return;
			}
			clamp_cat();
		} else {
			g_map_sel += dir;
			if (g_map_sel < 0) {
				g_map_sel    = 0;
				g_maps_mode  = 0;
				g_map_scroll = 0;
			} else {
				clamp_map();
			}
		}
	} else {
		int ns = g_skin_sel + dir;
		while (ns >= 0 && ns < K_SKINS_N && k_skins[ns].id == NULL)
			ns += dir;
		if (ns < 0) {
			g_in_header = qtrue;
			return;
		}
		if (ns >= K_SKINS_N)
			ns = g_skin_sel;
		g_skin_sel = ns;
		clamp_skin_scroll();
	}
}

/* -- Key handler ----------------------------------------------------------- */

qboolean Q3IDE_Menu_IsOpen(void) { return g_open; }

qboolean Q3IDE_Menu_OnKey(int key, qboolean down)
{
	if (key == 'm' && down) {
		Q3IDE_LOGI("menu: M key pressed, toggling menu (current g_open=%d)", (int)g_open);
		g_open = !g_open;
		if (g_open && !g_initialized) {
			build_cat_index();
			g_initialized = qtrue;
			while (g_skin_sel < K_SKINS_N && k_skins[g_skin_sel].id == NULL)
				g_skin_sel++;
		}
		g_nav_dir = 0;
		return qtrue;
	}
	if (!g_open)
		return qfalse;

	if ((key == K_LEFTARROW || key == K_RIGHTARROW) && down) {
		if (g_in_header)
			g_tab = 1 - g_tab;
		return qtrue;
	}
	if (key == K_UPARROW || key == K_DOWNARROW || key == K_MWHEELUP || key == K_MWHEELDOWN) {
		if (down) {
			int                dir = (key == K_UPARROW || key == K_MWHEELUP) ? -1 : +1;
			unsigned long long now = (unsigned long long) Sys_Milliseconds();
			if (key == K_UPARROW || key == K_DOWNARROW) {
				g_nav_dir      = dir;
				g_nav_pressed  = now;
				g_nav_last_rep = now;
			}
			menu_nav(dir);
		} else {
			if (key == K_UPARROW || key == K_DOWNARROW)
				g_nav_dir = 0;
		}
		return qtrue;
	}
	if (down) {
		if (key == K_ENTER) {
			if (g_in_header) {
				g_in_header = qfalse;
			} else if (g_tab == 0) {
				if (g_maps_mode == 0) {
					g_maps_mode  = 1;
					g_map_sel    = 0;
					g_map_scroll = 0;
					g_nav_dir    = 0;
					Q3IDE_LOGI("map menu: expanded %s", k_maps[g_cat_starts[g_cat_sel]].label);
				} else {
					int abs_idx = g_cat_starts[g_cat_sel] + 1 + g_map_sel;
					Q3IDE_LOGI("map menu: loading %s", k_maps[abs_idx].bsp);
					Cbuf_ExecuteText(EXEC_APPEND, va("devmap %s\n", k_maps[abs_idx].bsp));
					g_open    = qfalse;
					g_nav_dir = 0;
				}
			} else {
				Q3IDE_LOGI("map menu: skin %s", k_skins[g_skin_sel].id);
				Cbuf_ExecuteText(EXEC_APPEND, va("model %s\n", k_skins[g_skin_sel].id));
			}
		} else if (key == K_ESCAPE) {
			if (g_tab == 0 && g_maps_mode == 1) {
				g_maps_mode = 0;
				g_nav_dir   = 0;
			} else {
				g_open    = qfalse;
				g_nav_dir = 0;
			}
		}
	}
	return qtrue;
}

/* -- Draw ------------------------------------------------------------------ */

/* Strip "--- Foo Bar ---" -> "Foo Bar" for category display */
static void strip_cat(const char *in, char *out, int size)
{
	const char *s = in;
	int         n;
	while (*s == '-' || *s == ' ')
		s++;
	n = (int) strlen(s);
	while (n > 0 && (s[n - 1] == '-' || s[n - 1] == ' '))
		n--;
	if (n >= size)
		n = size - 1;
	memcpy(out, s, n);
	out[n] = '\0';
}

void Q3IDE_Menu_Draw(const void *refdef_ptr)
{
	const refdef_t *fd    = (const refdef_t *) refdef_ptr;
	const float     rx[3] = {fd->viewaxis[1][0], fd->viewaxis[1][1], fd->viewaxis[1][2]};
	const float     ux[3] = {fd->viewaxis[2][0], fd->viewaxis[2][1], fd->viewaxis[2][2]};
	float           lh    = Q3IDE_OVL_LINE_H * Q3IDE_MENU_LINE_SCALE;
	float           cw_sm = Q3IDE_OVL_CHAR_W * Q3IDE_MENU_FONT_SCALE;
	float           anchor[3], center[3];
	int             row, i, visible, total_rows, hint_row;

	if (!g_open || (fd->rdflags & RDF_NOWORLDMODEL))
		return;

	Q3IDE_LOGI("menu draw: tab=%d in_hdr=%d n_cats=%d w=%d h=%d", g_tab, (int)g_in_header, g_n_cats, fd->width, fd->height);

	/* Key-repeat */
	if (g_nav_dir != 0) {
		unsigned long long now  = (unsigned long long) Sys_Milliseconds();
		unsigned long long held = now - g_nav_pressed;
		if (held >= Q3IDE_MENU_REPEAT_DELAY_MS && now - g_nav_last_rep >= Q3IDE_MENU_REPEAT_RATE_MS) {
			menu_nav(g_nav_dir);
			g_nav_last_rep = now;
		}
	}

	q3ide_ovl_init();
	if (!g_ovl_chars || !re.AddPolyToScene) {
		Q3IDE_LOGI("menu draw: BLOCKED chars=%d", (int)g_ovl_chars);
		return;
	}

	/* Vertical centering: tab(1) + sep(1) + list + hint(1) */
	if (g_tab == 0)
		visible = (g_maps_mode == 0) ? g_n_cats : cat_map_count(g_cat_sel);
	else
		visible = K_SKINS_N;
	if (visible > Q3IDE_MENU_VISIBLE_ROWS)
		visible = Q3IDE_MENU_VISIBLE_ROWS;
	total_rows = visible + 3;
	hint_row   = visible + 2;
	q3ide_ovl_pixel_pos(fd, (float) fd->width * 0.5f, (float) fd->height * 0.5f, center);
	{
		float half_h = (float) (total_rows - 1) * lh * 0.5f;
		anchor[0]    = center[0] + ux[0] * half_h;
		anchor[1]    = center[1] + ux[1] * half_h;
		anchor[2]    = center[2] + ux[2] * half_h;
	}

#define DRAW_SM(str, row_i, ...)                                                            \
	do {                                                                                    \
		int   _n  = (int) strlen(str);                                                      \
		float _w  = (float) _n * cw_sm;                                                    \
		float _du = -(float) (row_i) * lh;                                                 \
		float _bx = anchor[0] - rx[0] * _w * 0.5f + ux[0] * _du;                         \
		float _by = anchor[1] - rx[1] * _w * 0.5f + ux[1] * _du;                         \
		float _bz = anchor[2] - rx[2] * _w * 0.5f + ux[2] * _du;                         \
		q3ide_ovl_str_sc(_bx, _by, _bz, rx, ux, str, Q3IDE_MENU_FONT_SCALE, __VA_ARGS__); \
	} while (0)

	/* Row 0: two-color tab header — active=amber, inactive=dark gray */
	{
		const char *ta     = (g_tab == 0) ? "[MAPS]" : " MAPS ";
		const char *tb     = (g_tab == 1) ? "[SKINS]" : " SKINS";
		int         na     = (int) strlen(ta);
		int         nb     = (int) strlen(tb);
		float       total_w = (float) (na + nb) * cw_sm;
		float       _du    = 0.0f;
		float       bx     = anchor[0] - rx[0] * total_w * 0.5f + ux[0] * _du;
		float       by     = anchor[1] - rx[1] * total_w * 0.5f + ux[1] * _du;
		float       bz     = anchor[2] - rx[2] * total_w * 0.5f + ux[2] * _du;
		byte        ra, ga, ba, rb, gb, bb;
		if (g_in_header) {
			ra = rb = 80;
			ga = gb = 255;
			ba = bb = 80;
		} else {
			ra = (g_tab == 0) ? 255 : 70;
			ga = (g_tab == 0) ? 180 : 70;
			ba = (g_tab == 0) ? 30 : 70;
			rb = (g_tab == 1) ? 255 : 70;
			gb = (g_tab == 1) ? 180 : 70;
			bb = (g_tab == 1) ? 30 : 70;
		}
		q3ide_ovl_str_sc(bx, by, bz, rx, ux, ta, Q3IDE_MENU_FONT_SCALE, ra, ga, ba);
		bx += rx[0] * (float) na * cw_sm;
		by += rx[1] * (float) na * cw_sm;
		bz += rx[2] * (float) na * cw_sm;
		q3ide_ovl_str_sc(bx, by, bz, rx, ux, tb, Q3IDE_MENU_FONT_SCALE, rb, gb, bb);
	}

	/* Row 1: separator */
	DRAW_SM("--------------------", 1, 50, 50, 50);

	/* Rows 2+: list */
	row = 2;
	if (g_tab == 0 && g_maps_mode == 0) {
		for (i = g_cat_scroll; i < g_n_cats && row < Q3IDE_MENU_VISIBLE_ROWS + 2; i++, row++) {
			char cat_buf[64];
			strip_cat(k_maps[g_cat_starts[i]].label, cat_buf, sizeof(cat_buf));
			if (i == g_cat_sel && !g_in_header) {
				char buf[64];
				Com_sprintf(buf, sizeof(buf), "> %s", cat_buf);
				DRAW_SM(buf, row, Q3IDE_CLR_MENU_SEL);
			} else {
				DRAW_SM(cat_buf, row, Q3IDE_CLR_MENU_CAT);
			}
		}
	} else if (g_tab == 0 && g_maps_mode == 1) {
		int base = g_cat_starts[g_cat_sel] + 1;
		int n    = cat_map_count(g_cat_sel);
		for (i = g_map_scroll; i < n && row < Q3IDE_MENU_VISIBLE_ROWS + 2; i++, row++) {
			const map_entry_t *e    = &k_maps[base + i];
			const char        *disp = e->label ? e->label : e->bsp;
			if (i == g_map_sel) {
				char buf[64];
				Com_sprintf(buf, sizeof(buf), "> %s", disp);
				DRAW_SM(buf, row, Q3IDE_CLR_MENU_SEL);
			} else {
				DRAW_SM(disp, row, Q3IDE_CLR_MENU_ITEM);
			}
		}
	} else {
		for (i = g_skin_scroll; i < K_SKINS_N && row < Q3IDE_MENU_VISIBLE_ROWS + 2; i++, row++) {
			if (k_skins[i].id == NULL) {
				char cat_buf[64];
				strip_cat(k_skins[i].label, cat_buf, sizeof(cat_buf));
				DRAW_SM(cat_buf, row, Q3IDE_CLR_MENU_CAT);
			} else if (i == g_skin_sel && !g_in_header) {
				char buf[64];
				Com_sprintf(buf, sizeof(buf), "> %s", k_skins[i].label);
				DRAW_SM(buf, row, Q3IDE_CLR_MENU_SEL);
			} else {
				DRAW_SM(k_skins[i].label, row, Q3IDE_CLR_MENU_ITEM);
			}
		}
	}

	/* Bottom hint */
	if (g_in_header)
		DRAW_SM("< > switch tab  down/enter  esc close", hint_row, 50, 50, 50);
	else if (g_tab == 0 && g_maps_mode == 0)
		DRAW_SM("enter expand  wheel/up/dn nav  esc close", hint_row, 50, 50, 50);
	else if (g_tab == 0 && g_maps_mode == 1)
		DRAW_SM("enter load  wheel/up/dn nav  esc back", hint_row, 50, 50, 50);
	else
		DRAW_SM("enter apply  wheel/up/dn nav  esc close", hint_row, 50, 50, 50);

#undef DRAW_SM
}
