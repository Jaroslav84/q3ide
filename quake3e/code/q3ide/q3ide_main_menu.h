/*
 * q3ide_main_menu.c — Custom map switcher (M key).
 *
 * Renders a centred billboard overlay listing all custom maps from the
 * random pool, grouped by releaser.  Navigate with arrow keys, load with
 * Enter, dismiss with M or Escape.
 */

#ifndef Q3IDE_MAIN_MENU_H
#define Q3IDE_MAIN_MENU_H

#include "../qcommon/q_shared.h"

/* Returns qtrue if the key was consumed. */
qboolean Q3IDE_Menu_OnKey(int key, qboolean down);

/* Returns qtrue while the menu is open — used by ConsumesInput(). */
qboolean Q3IDE_Menu_IsOpen(void);

/* Renders the menu overlay; no-ops when closed or RDF_NOWORLDMODEL. */
void Q3IDE_Menu_Draw(const void *refdef_ptr);

#endif /* Q3IDE_MAIN_MENU_H */
