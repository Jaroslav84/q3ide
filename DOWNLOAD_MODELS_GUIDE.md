# How to Download and Install Quake 3 HD Player Models

## Quick Start (Recommended)

### Option 1: ModDB (Most Complete - 180MB)
**Best for**: Getting all Quake Live models with bot support

1. Visit: https://www.moddb.com/games/quake-iii-arena/addons/ql-playermodels-ioquake3
2. Click "Download" button
3. Extract the pk3 file to `/root/Projects/q3ide/baseq3/`
4. Restart game engine

**What you get**: 30+ Quake Live player models with Sport/Bright skins, Team Arena models, Reaper, beta Sarge/Visor, all with bot support.

### Option 2: Steam Workshop (Easy if you own Q3A)
**Best for**: Easy automatic installation if you own Quake 3 on Steam

1. Visit: https://steamcommunity.com/workshop/filedetails/?id=1488263571
2. Click "Subscribe"
3. Game automatically downloads and manages files
4. Files typically go to Steam's workshop directory

### Option 3: Archive.org Collections (Historical)
**Best for**: Complete preservation archives with multiple packs

**Player Models Collection**:
- URL: https://archive.org/details/quake-3-includes-defrag-and-cpma
- Look for: "Player Models qlive-teamarena.zip" (170.6 MB)
- Download: Click the .zip file in the download section

**Anime Models Collection**:
- URL: https://archive.org/details/anime_quake_3_models
- Download: As .zip or individual files
- Models: Mezzo, Kite, Chobit, Mahoro, Akari, Henrietta, Nakoruru

**Full Model/Weapon/Sound Pack**:
- URL: https://archive.org/details/quake-3-model-weapon-sound-mods.-7z
- Format: 1.3GB 7z archive
- Tools needed: 7z (apt-get install p7zip-full on Linux)

### Option 4: LoneBullet (Individual Models)
**Best for**: Picking specific models you like

Browse: https://www.lonebullet.com/models/g/quake-3-arena-models-6622.htm

Popular models available:
- bunker (3.18 MB)
- chloe (3.88 MB)
- cloud strife (3.24 MB)
- dita (5.72 MB)
- And 50+ more on multiple pages

Click "Download" for each model you want.

### Option 5: CZ45's Repository (100+ Skins)
**Best for**: Massive collection with bot support

1. Visit: https://www.moddb.com/mods/cz45modbundle
2. Browse addons for individual skins or full bundles
3. Download individual pk3 files
4. Extract to baseq3/

**What's included**: 100+ addon skins (most with bots), 20+ maps, HD weapon remake.

## Manual Installation Steps

Once you've downloaded a model pack (pk3 or zip):

```bash
# Copy to baseq3 directory
cp your_model_pack.pk3 /root/Projects/q3ide/baseq3/

# OR if it's a zip file, extract it:
cd /root/Projects/q3ide/baseq3/
unzip your_model_pack.zip

# Verify structure (should see models/players/*)
ls -la models/players/
```

## Verify Installation

```bash
# Check what models are available
cd /root/Projects/q3ide/baseq3/
find . -type d -path "*/models/players/*" | sort

# You should see entries like:
# ./models/players/sarge
# ./models/players/visor
# ./models/players/xaero
# etc.
```

## Starting the Game with Models

```bash
# From q3ide directory, build and run with level loaded
sh ./scripts/build.sh --run --level 0

# In-game, set your player model:
# Menu: Settings → Player → Model → [Select character]
# OR console: model <charactername>
```

## Troubleshooting

### Models not showing
- Ensure pk3 files are in `baseq3/` (not in subdirectories)
- Check console for "couldn't load model" errors
- Restart engine after adding models

### File extraction issues
```bash
# If you have a zip file:
apt-get install unzip
cd baseq3/
unzip -t model_pack.zip  # Test archive first
unzip model_pack.zip     # Extract

# If you have a 7z file:
apt-get install p7zip-full
7z x model_pack.7z -o./

# If you have a pk3 file (it's just a renamed zip):
cd baseq3/
cp model_pack.pk3 model_pack.zip
unzip model_pack.zip
```

### Archive.org access
If direct download fails, try:
1. Use archive.org's "Download Options" menu
2. Try the torrent version (if available)
3. Check if files are available in the "BROWSE THE ARCHIVE" section
4. Some files have CDN direct links that work better

### ModDB downloads
If ModDB page loads but no download appears:
1. Try without JavaScript/extensions blocking
2. Check "Alternative Downloads" section
3. ModDB may require account (free)

## Directory Structure After Installation

Expected layout in baseq3/:
```
baseq3/
├── models/
│   └── players/
│       ├── sarge/
│       │   ├── head.md3
│       │   ├── lower.md3
│       │   ├── upper.md3
│       │   ├── model.shader
│       │   └── texture.jpg (or .tga)
│       ├── visor/
│       ├── xaero/
│       ├── quake_live_model_1/
│       └── [other models...]
├── model_pack.pk3  (or individual pk3 files)
└── [other game files]
```

## Network Alternative

If downloads fail due to network restrictions:
1. Download on another machine with open internet
2. Transfer via USB or cloud storage
3. Extract to baseq3/ on the q3ide machine
4. Verify with `find . -path "*/models/players/*"`

## Next Steps

Once models are installed:
1. Launch game: `sh ./scripts/build.sh --run --level 0`
2. Join multiplayer or bot match
3. In Settings → Player, select your preferred model
4. Models should display in HUD, third-person view, and captured screens

For Q3IDE window capture integration:
- Models will be visible in-game on all three monitors
- Screencapturekit should capture them in window attachment view
- Use q3ide console commands to test: `q3ide status`, `q3ide attach all`
