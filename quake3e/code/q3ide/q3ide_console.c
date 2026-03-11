/*
 * q3ide_console.c — Q3IDE_Cmd_f: console command dispatcher.
 * Engine hooks: q3ide_engine.c.
 */

#include "q3ide_engine_hooks.h"
#include "q3ide_log.h"
#include "q3ide_win_mngr.h"
#include "q3ide_win_mngr_internal.h"
#include "../qcommon/qcommon.h"
#include "../client/client.h"

extern q3ide_hooks_state_t q3ide_state;
extern int q3ide_selected_win;
extern int q3ide_select_time;

void Q3IDE_Cmd_f(void)
{
	const char *sub;
	if (Cmd_Argc() < 2) {
		Com_Printf("usage: q3ide <list|detach|status>\n");
		return;
	}
	sub = Cmd_Argv(1);
	if (!Q_stricmp(sub, "list"))
		Q3IDE_WM_CmdList();
	else if (!Q_stricmp(sub, "detach")) {
		if (Cmd_Argc() >= 3)
			Q3IDE_WM_DetachById((unsigned int) atoi(Cmd_Argv(2)));
		else
			Q3IDE_WM_CmdDetachAll();
	} else if (!Q_stricmp(sub, "status"))
		Q3IDE_WM_CmdStatus();
	else
		Com_Printf("q3ide: unknown sub-command '%s'\n", sub);
}
