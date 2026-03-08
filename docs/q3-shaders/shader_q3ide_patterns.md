# Q3IDE Custom Shader Patterns

## Glass Material Shader

A semi-transparent glass shader that renders screen content through it with proper blending and opacity.

```
textures/q3ide/glass_window
{
    nomipmaps
    nopicmip
    surfaceparm nomarks
    {
        map $whiteimage
        blendfunc GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA
        rgbGen const ( 0.15 0.15 0.2 )
        alphaGen const 0.85
    }
    {
        map *q3ide_win0
        blendfunc GL_ONE GL_ONE_MINUS_SRC_ALPHA
        rgbGen identity
    }
}
```

**Usage:**
- Stage 1: Semi-transparent dark blue base (glass tint)
- Stage 2: Screen content texture with additive blending
- **Parameters:**
  - `nomipmaps/nopicmip`: Disables scaling for clarity
  - `nomarks`: No bullet holes on windows
  - `alphaGen const 0.85`: Window is mostly opaque (85% visible glass)
  - `rgbGen const`: Glass tint color (0.15, 0.15, 0.2 = dark blue-grey)

**Customization:**
- Change `rgbGen const` values to tint glass (higher = more transparent, lower = darker)
- Adjust `alphaGen const` for window opacity (0.0 = fully transparent, 1.0 = fully opaque)
- Multiple windows: Use `*q3ide_win1`, `*q3ide_win2`, etc. (slots 0-15 available)

## Window Surface Shader (Per-Slot, Slots 0-15)

A self-illuminated window shader for displaying captured screen content. Each window uses its own slot.

```
textures/q3ide/window0
{
    nomipmaps
    nopicmip
    {
        map *q3ide_win0
        rgbGen identity
    }
}
```

**Variants for other slots:**
```
textures/q3ide/window1
{
    nomipmaps
    nopicmip
    {
        map *q3ide_win1
        rgbGen identity
    }
}

textures/q3ide/window2
{
    nomipmaps
    nopicmip
    {
        map *q3ide_win2
        rgbGen identity
    }
}
```

**Usage:**
- Direct window display without glass overlay
- No lightmap applied (self-illuminated)
- Slot pattern: `*q3ide_win[0-15]` for 16 windows max
- **No blending**: Uses default `GL_ONE GL_ZERO` (replaces)
- **Key properties:**
  - `nomipmaps`: No mipmapping for clarity
  - `nopicmip`: Ignore r_picmip cvar (always sharp)
  - `rgbGen identity`: Use texture colors as-is
  - No `alphaGen` (full opacity)

**Customization:**
- Add `rgbGen wave` to animate window brightness
- Add `blendfunc` for transparency effects
- Multiple stages for window decorations

## Billboard Shader (Always Bright, No Fog)

A shader for screen content that floats as a bright billboard, unaffected by fog or world lighting.

```
textures/q3ide/billboard
{
    nomipmaps
    nopicmip
    nofog
    sort nearest
    {
        map *q3ide_win0
        rgbGen identity
        blendfunc GL_ONE GL_ZERO
    }
}
```

**Usage:**
- Floating window displays (HUD-like elements)
- Brightest visual element on screen
- **Key properties:**
  - `sort nearest`: Render last (on top of everything)
  - `nofog`: Not affected by fog volumes
  - `blendfunc GL_ONE GL_ZERO`: Full replacement (no alpha needed)
  - `nomipmaps/nopicmip`: Sharp rendering at all distances

**Customization:**
- Change `sort nearest` to `sort additive` for glow effect
- Add `alphaGen` for transparency
- Add `tcMod` for animation

## Portal Shader

A shader that creates a portal surface showing an alternate view of the world.

```
textures/q3ide/portal
{
    nomipmaps
    sort portal
    portal
    {
        map $whiteimage
        rgbGen identity
    }
}
```

**Usage:**
- Create doorways or windows showing other areas
- Teleporter surfaces
- Mirrors with portal effect
- **Key properties:**
  - `portal` keyword: Enables alternate view rendering
  - `sort portal`: Renders before world geometry
  - `$whiteimage`: Simple placeholder (content shown via portal rendering)
  - No lightmap (portal content is pre-rendered)

**Customization:**
- Add decorative frame stages:
  ```
  {
      map textures/q3ide/portal_frame.tga
      blendfunc GL_ONE GL_ONE
      rgbGen wave sin 0.5 0.5 0 2
  }
  ```
- Adjust portal edge appearance
- Combine with lighting for glowing portals

## Key Shader Keywords for Q3IDE

### Display Keywords
- **`nomipmaps`**: No mipmapping, required for screen content
  - Preserves sharpness of window textures
  - Essential for readable text/UI

- **`nopicmip`**: Ignore r_picmip cvar, never downscale
  - Overrides r_picmip (texture detail setting)
  - Keeps windows always sharp regardless of user settings

### Sorting Keywords
- **`sort portal`**: Render before world geometry
  - Used for portals and special effects
  - Renders after sky but before opaque surfaces

- **`sort nearest`**: Render last, on top of everything
  - HUD and closest elements
  - Highest rendering priority

- **`sort additive`**: Medium-priority additive surfaces
  - Glows and transparent overlays

### Surface Properties
- **`surfaceparm nomarks`**: No bullet marks on windows
  - Prevents visual damage to window surfaces
  - Keeps glass appearance clean

- **`nofog`**: Not affected by fog volumes
  - Windows remain visible through fog
  - HUD-like appearance

