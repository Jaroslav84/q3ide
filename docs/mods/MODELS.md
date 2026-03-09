# Quake 3 — Player Models

## Download Sources

| Site | Size | URL | Notes |
|------|------|-----|-------|
| **ModDB — QL Playermodels** | 180 MB | https://www.moddb.com/games/quake-iii-arena/addons/ql-playermodels-ioquake3 | 30+ Quake Live models, bot support. **Best starting point.** |
| **ModDB — Model Pack v2** | 135 MB | https://www.moddb.com/games/quake-iii-arena/addons/quake-3-model-pack | Additional models |
| **ModDB — Remastered RUNO** | — | https://www.moddb.com/mods/quake-iii-arena-remastered-runo-edition | HD models + textures + effects full overhaul |
| **CZ45 bundle** | — | https://www.moddb.com/mods/cz45modbundle | 100+ addon skins (most with bot support), 20+ maps, HD weapons. Updated Nov 2024. |
| **LoneBullet** | varies | https://www.lonebullet.com/models/g/quake-3-arena-models-6622.htm | 50+ individual models |
| **archive.org — mods pack** | 1.3 GB | https://archive.org/details/quake-3-model-weapon-sound-mods.-7z | Player models + weapon models + sounds. 7z format. |
| **archive.org — CPMA/QL** | 170 MB | https://archive.org/details/quake-3-includes-defrag-and-cpma | qlive-teamarena.zip + HD Mod.zip |
| **archive.org — anime** | 487 KB | https://archive.org/details/anime_quake_3_models | Cel-shaded: Mezzo Forte, Kite Sawa, Chobit, Mahoro, Akari, Henrietta, Nakoruru |
| **Steam Workshop** | — | https://steamcommunity.com/workshop/filedetails/?id=1488263571 | Requires Steam app |
| **ioquake3** | — | https://ioquake3.org/extras/models/ | Catalog visible; direct downloads currently offline |
| **ws.q3df.org** | — | https://ws.q3df.org/models/ | Most comprehensive — currently 403 Forbidden |

**Recommended download order:**
1. ModDB QL Playermodels (180 MB) — covers most needs
2. archive.org CPMA/QL collection (170 MB) — additional variety
3. LoneBullet — pick individual favorites
4. CZ45 — 100+ extra skins

### LoneBullet direct URL pattern
The redirect page shows a filename. Direct URL:
```
https://files.lonebullet.com/quake-3-arena/models/<filename-from-page>.zip
```
Example: `https://files.lonebullet.com/quake-3-arena/models/homer-simpson-for-quake-iii.zip`

### ioquake3 via Wayback Machine
```
https://web.archive.org/web/*/https://ioquake3.org/files/models2/*.zip
```
Known filenames: `q3mdl-homer-eng.zip` (1 MB), `q3mdl-homer3d-eng.zip` (2 MB)

---

## Models in baseq3/

| File | Contents |
|------|----------|
| `homer.pk3` | Homer Simpson (LoneBullet direct) |
| `z-qlta_player-models.pk3` | 45 models — see list below |

### z-qlta_player-models.pk3 — all 45 models

```
Callisto  Flayer    Gammy     Gaunt     Khan
Megan     Morgan    Neptune   Ursula    anarki
biker     bitterman bones     brandon   carmack
cash      crash     doom      fritzkrieg grunt
hunter    james     janet     keel      klesk
lucy      major     mynx      orbb      paulj
pi        ranger    razor     santa     sarge
sargeproto slash    sorlag    tankjr    tim
uriel     visor     visorproto vixen    xaero
xian
```

Use in-game: `model <name>/default` (e.g. `model anarki/default`)

---

## Notable Models Not Yet Downloaded

From LoneBullet: SpongeBob SquarePants (1.4 MB), Darth Vader (3.14 MB), Alien (2.61 MB),
EVA-01 Anime (3.23 MB), Homer 3D higher-detail (2 MB), Cloud Strife (3.24 MB), Chloe (3.88 MB)

---

## Model Technical Specs

- **Format:** MD3 (Quake 3 native), packaged as `.pk3` (ZIP-compatible)
- **Structure:** `models/players/<name>/head.md3 + lower.md3 + upper.md3 + model.shader`
- **LOD:** High 800 faces / Medium 500 / Low 300
- All MD3 models work with Quake3e engine
