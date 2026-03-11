/* q3ide_attach_filter.c — App lists, filter helpers, pending-attach queue. */

#include "q3ide_win_mngr.h"
#include "q3ide_log.h"
#include "q3ide_win_mngr_internal.h"
#include "../qcommon/qcommon.h"
#include <string.h>

/* ── App category lists ─────────────────────────────────────────── */

const char *q3ide_terminal_apps[] = {"iTerm2", "Terminal", NULL};
const char *q3ide_browser_apps[] = {"Google Chrome", "Chromium", "Safari",         "Firefox", "Arc",
                                    "Brave Browser", "Opera",    "Microsoft Edge", NULL};

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
 */
static qboolean q3ide_is_system_junk(const Q3ideWindowInfo *w)
{
	const char *t = w->title ? w->title : "";
	const char *a = w->app_name ? w->app_name : "";

	if (Q_stristr(t, "Wallpaper-") || Q_stristr(a, "Wallpaper"))
		return qtrue;
	if (!Q_stricmp(t, "Dock") || !Q_stricmp(a, "Dock"))
		return qtrue;
	if (!Q_stricmp(t, "LPSpringboard") || !Q_stricmp(a, "LPSpringboard"))
		return qtrue;
	if (!Q_stricmp(t, "loginwindow") || !Q_stricmp(a, "loginwindow"))
		return qtrue;
	if (Q_stristr(t, "Accessibility Services") || Q_stristr(a, "Accessibility Services"))
		return qtrue;
	if (!Q_stricmp(t, "Spotlight") || !Q_stricmp(a, "Spotlight"))
		return qtrue;
	if (!Q_stricmp(t, "Notification Center") || !Q_stricmp(a, "NotificationCenter"))
		return qtrue;
	if (Q_stristr(t, "Open and Save Panel Service") || Q_stristr(a, "Open and Save Panel Service"))
		return qtrue;
	if (Q_stristr(t, "Screen & System Audio Recording") || Q_stristr(a, "Screen & System Audio Recording"))
		return qtrue;
	if (!Q_stricmp(a, "WindowServer"))
		return qtrue;
	if (!Q_stricmp(t, "Desktop") || !Q_stricmp(a, "Desktop"))
		return qtrue; /* Finder desktop layers */
	if (!Q_stricmp(t, "Little Snitch Agent") || !Q_stricmp(a, "Little Snitch Agent"))
		return qtrue;
	if (Q_stristr(t, "Keyboard Maestro Engine") || Q_stristr(a, "Keyboard Maestro Engine"))
		return qtrue;
	if (!Q_stricmp(a, "Default Folder X"))
		return qtrue;
	/* Exclude the game itself — capturing the Quake window inside itself kills FPS */
	if (Q_stristr(t, "Quake 3") || Q_stristr(a, "Quake3"))
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