### Texture Keywords
- **`portal`**: Marks surface as portal, triggers RE_RenderScene
  - Creates alternate view rendering
  - Essential for portal effects

- **`$whiteimage`**: Engine built-in white texture
  - Use for solid color overlays
  - Lightweight, no file I/O needed

- **`map *q3ide_winN`**: Reference scratchImage slot N (dynamic, uploaded per-frame)
  - Slots 0-15 available (16 windows max)
  - Updated every frame by engine
  - Contains captured window content

### Blending Keywords
- **`blendfunc GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA`**: Standard alpha blend
  - Source alpha controls opacity
  - Result = (Source * SrcAlpha) + (Dest * (1 - SrcAlpha))
  - Use for transparent windows

- **`blendfunc GL_ONE GL_ONE`**: Additive blending
  - Brightens without darkening
  - Result = Source + Destination
  - Use for glows and bright overlays

- **`blendfunc GL_ONE GL_ZERO`**: Opaque (replacement)
  - No blending, replaces background
  - Default for non-transparent surfaces

### Color Generation
- **`rgbGen const (r g b)`**: Constant color regardless of lighting
  - Apply tint or overlay color
  - Values 0.0-1.0 normalized
  - Example: `rgbGen const ( 0.15 0.15 0.2 )` = dark blue

- **`rgbGen identity`**: Use texture colors as-is
  - No color modification
  - Default for normal rendering

- **`rgbGen wave <func> <base> <amplitude> <phase> <freq>`**: Animated color
  - Creates pulsing/fading effects
  - Example: `rgbGen wave sin 0.5 0.5 0 2` = pulses every 0.5 seconds

### Alpha Generation
- **`alphaGen const A`**: Constant alpha (0.0-1.0)
  - 0.0 = fully transparent
  - 1.0 = fully opaque
  - 0.5 = 50% transparent

- **`alphaGen identity`**: Use texture alpha channel
  - Per-pixel transparency from texture
  - Requires texture with alpha channel

## Q3IDE Window Texture Slots Reference

Q3IDE provides 16 dynamic texture slots for window content:

| Slot | Shader Reference | Purpose |
|------|------------------|---------|
| 0 | `*q3ide_win0` | First window |
| 1 | `*q3ide_win1` | Second window |
| 2 | `*q3ide_win2` | Third window |
| 3 | `*q3ide_win3` | Fourth window |
| 4 | `*q3ide_win4` | Fifth window |
| 5 | `*q3ide_win5` | Sixth window |
| 6 | `*q3ide_win6` | Seventh window |
| 7 | `*q3ide_win7` | Eighth window |
| 8 | `*q3ide_win8` | Ninth window |
| 9 | `*q3ide_win9` | Tenth window |
| 10 | `*q3ide_win10` | Eleventh window |
| 11 | `*q3ide_win11` | Twelfth window |
| 12 | `*q3ide_win12` | Thirteenth window |
| 13 | `*q3ide_win13` | Fourteenth window |
| 14 | `*q3ide_win14` | Fifteenth window |
| 15 | `*q3ide_win15` | Sixteenth window |

**Maximum 16 windows** can be displayed simultaneously. Each texture is updated per-frame from captured window content.

## Complete Q3IDE Shader Pack

A minimal complete shader file for Q3IDE window texturing:

```
// Glass windows (semi-transparent with screen)
textures/q3ide/glass_window
{
    nomipmaps
    nopicmip
    surfaceparm nomarks
    {
        map $whiteimage
        blendfunc GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA
        rgbGen const ( 0.15 0.15 0.2 )
        alphaGen const 0.85
    }
    {
        map *q3ide_win0
        blendfunc GL_ONE GL_ONE_MINUS_SRC_ALPHA
        rgbGen identity
    }
}

// Window surface slots 0-3
textures/q3ide/window0
{
    nomipmaps
    nopicmip
    {
        map *q3ide_win0
        rgbGen identity
    }
}

textures/q3ide/window1
{
    nomipmaps
    nopicmip
    {
        map *q3ide_win1
        rgbGen identity
    }
}

textures/q3ide/window2
{
    nomipmaps
    nopicmip
    {
        map *q3ide_win2
        rgbGen identity
    }
}

textures/q3ide/window3
{
    nomipmaps
    nopicmip
    {
        map *q3ide_win3
        rgbGen identity
    }
}

// Bright billboard display
textures/q3ide/billboard
{
    nomipmaps
    nopicmip
    nofog
    sort nearest
    {
        map *q3ide_win0
        rgbGen identity
        blendfunc GL_ONE GL_ZERO
    }
}

// Portal shader
textures/q3ide/portal
{
    nomipmaps
    sort portal
    portal
    {
        map $whiteimage
        rgbGen identity
    }
}
```

## Shader File Organization

Save shaders in: `baseq3/scripts/q3ide.shader`

**File structure:**
```
// Q3IDE Custom Shaders
// Auto-generated or manually maintained
// Contains all window texture definitions

textures/q3ide/...
```

**Loading:**
- Engine reads all .shader files in `baseq3/scripts/`
- Shader files are not map-specific
- Changes take effect on `vid_restart` or `map_restart`

## Performance Notes

- Window textures (scratchImage slots 0-15) updated every frame
- Use `nomipmaps` to disable mipmapping (saves VRAM on window textures)
- Additive blending (`GL_ONE GL_ONE`) is fast, good for glows
- Alpha blending (`GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA`) requires depth sorting
- Minimize shader stages for window surfaces
- Glass overlay adds one extra rendering pass

