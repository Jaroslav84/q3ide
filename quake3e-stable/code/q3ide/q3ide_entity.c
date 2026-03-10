/*
 * q3ide_entity.c — Game entity hover detection.
 *
 * Each frame casts the crosshair ray against snapshot entity positions.
 * For ET_ITEM: entityState_t.modelindex == item index in bg_itemlist[].
 * For ET_PLAYER: shows "Player".
 * Writes the name into q3ide_interaction.hovered_entity_name.
 */

#include "q3ide_engine_hooks.h"
#include "q3ide_interaction.h"
#include "q3ide_params.h"
#include "../qcommon/qcommon.h"
#include "../client/client.h"
#include "../game/bg_public.h"
#include <string.h>
#include <math.h>


/*
 * bg_itemlist[] order — standard Q3 / Quake3e.
 * Index 0 is the "no item" sentinel; indices match es->modelindex for ET_ITEM.
 */
static const char *q3_item_names[] = {
    NULL,                  /* 0  — sentinel */
    "Gauntlet",            /* 1  weapon_gauntlet */
    "Machine Gun",         /* 2  weapon_machinegun */
    "Shotgun",             /* 3  weapon_shotgun */
    "Grenade Launcher",    /* 4  weapon_grenadelauncher */
    "Rocket Launcher",     /* 5  weapon_rocketlauncher */
    "Lightning Gun",       /* 6  weapon_lightning */
    "Railgun",             /* 7  weapon_railgun */
    "Plasma Gun",          /* 8  weapon_plasmagun */
    "BFG10K",              /* 9  weapon_bfg */
    "Grappling Hook",      /* 10 weapon_grapplinghook */
    "Bullets",             /* 11 ammo_bullets */
    "Shells",              /* 12 ammo_shells */
    "Grenades",            /* 13 ammo_grenades */
    "Cells",               /* 14 ammo_cells */
    "Lightning",           /* 15 ammo_lightning */
    "Rockets",             /* 16 ammo_rockets */
    "Slugs",               /* 17 ammo_slugs */
    "BFG Ammo",            /* 18 ammo_bfg */
    "Personal Teleporter", /* 19 holdable_teleporter */
    "Medkit",              /* 20 holdable_medkit */
    "Quad Damage",         /* 21 item_quad */
    "Battle Suit",         /* 22 item_enviro */
    "Haste",               /* 23 item_haste */
    "Invisibility",        /* 24 item_invis */
    "Regeneration",        /* 25 item_regen */
    "Flight",              /* 26 item_flight */
    "+5 Health",           /* 27 item_health_small */
    "+25 Health",          /* 28 item_health */
    "+50 Health",          /* 29 item_health_large */
    "Mega Health",         /* 30 item_health_mega */
    "Armor Shard",         /* 31 item_armor_shard */
    "Yellow Armor",        /* 32 item_armor_combat */
    "Red Armor",           /* 33 item_armor_body */
    "Red Flag",            /* 34 team_CTF_redflag */
    "Blue Flag",           /* 35 team_CTF_blueflag */
    "Neutral Flag",        /* 36 team_CTF_neutralflag */
};
#define Q3_ITEM_NAMES_COUNT (int) (sizeof(q3_item_names) / sizeof(q3_item_names[0]))

void Q3IDE_UpdateEntityHover(void)
{
	vec3_t eye, fwd;
	float yaw, pitch, cy, sy, cp, sp;
	int i;
	float best_dist = Q3IDE_ENT_MAX_DIST;
	const char *best_name = NULL;

	q3ide_interaction.hovered_entity_name[0] = '\0';

	if (cls.state != CA_ACTIVE)
		return;

	VectorCopy(cl.snap.ps.origin, eye);
	eye[2] += cl.snap.ps.viewheight;
	yaw = cl.snap.ps.viewangles[YAW] * (float) M_PI / 180.0f;
	pitch = cl.snap.ps.viewangles[PITCH] * (float) M_PI / 180.0f;
	cy = cosf(yaw);
	sy = sinf(yaw);
	cp = cosf(pitch);
	sp = sinf(pitch);
	fwd[0] = cp * cy;
	fwd[1] = cp * sy;
	fwd[2] = -sp;

	for (i = 0; i < cl.snap.numEntities; i++) {
		const entityState_t *ent = &cl.parseEntities[(cl.snap.parseEntitiesNum + i) & (MAX_PARSE_ENTITIES - 1)];
		vec3_t delta;
		float dist, dot;
		const char *name = NULL;

		if (ent->eType != ET_ITEM && ent->eType != ET_PLAYER)
			continue;

		VectorSubtract(ent->pos.trBase, eye, delta);
		dist = VectorLength(delta);
		if (dist < 1.0f || dist > Q3IDE_ENT_MAX_DIST)
			continue;

		dot = (delta[0] * fwd[0] + delta[1] * fwd[1] + delta[2] * fwd[2]) / dist;
		if (dot < Q3IDE_ENT_COS_CONE)
			continue;

		if (ent->eType == ET_PLAYER) {
			name = "Player";
		} else {
			int idx = ent->modelindex;
			if (idx > 0 && idx < Q3_ITEM_NAMES_COUNT)
				name = q3_item_names[idx];
			if (!name)
				name = "Item";
		}

		if (dist < best_dist) {
			best_dist = dist;
			best_name = name;
		}
	}

	if (best_name)
		Q_strncpyz(q3ide_interaction.hovered_entity_name, best_name, sizeof(q3ide_interaction.hovered_entity_name));
}
