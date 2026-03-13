/*
 * q3ide_main_menu_data.h — Map and skin tables for M-menu.
 * Included by q3ide_map_skin_browser.c and q3ide_map_skin_browser_draw.c.
 */

#ifndef Q3IDE_MAIN_MENU_DATA_H
#define Q3IDE_MAIN_MENU_DATA_H

typedef struct {
	const char *bsp;   /* NULL = category header */
	const char *label; /* NULL = use bsp as display label */
} map_entry_t;

/* clang-format off */
static const map_entry_t k_maps[] = {
	{NULL, "--- Lunaran ---"},
	{"lun3dm1", NULL}, {"lun3dm2", NULL}, {"lun3dm3-cpm", NULL},
	{"lun3dm4", NULL}, {"lun3dm5", NULL}, {"lun3_20b1",   NULL},
	{NULL, "--- Acidwire ---"},
	{"acid3dm2",      NULL}, {"acid3dm3",  NULL}, {"acid3dm3r", NULL},
	{"acid3tourney3", NULL}, {"acid3dm4",  NULL}, {"acid3dm5",  NULL},
	{"acid3dm6",      NULL}, {"acid3dm7",  NULL}, {"acid3dm8",  NULL},
	{"acid3dm9",      NULL}, {"acid3dm10", NULL}, {"acid3dm11", NULL},
	{"acid3dm12",     NULL},
	{NULL, "--- CPMA ---"},
	{"cpm1a",  NULL},
	{"cpm2",   NULL}, {"cpm3",   NULL}, {"cpm3a",  NULL}, {"cpm4",   NULL}, {"cpm4a",  NULL},
	{"cpm5",   NULL}, {"cpm6",   NULL}, {"cpm7",   NULL}, {"cpm8",   NULL}, {"cpm9",   NULL},
	{"cpm10",  NULL}, {"cpm11",  NULL}, {"cpm11a", NULL}, {"cpm12",  NULL}, {"cpm13",  NULL},
	{"cpm14",  NULL}, {"cpm15",  NULL}, {"cpm16",  NULL}, {"cpm17",  NULL}, {"cpm18",  NULL},
	{"cpm18r", NULL}, {"cpm19",  NULL}, {"cpm20",  NULL}, {"cpm21",  NULL}, {"cpm22",  NULL},
	{"cpm23",  NULL}, {"cpm24",  NULL}, {"cpm25",  NULL}, {"cpm26",  NULL}, {"cpm27",  NULL},
	{"cpm28",  NULL}, {"cpm29",  NULL},
	{"cpmctf1", NULL}, {"cpmctf2", NULL}, {"cpmctf3", NULL},
	{"cpmctf4", NULL}, {"cpmctf5", NULL}, {"cpma3",   NULL},
	{NULL, "--- CTF ---"},
	{"q3ctfchnu01",    "q3ctfchnu01 (Porcelain)"},
	{"QuadCTF",        NULL},
	{"jlctf1",         NULL}, {"jlctf2", NULL}, {"jlctf3", NULL},
	{"q3tourney6_ctf", NULL},
	{NULL, "--- DC Mappack ---"},
	{"dc_map02", NULL}, {"dc_map03", NULL}, {"dc_map04", NULL}, {"dc_map05", NULL},
	{"dc_map06", NULL}, {"dc_map07", NULL}, {"dc_map08", NULL}, {"dc_map09", NULL},
	{"dc_map10", NULL}, {"dc_map11", NULL}, {"dc_map12", NULL}, {"dc_map13", NULL},
	{"dc_map14", NULL}, {"dc_map15", NULL}, {"dc_map16", NULL}, {"dc_map17", NULL},
	{"dc_map18", NULL}, {"dc_map19", NULL}, {"dc_map20", NULL}, {"dc_map21", NULL},
	{"dc_map22", NULL}, {"dc_map23", NULL}, {"dc_map24", NULL},
	{NULL, "--- ZTN ---"},
	{"ztn3dm1",      "ztn3dm1      (The Forgotten Place)"},
	{"ztn3dm1-ho",   "ztn3dm1-ho   (Hospital variant)"},
	{"ztn3tourney1", "ztn3tourney1 (Blood Run)"},
	{NULL, "--- Classic Pro DM ---"},
	{"hub3aeroq3",     "hub3aeroq3   (Aerowalk)"},
	{"aggressor",      NULL}, {"bloodcovenant", NULL},
	{"focal_p132",     "focal_p132   (Focal Point)"},
	{"overkill",       NULL},
	{"pro-nodm9",      "pro-nodm9    (A Familiar Place)"},
	{"pro-q3dm6",      NULL}, {"pro-q3dm13",     NULL},
	{"pro-q3tourney2", NULL}, {"pro-q3tourney4",  NULL},
	{"pro-q3tourney7", "pro-q3tourney7 (Almost Lost)"},
	{"pukka3tourney2", NULL},
	{"tig_den",        "tig_den      (The Den)"},
	{NULL, "--- Egyptian ---"},
	{"egyptsm1", "egyptsm1 (Egyptian SM1)"}, {"gpl-gypt", "gpl-gypt (GPL Egypt)"},
	{NULL, "--- OSP ---"},
	{"ospca1",  NULL}, {"ospctf1", NULL}, {"ospctf2", NULL},
	{"ospdm1",  NULL}, {"ospdm2",  NULL}, {"ospdm3",  NULL}, {"ospdm4",  NULL},
	{"ospdm5",  NULL}, {"ospdm6",  NULL}, {"ospdm7",  NULL}, {"ospdm8",  NULL},
	{"ospdm9",  NULL}, {"ospdm10", NULL}, {"ospdm11", NULL}, {"ospdm12", NULL},
	{NULL, "--- Threewave CTF ---"},
	{"q3wcp1",  NULL}, {"q3wcp2",  NULL}, {"q3wcp3",  NULL}, {"q3wcp4",  NULL},
	{"q3wcp5",  NULL}, {"q3wcp6",  NULL}, {"q3wcp7",  NULL}, {"q3wcp8",  NULL},
	{"q3wxs1",  NULL}, {"q3wcp9",  NULL}, {"q3wcp10", NULL}, {"q3wcp11", NULL},
	{"q3wcp12", NULL}, {"q3wcp13", NULL}, {"q3wcp14", NULL}, {"q3wcp15", NULL},
	{"q3wcp16", NULL}, {"q3wxs2",  NULL},
	{NULL, "--- WTF Pack ---"},
	{"wtf01", NULL}, {"wtf02", NULL}, {"wtf03", NULL}, {"wtf04", NULL}, {"wtf05", NULL},
	{"wtf06", NULL}, {"wtf07", NULL}, {"wtf08", NULL}, {"wtf09", NULL}, {"wtf10", NULL},
	{"wtf11", NULL}, {"wtf12", NULL}, {"wtf13", NULL}, {"wtf14", NULL}, {"wtf15", NULL},
	{"wtf16", NULL}, {"wtf17", NULL}, {"wtf18", NULL}, {"wtf19", NULL}, {"wtf20", NULL},
	{"wtf21", NULL}, {"wtf22", NULL}, {"wtf23", NULL}, {"wtf24", NULL}, {"wtf25", NULL},
	{"wtf26", NULL}, {"wtf27", NULL}, {"wtf28", NULL}, {"wtf29", NULL}, {"wtf30", NULL},
	{"wtf31", NULL}, {"wtf32", NULL}, {"wtf33", NULL}, {"wtf34", NULL}, {"wtf35", NULL},
	{"wtf36", NULL}, {"wtf37", NULL}, {"wtf38", NULL}, {"wtf39", NULL}, {"wtf40", NULL},
	{"wtf41", NULL}, {"wtf42", NULL}, {"wtf43", NULL}, {"wtf44", NULL}, {"wtf45", NULL},
	{"wtf46", NULL}, {"wtf47", NULL}, {"wtf48", NULL},
	{"wtf01-pro", NULL}, {"wtf02-pro", NULL}, {"wtf04-pro", NULL},
	{"wtf07-pro", NULL}, {"wtf08-pro", NULL}, {"wtf25-pro", NULL},
	{"wtf32-day", NULL}, {"wtf33-pro", NULL}, {"wtf37-pro", NULL}, {"wtf48-pro", NULL},
	{"wtf35-bfg",       NULL}, {"wtf35-gauntlet",  NULL}, {"wtf35-grayscale", NULL},
	{"wtf35-grenades",  NULL}, {"wtf35-lightning",  NULL}, {"wtf35-machine",   NULL},
	{"wtf35-plasmagun", NULL}, {"wtf35-rocket",     NULL}, {"wtf35-shotgun",   NULL},
	{"wtf35-ultra",     NULL},
	{NULL, "--- sst13 Fun ---"},
	{"13matrix", "13matrix (Campgrounds Matrix)"}, {"13star",   "13star   (Starforce)"},
	{"13cube",   "13cube   (Hypercube 13)"},       {"13island", "13island (Stoneface Islands)"},
	{"13stone",  "13stone  (Floating Stonefaces)"}, {"13dyna",  "13dyna   (Dynablast)"},
	{NULL, "--- Other ---"},
	{"ori_apt",        "ori_apt        (Apartment)"},
	{"quatrix",        "quatrix        (Matrix)"},
	{"r7-blockworld1", "r7-blockworld1 (Minecraft)"},
	{"m3amap1",        NULL},
};
/* clang-format on */

