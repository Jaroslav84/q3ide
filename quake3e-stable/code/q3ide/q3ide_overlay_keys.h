/* q3ide_overlay_keys.h — QWERTY layout + glyph cache infrastructure.
 * Include ONLY from q3ide_overlay_keyboard.c (statics are TU-private). */
#pragma once

/* clang-format off */
static const struct { char display; int keynum; } krows[4][10] = {
    {{'1',(int)'1'},{'2',(int)'2'},{'3',(int)'3'},{'4',(int)'4'},{'5',(int)'5'},
     {'6',(int)'6'},{'7',(int)'7'},{'8',(int)'8'},{'9',(int)'9'},{'0',(int)'0'}},
    {{'Q',(int)'q'},{'W',(int)'w'},{'E',(int)'e'},{'R',(int)'r'},{'T',(int)'t'},
     {'Y',(int)'y'},{'U',(int)'u'},{'I',(int)'i'},{'O',(int)'o'},{'P',(int)'p'}},
    {{'A',(int)'a'},{'S',(int)'s'},{'D',(int)'d'},{'F',(int)'f'},{'G',(int)'g'},
     {'H',(int)'h'},{'J',(int)'j'},{'K',(int)'k'},{'L',(int)'l'},{';',(int)';'}},
    {{'Z',(int)'z'},{'X',(int)'x'},{'C',(int)'c'},{'V',(int)'v'},{'B',(int)'b'},
     {'N',(int)'n'},{'M',(int)'m'},{'\0', 0},{'\0', 0},{'\0', 0}},
};
/* clang-format on */

static const int krow_len[4] = {10, 10, 10, 7};
static const float krow_indent[4] = {0.0f, 0.0f, 0.5f, 1.0f};

static const struct {
	const char *disp;
	int keynum;
	float row, col;
} kextras[4] = {
    {"E", K_ESCAPE, 4.5f, 0.0f},
    {"SP", K_SPACE, 4.5f, 2.0f},
    {"M1", K_MOUSE1, 4.5f, 5.5f},
    {"M2", K_MOUSE2, 4.5f, 7.0f},
};

/* ── Glyph cache (rate-limited rebuild → cheap per-frame replay) ── */

#define OVL_MAX_GLYPHS 1024

typedef struct {
	float right, up;
	int ch;
	byte r, g, b;
	byte small;  /* 0 = normal size, 1 = small scale (Q3IDE_OVL_SMALL_SCALE) */
	byte rotate; /* 0 = none, 1 = -45° screen-plane (small implied)          */
} ovl_rel_glyph_t;

/* Defined in q3ide_overlay_keyboard.c — shared with q3ide_overlay_kbcache.c */
extern ovl_rel_glyph_t g_glyphs[]; /* [OVL_MAX_GLYPHS] */
extern int g_glyph_count;
extern qboolean g_ovl_dirty;
extern unsigned long long g_ovl_last_build_ms;

static void ovl_emit(float right, float up, int ch, byte r, byte g, byte b)
{
	if (g_glyph_count < OVL_MAX_GLYPHS) {
		ovl_rel_glyph_t *gp = &g_glyphs[g_glyph_count++];
		gp->right = right;
		gp->up = up;
		gp->ch = ch;
		gp->r = r;
		gp->g = g;
		gp->b = b;
		gp->small = 0;
		gp->rotate = 0;
	}
}

static void ovl_emit_sm(float right, float up, int ch, byte r, byte g, byte b)
{
	if (g_glyph_count < OVL_MAX_GLYPHS) {
		ovl_rel_glyph_t *gp = &g_glyphs[g_glyph_count++];
		gp->right = right;
		gp->up = up;
		gp->ch = ch;
		gp->r = r;
		gp->g = g;
		gp->b = b;
		gp->small = 1;
		gp->rotate = 0;
	}
}

/* -45° rotated glyph: baseline goes up-right diagonally. Always small scale. */
static void ovl_emit_rot(float right, float up, int ch, byte r, byte g, byte b)
{
	if (g_glyph_count < OVL_MAX_GLYPHS) {
		ovl_rel_glyph_t *gp = &g_glyphs[g_glyph_count++];
		gp->right = right;
		gp->up = up;
		gp->ch = ch;
		gp->r = r;
		gp->g = g;
		gp->b = b;
		gp->small = 1;
		gp->rotate = 1;
	}
}

static Q3IDE_UNUSED void ovl_emit_str(float right, float up, const char *s, byte r, byte g, byte b)
{
	for (; *s; s++, right += Q3IDE_OVL_CHAR_W)
		ovl_emit(right, up, (unsigned char) *s, r, g, b);
}

static Q3IDE_UNUSED void ovl_emit_str_sm(float right, float up, const char *s, byte r, byte g, byte b)
{
	float cw = Q3IDE_OVL_CHAR_W * Q3IDE_OVL_SMALL_SCALE;
	for (; *s; s++, right += cw)
		ovl_emit_sm(right, up, (unsigned char) *s, r, g, b);
}

/* Rotated -45°: each char step = (right += cw, up += cw) — goes up-right. */
static void ovl_emit_str_rot(float right, float up, const char *s, byte r, byte g, byte b)
{
	float cw = Q3IDE_OVL_CHAR_W * Q3IDE_OVL_SMALL_SCALE * 0.7071f;
	for (; *s; s++, right += cw, up += cw)
		ovl_emit_rot(right, up, (unsigned char) *s, r, g, b);
}

/* ── Key state ─────────────────────────────────────────────────────── */

typedef enum { KEY_UNBOUND = 0, KEY_QUAKE, KEY_Q3IDE, KEY_COLLISION, KEY_PASSTHROUGH } key_state_t;

/* Keys hardwired to q3ide actions (no autoexec bind required). */
static qboolean is_q3ide_hardcoded_key(int keynum)
{
	return (keynum == ';' || keynum == 'h') ? qtrue : qfalse;
}

/* Keys that are critical game actions — q3ide on these = collision (red). */
static qboolean is_quake_default_key(int keynum)
{
	static const int q[] = {'w',          'a',     's',      'd',      K_UPARROW, K_DOWNARROW, K_LEFTARROW,
	                        K_RIGHTARROW, K_SPACE, K_MOUSE1, K_MOUSE2, '1',       '2',         '3',
	                        '4',          '5',     '6',      '7',      '8',       '9',         0};
	int i;
	for (i = 0; q[i]; i++)
		if (q[i] == keynum)
			return qtrue;
	return qfalse;
}

static key_state_t classify(int keynum)
{
	const char *b;
	if (is_q3ide_hardcoded_key(keynum))
		return KEY_Q3IDE;
	b = Key_GetBinding(keynum);
	if (!b || !*b)
		return KEY_UNBOUND;
	if (strstr(b, "q3ide_"))
		return is_quake_default_key(keynum) ? KEY_COLLISION : KEY_Q3IDE;
	return KEY_QUAKE;
}

static const char *q3ide_label(int keynum)
{
	const char *b;
	if (keynum == ';')
		return "Pause FPS";
	if (keynum == 'h')
		return "Hide Wins";
	b = Key_GetBinding(keynum);
	if (!b)
		return "";
	if (strstr(b, "q3ide_overview"))
		return "All Windows";
	if (strstr(b, "q3ide_focus3"))
		return "3 Monitors";
	return "Q3IDE";
}
