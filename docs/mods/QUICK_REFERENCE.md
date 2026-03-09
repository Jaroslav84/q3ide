# Quake 3 — Quick Reference

## Get Models in 30 Seconds

1. Download: https://www.moddb.com/games/quake-iii-arena/addons/ql-playermodels-ioquake3
2. Extract pk3 → `baseq3/`
3. `sh ./scripts/build.sh --run --level 0`
4. In-game: Settings → Player → Model

## Console Commands

```
\model anarki/default    — change player model live
\map cpm2                — load map
\devmap q3dm0            — load map (cheats on)
\cg_drawgun 0            — hide weapon model
\r_picmip 0              — max texture quality
\com_maxfps 125          — competitive FPS cap
\r_gamma 1.3             — brightness
\sensitivity 5           — mouse sensitivity
\modellist               — list all loaded models
```

## q3ide Console Commands

```
q3ide attach all         — attach iTerm/Terminal windows to walls
q3ide desktop            — capture all monitors on nearest wall
q3ide list               — list capturable windows
q3ide detach             — detach all windows
q3ide status             — show active windows + dylib state
```

## autoexec.cfg Useful Settings

```
seta model sarge/default         — default player model
seta headmodel sarge/default     — default head model
seta r_picmip 0                  — max textures
seta com_maxfps 125              — FPS cap
seta sensitivity 5               — mouse speed
seta m_pitch -0.022              — inverted mouse
```

## URLs Blocked as of March 2026

```
✗ ws.q3df.org/models/           — 403 Forbidden
✗ ioquake3.org/extras/models/   — offline (use Wayback Machine)
✗ polycount.com Q3 section      — discontinued
```