#define K_MAPS_N ((int) (sizeof(k_maps) / sizeof(k_maps[0])))

typedef struct {
	const char *id; /* NULL = category header */
	const char *label;
} skin_entry_t;

/* clang-format off */
static const skin_entry_t k_skins[] = {
	/* Custom models — listed first, homer is the default */
	{NULL,                  "--- Custom ---"},
	{"homer/default",       "Homer (default)"},
	{"bender/default",      "Bender (default)"},
	{"ewj/default",         "Earthworm Jim (default)"},
	{"SargentDoom/default", "SargentDoom (default)"},
	{"SargentDoom/blue",    "SargentDoom (blue)"},
	{"SargentDoom/red",     "SargentDoom (red)"},
	{"SargentDoom/psyco",   "SargentDoom (psyco)"},
	/* Standard Q3 models */
	{NULL,               "--- Standard ---"},
	{"sarge/default",    "Sarge (default)"},  {"sarge/blue",     "Sarge (blue)"},
	{"sarge/red",        "Sarge (red)"},      {"keel/default",   "Keel (default)"},
	{"keel/blue",        "Keel (blue)"},      {"keel/red",       "Keel (red)"},
	{"visor/default",    "Visor (default)"},  {"anarki/default", "Anarki (default)"},
	{"klesk/default",    "Klesk (default)"},  {"daemia/default", "Daemia (default)"},
	{"slash/default",    "Slash (default)"},  {"mynx/default",   "Mynx (default)"},
	{"hunter/default",   "Hunter (default)"}, {"orbb/default",   "Orbb (default)"},
	{"sorlag/default",   "Sorlag (default)"}, {"tankjr/default", "TankJr (default)"},
	{"biker/default",    "Biker (default)"},  {"major/default",  "Major (default)"},
};
/* clang-format on */

#define K_SKINS_N ((int) (sizeof(k_skins) / sizeof(k_skins[0])))

#endif /* Q3IDE_MAIN_MENU_DATA_H */
