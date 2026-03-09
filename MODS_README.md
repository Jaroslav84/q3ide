# Quake 3 Mods, Maps & Models — Resource Guide

Accumulated during the q3ide session, March 2026. All sources tested.

---

## Maps

### Best Sites

| Site | URL | Notes |
|------|-----|-------|
| **LvLWorld** | https://lvlworld.com | The definitive Q3 map database. Ratings, screenshots, reviews. Huge archive. |
| **quake.okayfun.com** | http://quake.okayfun.com/maps/ | Direct .pk3 downloads, no JS bullshit. Worked great. |
| **Quake3World forums** | https://www.quake3world.com/forum/ | Community, custom content links, converters |
| **archive.org** | https://archive.org | Search "quake 3 maps" — large map packs available |
| **Quaddicted** | https://www.quaddicted.com | Mostly Quake 1 but has some Q3 content |

### Maps We Have (in baseq3/ on Mac)

```
map r7-blockworld1     — Minecraft/blocky style, open parkour (from quake.okayfun.com)
map acid3dm2..12       — All 12 Acidwire maps (excellent FFA/duel maps)
map ori_apt            — Apartment layout, tight corridors
map q3ctfchnu01        — "Porcelain" CTF map
map QuadCTF            — CTF map
map quatrix            — Matrix-themed
map q325_deck          — Deck-style remake
map spikedm1c          — Spiked DM
map spikedm9           — Spiked DM 9
map tsh3dm2            — TSH DM2
map ztn3dm1            — ZTN classic
map shortcircuit       — Shortcircuit DM
map tig_den            — Tig's Den
```

### Map Packs on archive.org

Search: https://archive.org/search?query=quake+3+arena+maps

Notable finds:
- **CPMA map pack** — https://archive.org/details/quake-3-includes-defrag-and-cpma (huge pack)
- **Dreamcast maps** — dreamcast_maps.zip (already in baseq3/)
- **DC map pack** — dcmappack.zip (already in baseq3/)
- **Q3ADC pack** — Q3ADC pack instructions.txt/doc (already in baseq3/)

### Map Converter (Minecraft → Q3)
- Thread: https://www.quake3world.com/forum/viewtopic.php?t=46333
- Python tool that converts Minecraft worlds to Q3 BSP

---

## Player Models / Skins

### Best Sites

| Site | URL | Notes |
|------|-----|-------|
| **LoneBullet** | https://www.lonebullet.com/models/g/quake-3-arena-models-6622.htm | 100+ models listed. Direct file URLs work despite redirect page. |
| **ws.q3df.org** | https://ws.q3df.org/models/ | Q3DF model repo. Has Simpsons and many others. |
| **DS-Servers** | https://en.ds-servers.com/gf/quake-3-arena/models/ | Another directory. Protected downloads. |
| **ioquake3 models** | https://ioquake3.org/extras/models/ | **Currently offline** but was the best archive. |
| **archive.org** | https://archive.org/search?query=quake+3+player+models | Packs available |

### Direct Download Pattern (LoneBullet)

The redirect page shows a filename. Construct the direct URL as:
```
https://files.lonebullet.com/quake-3-arena/models/<filename-from-page>.zip
```
Example — Homer: `https://files.lonebullet.com/quake-3-arena/models/homer-simpson-for-quake-iii.zip`

This worked! The page says it's protected but the direct URL downloads fine.

### ioquake3 Models (Wayback Machine)

Many models were hosted at ioquake3.org. Try Wayback:
```
https://web.archive.org/web/*/https://ioquake3.org/files/models2/*.zip
```

Known filenames from that server:
- `q3mdl-homer-eng.zip` (1 MB)
- `q3mdl-homer3d-eng.zip` (2 MB — higher detail)

### Models We Have

| File | Model Name | Source |
|------|-----------|--------|
| `homer.pk3` | `homer/default` — Homer Simpson | LoneBullet direct |
| `z-qlta_player-models.pk3` | 45 models (see below) | archive.org |
| `md3-EarthwormJim.pk3` | EarthwormJim | — |
| `md3-bender.pk3` | Bender (Futurama) | — |
| `md3-tis.pk3` | TIS | — |

### z-qlta_player-models.pk3 — Full Model List

All 45 models in this pack (use `model <name>/default`):

```
Callisto    Flayer      Gammy       Gaunt       Khan
Megan       Morgan      Neptune     Ursula      anarki
biker       bitterman   bones       brandon     carmack
cash        crash       doom        fritzkrieg  grunt
hunter      james       janet       keel        klesk
lucy        major       mynx        orbb        paulj
pi          ranger      razor       santa       sarge
sargeproto  slash       sorlag      tankjr      tim
uriel       visor       visorproto  vixen       xaero
xian
```

### Unextracted Archive

- `models_pack.7z` (1.3 GB) — in baseq3/ — **needs p7zip to extract on Mac:**
  ```sh
  7z x baseq3/models_pack.7z -o/tmp/q3models/
  # Then copy any .pk3 files found into baseq3/
  ```
  Source: https://archive.org/details/quake-3-model-weapon-sound-mods.-7z

### Notable Models on LoneBullet (not yet downloaded)

From their listing at ~100 models:
- SpongeBob SquarePants (1.4 MB)
- Darth Vader (3.14 MB)
- Alien (2.61 MB)
- EVA-01 Anime (3.23 MB)
- Homer 3D higher-detail version (2 MB)

URL pattern: find the model page, grab filename from the "download" button href, use direct URL above.

---

## Weapons / Sounds / Other

### archive.org Pack
- `models_pack.7z` above also contains weapon models and sound mods (1.3 GB total)

---

## Setting Default Model

In `baseq3/autoexec.cfg`:
```
seta model homer/default
seta headmodel homer/default
```

Change `homer` to any model folder name from the list above.

---

## Useful Q3 Console Commands

```
\model homer/default          — change model live
\map r7-blockworld1           — load map
\cg_drawgun 0                 — hide weapon
\r_picmip 0                   — max texture quality
\com_maxfps 125               — competitive FPS cap
```
