/*
 * q3ide_gamma.c — Map-specific gamma correction.
 * Detects over-bright maps by name prefix and lowers r_gamma on them.
 */

#include "q3ide_log.h"
#include "q3ide_params.h"
#include "../qcommon/qcommon.h"
#include <ctype.h>
#include <string.h>

/* Bright-map pattern list — defined in q3ide_params.h (Q3IDE_BRIGHT_MAPS). */
static const char *q3ide_bright_maps[] = {Q3IDE_BRIGHT_MAPS};

/* Case-insensitive glob match: '*' = any sequence, '?' = any char. */
static qboolean q3ide_glob_match(const char *pattern, const char *str)
{
	while (*pattern && *str) {
		if (*pattern == '*') {
			/* skip consecutive stars */
			while (*(pattern + 1) == '*')
				pattern++;
			if (!*(pattern + 1))
				return qtrue; /* trailing * matches everything */
			while (*str) {
				if (q3ide_glob_match(pattern + 1, str))
					return qtrue;
				str++;
			}
			return qfalse;
		} else if (*pattern == '?' || tolower((unsigned char)*pattern) == tolower((unsigned char)*str)) {
			pattern++;
			str++;
		} else {
			return qfalse;
		}
	}
	while (*pattern == '*')
		pattern++;
	return (*pattern == '\0' && *str == '\0');
}

static float g_base_gamma      = 0.0f; /* r_gamma saved before override; 0 = no override active */
static int   g_base_overbright = -1;   /* r_overbrightbits saved before override; -1 = not saved */
static char  g_last_mapname[64] = "";

void q3ide_apply_map_gamma(const char *mapname)
{
	const char **pfx;
	const char  *leaf;
	float        cur_gamma;
	qboolean     is_bright = qfalse;

	/* Strip leading path — mapname may be "maps/lun3dm5" */
	leaf = strrchr(mapname, '/');
	leaf = leaf ? leaf + 1 : mapname;

	for (pfx = q3ide_bright_maps; *pfx; pfx++) {
		if (q3ide_glob_match(*pfx, leaf)) {
			is_bright = qtrue;
			break;
		}
	}

	cur_gamma = Cvar_VariableValue("r_gamma");
	Q3IDE_LOGI("map change: '%s' leaf='%s' is_bright=%d cur_gamma=%.2f base_gamma=%.2f", mapname, leaf, is_bright,
	           cur_gamma, g_base_gamma);

	if (is_bright) {
		if (g_base_gamma == 0.0f) {
			g_base_gamma = (cur_gamma > 0.0f) ? cur_gamma : 1.0f;
			g_base_overbright = (int) Cvar_VariableValue("r_overbrightbits");
		}
		Cvar_SetValue("r_gamma", g_base_gamma * Q3IDE_BRIGHT_MAP_GAMMA);
		Cvar_SetValue("r_overbrightbits", Q3IDE_BRIGHT_MAP_OVERBRIGHT_BITS);
		Q3IDE_LOGI("bright map '%s': gamma %.2f->%.2f overbright %d->%d", leaf, g_base_gamma,
		           g_base_gamma * Q3IDE_BRIGHT_MAP_GAMMA, g_base_overbright, Q3IDE_BRIGHT_MAP_OVERBRIGHT_BITS);
	} else if (g_base_gamma != 0.0f) {
		Cvar_SetValue("r_gamma", g_base_gamma);
		Cvar_SetValue("r_overbrightbits", g_base_overbright);
		Q3IDE_LOGI("normal map '%s': gamma %.2f overbright %d restored", leaf, g_base_gamma, g_base_overbright);
		g_base_gamma = 0.0f;
		g_base_overbright = -1;
	}
}

/* Returns the last seen mapname (static buffer — read-only). */
const char *q3ide_last_mapname(void)
{
	return g_last_mapname;
}

/* Update cached mapname and apply gamma if it changed.  Call every frame. */
void q3ide_gamma_tick(const char *cur_map)
{
	if (Q_stricmp(cur_map, g_last_mapname) != 0) {
		Q_strncpyz(g_last_mapname, cur_map, sizeof(g_last_mapname));
		q3ide_apply_map_gamma(cur_map);
	}
}
