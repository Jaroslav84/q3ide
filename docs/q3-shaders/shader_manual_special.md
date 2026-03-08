# Quake III Arena Special Shader Effects

## Portal Shaders

Portal shaders create surfaces that can show an alternate view of the world, enabling teleporters, mirrors, and windows into other areas.

### Portal Keyword
```
{
    map $whiteimage
    portal
}
```

The `portal` keyword marks a surface as a portal and triggers `RE_RenderScene` with an alternate view matrix.

### Portal Implementation

A portal shader typically involves:

1. **Base surface**: Simple white texture with portal keyword
2. **Optional frame**: Decoration around the portal edge
3. **Portal-specific properties**: No lighting, no fog

**Complete Portal Example:**
```
textures/teleport/portal_blue
{
    surfaceparm nomarks
    sort portal
    portal

    {
        map $whiteimage
        rgbGen const ( 0.2 0.4 0.8 )
    }
    {
        map textures/teleport/portal_frame.tga
        blendfunc GL_ONE GL_ONE
        rgbGen wave sin 0.5 0.5 0 2
    }
}
```

### Portal Frame
The surface around the portal can have its own visual treatment:

```
{
    map textures/teleport/frame_edge.tga
    blendfunc GL_ONE GL_ONE
    rgbGen const ( 1.0 0.8 0.2 )
    tcMod rotate 360
}
```

## Sky Shaders

Sky shaders render the distant background using parallax-mapped sky domes or panoramic textures.

### Sky Parameters
```
skyParms <farbox> <cloudheight> <nearbox>
```

- **farbox**: Skybox texture (at distance, e.g., "env/bigsky")
- **cloudheight**: Cloud layer height relative to world (typically 256)
- **nearbox**: Optional near-plane skybox

### Complete Sky Shader

```
textures/skies/sky_day
{
    qer_editorimage textures/skies/sky_ed.tga
    surfaceparm noimpact
    surfaceparm nolightmap
    sort sky

    skyParms env/city 512 -

    {
        map textures/skies/clouds.tga
        rgbGen identity
        tcMod scroll 0.1 0.05
    }
}
```

### Sky Components

**Far Box**: Located infinitely far away, forms the horizon.

**Cloud Layer**: Closer, semi-transparent clouds scrolling across the far box.

**Near Box**: Optional, placed between camera and far box for additional depth.

## Mirror/Reflection Shaders

Mirrors use portal surfaces with special rendering logic to reflect the environment.

### Mirror Implementation

A mirror shader typically combines:

1. Portal surface for reflection rendering
2. Reflective material appearance
3. Frame/backing

**Mirror Shader:**
```
textures/mirrors/mirror_clean
{
    surfaceparm nomarks
    sort portal
    portal
    nofog

    {
        map textures/mirrors/glass_reflection.tga
        rgbGen identity
    }
    {
        map textures/mirrors/frame_wood.tga
        blendfunc GL_ONE GL_ONE
    }
}
```

## Fog Volume Shaders

Fog volumes create areas with atmospheric effects.

### Fog Parameters
```
fogparms <r> <g> <b> <density>
```

- **RGB**: Fog color (0.0-1.0 normalized)
- **Density**: Fog thickness (higher = thicker fog, typical range 0.001-0.01)

### Fog Shader Example

```
textures/fog/fog_red
{
    qer_nocarve
    surfaceparm fog
    surfaceparm trans
    surfaceparm nolightmap
    sort underwater

    fogparms ( 1.0 0.3 0.3 ) 0.005

    {
        map $whiteimage
        rgbGen const ( 1.0 0.3 0.3 )
        alphaGen const 0.1
    }
}
```

### Fog Properties

- **Must be brush entity** (func_group with fog shader applied)
- Uses `qer_nocarve` to prevent being carved by other brushes
- Combines `surfaceparm fog` and `surfaceparm trans`
- Requires water/fog content flag in radiant

## Translucent Shaders

Translucent surfaces are visible from one side but can be looked through.

### Trans Surfaceparm
```
surfaceparm trans
```

Combined with `q3map_backshader` to show different geometry from inside vs. outside.

### Translucent Window Example

```
textures/windows/stained_glass
{
    q3map_backshader textures/windows/stained_glass_back
    surfaceparm trans

    {
        map textures/windows/glass_front.tga
        alphaGen identity
        blendfunc GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA
    }
    {
        map $lightmap
        blendfunc GL_DST_COLOR GL_ZERO
    }
}

textures/windows/stained_glass_back
{
    surfaceparm trans

    {
        map textures/windows/glass_back.tga
        alphaGen identity
        blendfunc GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA
    }
}
```

## Animated Textures (Multi-Frame)

While not traditional animation, Quake III simulates animation using scrolling and wave functions.

