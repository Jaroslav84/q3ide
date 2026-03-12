/* q3ide_attach_filter.c — App lists, filter helpers, pending-attach queue. */

#include "q3ide_win_mngr.h"
#include "q3ide_log.h"
#include "q3ide_params.h"
#include "q3ide_win_mngr_internal.h"
#include "../qcommon/qcommon.h"
#include <string.h>

/* ── App category lists (sourced from q3ide_params.h) ───────────── */

const char *q3ide_terminal_apps[] = {Q3IDE_CPU_WINDOWS_LIST};
const char *q3ide_browser_apps[]  = {Q3IDE_GPU_WINDOWS_LIST};

qboolean q3ide_match(const char *app, const char **list)
{
	int i;
	if (!app)
		return qfalse;
	for (i = 0; list[i]; i++)
		if (Q_stristr(app, list[i]))
			return qtrue;
	return qfalse;
}

qboolean q3ide_is_attached(unsigned int id)
{
	int i;
	for (i = 0; i < Q3IDE_MAX_WIN; i++)
		if (q3ide_wm.wins[i].active && q3ide_wm.wins[i].capture_id == id)
			return qtrue;
	return qfalse;
}

/*
 * Returns qtrue for windows that are system UI — not real application content.
 * Pattern list defined in q3ide_params.h: Q3IDE_JUNK_LIST.
 * Glob syntax: no wildcard = exact match, *word* = substring. Case-insensitive.
 */
static qboolean q3ide_is_system_junk(const Q3ideWindowInfo *w)
{
	static const char *junk[] = {Q3IDE_APP_BLACKLIST};
	const char        *t      = w->title ? w->title : "";
	const char        *a      = w->app_name ? w->app_name : "";
	int                i;

	for (i = 0; junk[i]; i++)
		if (Com_Filter(junk[i], t) || Com_Filter(junk[i], a))
			return qtrue;
	return qfalse;
}

qboolean Q3IDE_IsSystemJunk(const Q3ideWindowInfo *w)
{
	return q3ide_is_system_junk(w);
}

/*
 * Junk check when only app_name is available (change events lack title).
 * Uses the same rules as q3ide_is_system_junk with app_name standing in for title.
 */
qboolean Q3IDE_IsJunkAppName(const char *app_name)
{
	Q3ideWindowInfo tmp;
	tmp.window_id = 0;
	tmp.title = app_name;
	tmp.app_name = app_name;
	tmp.width = 0;
	tmp.height = 0;
	tmp.is_on_screen = 0;
	return q3ide_is_system_junk(&tmp);
}

/* Pending queue — moved to q3ide_pending_queue.c */
