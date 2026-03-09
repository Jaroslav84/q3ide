# Quake 3 — Installation Guide

## Installing Models / Maps

```sh
# .pk3 file — drop directly into baseq3/
cp model.pk3 /path/to/q3ide/baseq3/

# .zip file — extract into baseq3/
unzip model.zip -d /path/to/q3ide/baseq3/

# .7z file — requires p7zip
brew install p7zip          # macOS
apt-get install p7zip-full  # Linux
7z x archive.7z -o./baseq3/

# Verify models installed correctly
find baseq3/ -path "*/models/players/*" -type d | head -20
# Should show: ./models/players/sarge, ./models/players/anarki, etc.
```

## Set Default Model

In `baseq3/autoexec.cfg`:
```
seta model homer/default
seta headmodel homer/default
```
Replace `homer` with any model folder name. Takes effect on next launch.

## Build and Run

```sh
# Full build + launch on q3dm0
sh ./scripts/build.sh --run --level 0

# With a specific map
sh ./scripts/build.sh --run --level q3dm7

# Engine only (faster, skips Rust rebuild)
sh ./scripts/build.sh --run --level 0 --engine-only
```

---

## Troubleshooting Downloads

**archive.org slow/blocked:**
- Use "Download Options" section instead of the HTML page
- Try direct file URL: replace `/details/` with `/download/` in the URL
- Torrent versions often faster than HTTP

**ModDB requires login:**
- Free account sufficient
- Check "Alternative Downloads" section on the addon page
- Disable ad blockers if download button doesn't respond

**Network restricted (Docker/server):**
- Download on a machine with open internet
- Transfer files via USB or cloud storage
- Extract on target machine

**7z extraction (large archives):**
```sh
# Extract only player model pk3s, skip everything else
7z e archive.7z "*.pk3" -o./baseq3/

# Extract everything then copy pk3s
7z x archive.7z -o/tmp/q3extract/
find /tmp/q3extract -name "*.pk3" -exec cp {} ./baseq3/ \;
```

**Model not appearing in-game:**
- Confirm structure: `models/players/<name>/lower.md3` must exist inside the pk3
- Try `\model <name>/default` in console to load manually
- Check `\modellist` to see what Q3 detected

**q3df.org blocked (403):**
- No workaround known — try Wayback Machine snapshots:
  `https://web.archive.org/web/*/https://ws.q3df.org/models/`