### Scrolling Animation
```
{
    map textures/anim/scroll.tga
    tcMod scroll 0.5 0.5  // Continuous scroll effect
}
```

### Pulsing Animation
```
{
    map textures/anim/pulse.tga
    rgbGen wave sin 0.5 0.5 0 2  // Pulses on/off
    alphaGen wave sin 0.5 0.5 0 1  // Fades in/out
}
```

### Rotating Animation
```
{
    map textures/anim/spinner.tga
    tcMod rotate 360  // Full rotation per second
}
```

## Light Emitting Surfaces

Surfaces that emit light into the world.

### q3map_surfaceLight
```
q3map_surfaceLight 150
```

Makes the shader surface emit light during map compilation. The light color is derived from the texture's average color.

**Bright Light Surface:**
```
textures/lights/bright
{
    q3map_surfaceLight 300
    surfaceparm nomarks

    {
        map textures/lights/light_panel.tga
        rgbGen identity
    }
    {
        map textures/lights/glow.tga
        blendfunc GL_ONE GL_ONE
    }
}
```

### q3map_lightimage
```
q3map_lightimage textures/lights/custom_color.tga
```

Uses a specific image to determine light emission color instead of averaging the base texture.

**Custom Color Light:**
```
textures/lights/neon_blue
{
    q3map_lightimage textures/lights/neon_blue_light.tga
    q3map_surfaceLight 200

    {
        map textures/lights/neon_tube.tga
        rgbGen wave sin 0.5 0.5 0 3
    }
}
```

## Sun and Sunlight

Creates a distant directional light simulating sunlight.

### q3map_sun
```
q3map_sun <r> <g> <b> <intensity> <degrees> <elevation>
```

- **RGB**: Sun color (0.0-1.0)
- **Intensity**: Brightness (100-300 typical)
- **Degrees**: Direction (0=east, 90=north, 180=west, 270=south)
- **Elevation**: Angle above horizon (0-90 degrees)

**Usage in any shader (usually a sky shader):**
```
textures/skies/day_sun
{
    q3map_sun 1.0 0.95 0.8 200 45 60
    skyParms env/day 512 -

    {
        map textures/skies/daylight.tga
        rgbGen identity
    }
}
```

## Decal Shaders

Decals are small detail textures applied to surfaces without full texture sheets.

### Decal Properties
- Use `alphaGen identity` for texture-based transparency
- Typically use `GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA` blend
- Small file sizes (256x256 or smaller)
- Often applied in-game or in editor

**Decal Example (bullet hole):**
```
textures/decals/bullet_hole
{
    nomipmaps
    nopicmip
    surfaceparm nomarks
    sort additive

    {
        map textures/decals/bullethole.tga
        alphaGen identity
        blendfunc GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA
    }
}
```

## Invisible Geometry

Surfaces that block movement but are invisible.

### nodraw Surfaceparm
```
surfaceparm nodraw
```

Used for collision geometry, trigger volumes, and clipping brushes.

**Invisible Clip Brush:**
```
textures/common/clip
{
    qer_editorimage textures/common/clip_ed.tga
    surfaceparm nodraw
    surfaceparm playerclip
    sort nodraw
}
```

## Shader Keyword Quick Reference Table

| Keyword | Type | Purpose | Example |
|---------|------|---------|---------|
| `portal` | Renderer | Enable portal rendering | `portal` |
| `skyParms` | Q3MAP | Sky dome setup | `skyParms env/sky 512 -` |
| `fogparms` | Q3MAP | Fog volume color/density | `fogparms ( 0.5 0.5 0.5 ) 0.005` |
| `q3map_sun` | Q3MAP | Directional sunlight | `q3map_sun 1.0 0.9 0.8 200 45 60` |
| `q3map_surfaceLight` | Q3MAP | Surface emits light | `q3map_surfaceLight 150` |
| `q3map_lightimage` | Q3MAP | Custom light color | `q3map_lightimage textures/light.tga` |
| `q3map_backshader` | Q3MAP | Interior shader | `q3map_backshader textures/back` |
| `surfaceparm fog` | Q3MAP | Fog volume content | `surfaceparm fog` |
| `surfaceparm water` | Q3MAP | Water surface | `surfaceparm water` |
| `surfaceparm trans` | Q3MAP | Transparent to visibility | `surfaceparm trans` |
| `sort portal` | Renderer | Render before world | `sort portal` |
| `sort sky` | Renderer | Render very early | `sort sky` |
| `nofog` | Renderer | Immune to fog | `nofog` |
| `surfaceparm nodraw` | Q3MAP | Invisible geometry | `surfaceparm nodraw` |
| `surfaceparm playerclip` | Q3MAP | Block players | `surfaceparm playerclip` |

