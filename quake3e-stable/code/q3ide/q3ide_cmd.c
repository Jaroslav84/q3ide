/* q3ide_cmd.c — DetachById. */

#include "q3ide_wm.h"
#include "q3ide_log.h"
#include "q3ide_wm_internal.h"
#include "../qcommon/qcommon.h"
#include "../client/client.h"
#include <string.h>

/* App filtering — defined in q3ide_cmd_query.c */
extern const char *q3ide_terminal_apps[];
extern const char *q3ide_browser_apps[];
extern qboolean q3ide_match(const char *app, const char **list);

extern qboolean q3ide_is_attached(unsigned int id);

qboolean Q3IDE_WM_DetachById(unsigned int cid)
{
	int i;
	for (i = 0; i < Q3IDE_MAX_WIN; i++) {
		q3ide_win_t *w = &q3ide_wm.wins[i];
		if (!w->active || w->capture_id != cid)
			continue;
		if (q3ide_wm.cap_stop)
			q3ide_wm.cap_stop(q3ide_wm.cap, cid);
		memset(w, 0, sizeof(q3ide_win_t));
		q3ide_wm.num_active--;
		Q3IDE_LOGI("detached wid=%u", cid);
		return qtrue;
	}
	Q3IDE_LOGI("wid=%u not found", cid);
	return qfalse;
}

/* CmdAttach — q3ide_cmd_attach.c */
/* PollChanges, Desktop — q3ide_cmd_query.c */
void Q3IDE_WM_PollChanges(void);
void Q3IDE_WM_CmdDesktop(void);
