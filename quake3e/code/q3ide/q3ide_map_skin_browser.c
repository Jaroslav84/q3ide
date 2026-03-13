/* q3ide_map_skin_browser.c — Map/skin browser: state, navigation, key handling (M key). */

#include "q3ide_map_skin_browser.h"
#include "q3ide_engine_hooks.h"
#include "q3ide_log.h"
#include "q3ide_params.h"
#include "../client/client.h"
#include "../client/snd_public.h"
#include "../qcommon/qcommon.h"
#include <string.h>
#include "q3ide_main_menu_data.h"

#define VISIBLE_ROWS    14
#define REPEAT_DELAY_MS 300
#define REPEAT_RATE_MS  60

int g_open = 0; /* 0=closed 1=root 2=maps 3=skins */
int g_root_sel = 0;
int g_map_sel = 0;
int g_map_scroll = 0;
int g_skin_sel = 0;
int g_skin_scroll = 0;
static int g_nav_key = 0;
static unsigned long long g_nav_down_ms = 0;
static unsigned long long g_nav_last_rep = 0;

static sfxHandle_t g_snd_move = 0;
static sfxHandle_t g_snd_select = 0;
static sfxHandle_t g_snd_back = 0;
qhandle_t g_bg_shader = 0;

static void snd(sfxHandle_t h)
{
	if (h)
		S_StartLocalSound(h, CHAN_LOCAL);
}

/* Clamp scroll so selected item stays visible. */
static void clamp_scroll(int *sel, int *scroll, int n)
{
	if (*sel < 0)
		*sel = 0;
	if (*sel >= n)
		*sel = n - 1;
	if (*sel < *scroll)
		*scroll = *sel;
	if (*sel >= *scroll + VISIBLE_ROWS)
		*scroll = *sel - VISIBLE_ROWS + 1;
}

/* Move selection, skipping section headers (bsp==NULL). */
static void map_move(int delta)
{
	int n = K_MAPS_N;
	int i, next = g_map_sel + delta;
	for (i = 0; i < n; i++) {
		if (next < 0)
			next = n - 1;
		if (next >= n)
			next = 0;
		if (k_maps[next].bsp != NULL)
			break;
		next += delta;
	}
	if (k_maps[next].bsp == NULL)
		return;
	if (next != g_map_sel)
		snd(g_snd_move);
	g_map_sel = next;
	clamp_scroll(&g_map_sel, &g_map_scroll, n);
}

static void skin_preview(void)
{
	const char *id = k_skins[g_skin_sel].id;
	if (id)
		Cbuf_AddText(va("model %s; headmodel %s\n", id, id));
}

static void skin_move(int delta)
{
	int n = K_SKINS_N;
	int next = g_skin_sel + delta;
	for (int i = 0; i < n; i++) {
		if (next < 0)
			next = n - 1;
		if (next >= n)
			next = 0;
		if (k_skins[next].id != NULL)
			break;
		next += delta;
	}
	if (k_skins[next].id == NULL)
		return;
	if (next != g_skin_sel)
		snd(g_snd_move);
	g_skin_sel = next;
	clamp_scroll(&g_skin_sel, &g_skin_scroll, n);
	skin_preview();
}

void Q3IDE_MMenu_Init(void)
{
	g_snd_move = S_RegisterSound("sound/misc/menu1.wav", qfalse);
	g_snd_select = S_RegisterSound("sound/misc/menu2.wav", qfalse);
	g_snd_back = S_RegisterSound("sound/misc/menu3.wav", qfalse);
	g_skin_sel = 1; /* default selection: homer/default (index 1, after the Custom header) */
}

qboolean Q3IDE_MMenu_IsOpen(void)
{
	return g_open != 0;
}

qboolean Q3IDE_MMenu_OnKey(int key, qboolean down)
{
	unsigned long long now;

	if (key == 'm') {
		if (!down)
			return g_open != 0;
		if (!g_open) {
			g_open = 1;
			snd(g_snd_select);
		} else {
			g_open = 0;
			snd(g_snd_back);
		}
		return qtrue;
	}

	if (!g_open)
		return qfalse;

	if (!down)
		return qtrue;

	now = (unsigned long long) Sys_Milliseconds();

	if (key == g_nav_key) {
		if (now - g_nav_down_ms < REPEAT_DELAY_MS)
			return qtrue;
		if (now - g_nav_last_rep < REPEAT_RATE_MS)
			return qtrue;
		g_nav_last_rep = now;
	} else {
		g_nav_key = key;
		g_nav_down_ms = now;
		g_nav_last_rep = now;
	}

	if (key == K_ESCAPE) {
		if (g_open > 1) {
			g_open = 1;
			snd(g_snd_back);
		} else {
			g_open = 0;
			snd(g_snd_back);
		}
		return qtrue;
	}

	if (g_open == 1) {
		if (key == K_UPARROW || key == K_DOWNARROW) {
			g_root_sel = !g_root_sel;
			snd(g_snd_move);
		} else if (key == K_ENTER || key == K_KP_ENTER) {
			g_open = g_root_sel == 0 ? 2 : 3;
			snd(g_snd_select);
			if (g_open == 3)
				skin_preview(); /* show current selection immediately on open */
		}
		return qtrue;
	}

	if (g_open == 2) {
		if (key == K_UPARROW)
			map_move(-1);
		else if (key == K_DOWNARROW)
			map_move(1);
		else if (key == K_ENTER || key == K_KP_ENTER) {
			if (k_maps[g_map_sel].bsp) {
				snd(g_snd_select);
				g_open = 0;
				Cbuf_AddText(va("map %s\n", k_maps[g_map_sel].bsp));
			}
		}
		return qtrue;
	}

	if (g_open == 3) {
		if (key == K_UPARROW)
			skin_move(-1);
		else if (key == K_DOWNARROW)
			skin_move(1);
		else if (key == K_ENTER || key == K_KP_ENTER) {
			if (k_skins[g_skin_sel].id) {
				snd(g_snd_select);
				g_open = 0; /* model already applied live by skin_preview() */
			}
		}
		return qtrue;
	}

	return qtrue;
}
