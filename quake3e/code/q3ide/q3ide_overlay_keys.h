/* q3ide_overlay_keys.h — QWERTY layout + glyph cache infrastructure.
 * Include ONLY from q3ide_overlay.c (statics are translation-unit-private). */
#pragma once

/* clang-format off */
static const struct { char display; int keynum; } krows[4][10] = {
    {{'1',(int)'1'},{'2',(int)'2'},{'3',(int)'3'},{'4',(int)'4'},{'5',(int)'5'},
     {'6',(int)'6'},{'7',(int)'7'},{'8',(int)'8'},{'9',(int)'9'},{'0',(int)'0'}},
    {{'Q',(int)'q'},{'W',(int)'w'},{'E',(int)'e'},{'R',(int)'r'},{'T',(int)'t'},
     {'Y',(int)'y'},{'U',(int)'u'},{'I',(int)'i'},{'O',(int)'o'},{'P',(int)'p'}},
    {{'A',(int)'a'},{'S',(int)'s'},{'D',(int)'d'},{'F',(int)'f'},{'G',(int)'g'},
     {'H',(int)'h'},{'J',(int)'j'},{'K',(int)'k'},{'L',(int)'l'},{'\0', 0}},
    {{'Z',(int)'z'},{'X',(int)'x'},{'C',(int)'c'},{'V',(int)'v'},{'B',(int)'b'},
     {'N',(int)'n'},{'M',(int)'m'},{'\0', 0},{'\0', 0},{'\0', 0}},
};
/* clang-format on */

static const int   krow_len[4]    = {10, 10, 9, 7};
static const float krow_indent[4] = {0.0f, 0.0f, 0.5f, 1.0f};

static const struct {
    const char *disp;
    int         keynum;
    float       row, col;
} kextras[4] = {
    {"E",  K_ESCAPE, 4.5f, 0.0f},
    {"SP", K_SPACE,  4.5f, 2.0f},
    {"M1", K_MOUSE1, 4.5f, 5.5f},
    {"M2", K_MOUSE2, 4.5f, 7.0f},
};

/* ── Glyph cache (rate-limited keyboard rebuild → cheap per-frame replay) ── */

#define OVL_MAX_GLYPHS 512

typedef struct {
    float right, up;
    int   ch;
    byte  r, g, b;
} ovl_rel_glyph_t;

static ovl_rel_glyph_t    g_glyphs[OVL_MAX_GLYPHS];
static int                g_glyph_count;
static qboolean           g_ovl_dirty         = qtrue;
static unsigned long long g_ovl_last_build_ms = 0;

static void ovl_emit(float right, float up, int ch, byte r, byte g, byte b)
{
    if (g_glyph_count < OVL_MAX_GLYPHS)
        g_glyphs[g_glyph_count++] = (ovl_rel_glyph_t){right, up, ch, r, g, b};
}

static void ovl_emit_str(float right, float up, const char *s, byte r, byte g, byte b)
{
    for (; *s; s++, right += Q3IDE_OVL_CHAR_W)
        ovl_emit(right, up, (unsigned char) *s, r, g, b);
}

typedef enum { KEY_UNBOUND = 0, KEY_QUAKE, KEY_Q3IDE } key_state_t;

static key_state_t classify(int keynum)
{
    const char *b = Key_GetBinding(keynum);
    if (!b || !*b) return KEY_UNBOUND;
    return strstr(b, "q3ide_") ? KEY_Q3IDE : KEY_QUAKE;
}

static const char *q3ide_label(int keynum)
{
    const char *b = Key_GetBinding(keynum);
    if (!b) return "";
    if (strstr(b, "q3ide_use_key")) return "Focus";
    if (strstr(b, "q3ide_lock"))    return "Laser";
    if (strstr(b, "q3ide_escape"))  return "Exit";
    return "Q3IDE";
}
