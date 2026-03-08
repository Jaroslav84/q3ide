# Quake III Arena Shader Stages

## Stage Basics

Shader stages are the fundamental building blocks of visual effects in Quake III Arena. Each stage in a shader performs a specific rendering operation, and stages are rendered in order from top to bottom.

## Stage Structure

```
textures/example/surface
{
    // Global shader keywords
    surfaceparm nomarks

    // Stage 1: Base texture
    {
        map textures/example/base.tga
        rgbGen identity
    }

    // Stage 2: Lightmap
    {
        map $lightmap
        blendfunc GL_DST_COLOR GL_ZERO
    }

    // Stage 3: Optional effect
    {
        map textures/example/effect.tga
        blendfunc GL_ONE GL_ONE
        rgbGen wave sin 0.5 0.5 0 1
    }
}
```

## Stage Processing Order

Stages are processed sequentially from the first to the last. Each stage:

1. Loads a texture via `map`
2. Modifies its color with `rgbGen`
3. Modifies its transparency with `alphaGen`
4. Applies texture coordinate transformations with `tcMod`
5. Blends with previous stages using `blendfunc`

## Texture Mapping Keywords

### map <texture>

Specifies the texture for this stage.

**Format:**
```
map textures/path/to/texture.tga
```

**Built-in Textures:**
- `$whiteimage`: Solid white texture (useful for color overlays)
- `$lightmap`: Lightmap computed during map compilation
- `*name`: Dynamic texture reference (game-specific, like `*q3ide_win0` for Q3IDE windows)

**Examples:**
```
map textures/base/metal.tga
map $lightmap
map *q3ide_win0
```

## Color Generation (rgbGen)

Controls how the RGB color of the stage is generated.

### rgbGen identity
Uses the texture color unchanged. This is the most common mode for base textures.

```
{
    map textures/base/texture.tga
    rgbGen identity
}
```

### rgbGen const (<r> <g> <b>)
Applies a constant color overlay. All values 0.0 to 1.0.

```
{
    map $whiteimage
    rgbGen const ( 0.8 0.2 0.2 )  // Red tint
}
```

### rgbGen wave <func> <base> <amplitude> <phase> <freq>
Animated color that cycles over time.

- **func**: sin, triangle, square, sawtooth, inversesawtooth
- **base**: Starting color value (0.0-1.0)
- **amplitude**: Color variation range (0.0-1.0)
- **phase**: Initial offset (0.0-1.0)
- **freq**: Cycles per second (Hz)

```
{
    map textures/hell/fire.tga
    rgbGen wave sin 0.5 0.5 0 2  // Cycles red every 0.5 seconds
}
```

### rgbGen entity
Uses the entity's color (for colored models, items, etc.). The entity's color is multiplied with the texture.

```
{
    map textures/weapons/rocket.tga
    rgbGen entity
}
```

### rgbGen oneMinusEntity
Inverts the entity color (1.0 - entity_color).

```
{
    map textures/effect/invert.tga
    rgbGen oneMinusEntity
}
```

## Alpha Generation (alphaGen)

Controls transparency of the stage.

### alphaGen identity
Uses the texture's built-in alpha channel unchanged.

```
{
    map textures/glass/window.tga
    alphaGen identity
    blendfunc GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA
}
```

### alphaGen const <value>
Constant alpha (0.0 = fully transparent, 1.0 = fully opaque).

```
{
    map $whiteimage
    alphaGen const 0.5
    blendfunc GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA
}
```

### alphaGen wave <func> <base> <amplitude> <phase> <freq>
Animated alpha that fades in and out over time.

```
{
    map textures/effect/beacon.tga
    alphaGen wave sin 0.5 0.5 0 2  // Pulses every 0.5 seconds
}
```

### alphaGen entity
Uses the entity's alpha value.

```
{
    map textures/items/glow.tga
    alphaGen entity
    blendfunc GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA
}
```

## Texture Coordinate Modification (tcMod)

Modifies the texture coordinates before rendering, creating animation effects without animating the texture itself.

### tcMod scroll <s> <t>
Scrolls texture coordinates at specified speed.

- **s**: Horizontal scroll speed (texture widths per second)
- **t**: Vertical scroll speed (texture heights per second)

```
{
    map textures/water/water.tga
    tcMod scroll 1 -2  // Scroll right at 1x, down at 2x speed
}
```

### tcMod scale <s> <t>
Scales the texture coordinates.

- **s**: Horizontal scale (1.0 = normal, 2.0 = half-sized tiles)
- **t**: Vertical scale (1.0 = normal, 0.5 = double-sized tiles)

```
{
    map textures/stone/brick.tga
    tcMod scale 2 2  // Tiles texture 4x smaller (2x2 grid)
}
```

### tcMod rotate <degrees>
Rotates texture coordinates around center.

- **degrees**: Degrees per second (positive = clockwise)

```
{
    map textures/effect/logo.tga
    tcMod rotate 45  // Rotates 45 degrees per second
}
```

### tcMod turb <base> <amplitude> <phase> <freq>
Creates turbulent/wavy distortion effect.

- **base**: Base offset (typically 0.0)
- **amplitude**: Distortion magnitude (typically 0.1-0.5)
- **phase**: Initial phase (0.0)
- **freq**: Frequency in Hz (typically 1-3)

```
{
    map textures/water/water_surface.tga
    tcMod turb 0 0.25 0 1  // Subtle water ripple effect
}
```

### tcMod stretch <func> <base> <amplitude> <phase> <freq>
Stretches texture along U/V by wave pattern.

```
{
    map textures/effect/stretch.tga
    tcMod stretch sin 1.0 0.2 0 2  // Stretches back and forth
}
```

## Blending Modes (blendfunc)

Controls how stages combine with previously rendered stages. Requires two parameters: source blend and destination blend.

### Common Blend Modes

#### GL_ONE GL_ZERO (Default)
Replaces previous content completely. Use for base texture (opaque).

```
{
    map textures/base/texture.tga
    blendfunc GL_ONE GL_ZERO
}
```

#### GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA (Standard Alpha)
Standard transparency blending. Source alpha controls opacity.

```
{
    map textures/glass/window.tga
    blendfunc GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA
    alphaGen identity
}
```

#### GL_ONE GL_ONE (Additive)
Brightens by adding source to previous. Used for glows and light effects.

```
{
    map textures/effect/glow.tga
    blendfunc GL_ONE GL_ONE
    rgbGen wave sin 0.5 0.5 0 2
}
```

#### GL_ZERO GL_SRC_COLOR (Multiply)
Darkens by multiplying. Source becomes filter/mask.

```
{
    map textures/shadow/shadow.tga
    blendfunc GL_ZERO GL_SRC_COLOR
}
```

#### GL_DST_COLOR GL_ZERO (Modulate - Lightmap Standard)
Typical lightmap blending. Multiplies lightmap with base texture.

```
{
    map $lightmap
    blendfunc GL_DST_COLOR GL_ZERO
}
```

#### GL_ONE GL_ONE_MINUS_SRC_ALPHA (Pre-multiplied Alpha)
Used for pre-multiplied alpha textures.

```
{
    map textures/effect/alpha_pre.tga
    blendfunc GL_ONE GL_ONE_MINUS_SRC_ALPHA
}
```

## Complete Multi-Stage Examples

### Water Surface
```
textures/water/water
{
    q3map_globaltexture
    surfaceparm water
    surfaceparm nolightmap

    {
        map textures/water/water_base.tga
        rgbGen identity
        tcMod scroll 0.5 0.3
    }
    {
        map textures/water/water_ripple.tga
        blendfunc GL_ONE GL_ONE
        rgbGen wave sin 0.5 0.5 0 2
        tcMod scroll -0.3 0.5
    }
    {
        map $lightmap
        blendfunc GL_DST_COLOR GL_ZERO
    }
}
```

### Glowing Window
```
textures/windows/bright_window
{
    nomipmaps
    nopicmip
    surfaceparm nomarks

    {
        map textures/windows/glass.tga
        alphaGen identity
        blendfunc GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA
    }
    {
        map textures/windows/glow.tga
        blendfunc GL_ONE GL_ONE
        alphaGen const 0.7
    }
}
```

### Lava with Animation
```
textures/lava/lava
{
    surfaceparm lava
    surfaceparm nolightmap
    q3map_surfaceLight 150

    {
        map textures/lava/lava.tga
        rgbGen wave sin 0.8 0.2 0 1
        tcMod scroll 1 -1
    }
    {
        map textures/lava/distort.tga
        blendfunc GL_ONE GL_ONE
        alphaGen const 0.3
        tcMod scroll -0.5 0.5
    }
}
```

## Stage Performance Considerations

- Each stage adds a rendering pass
- More stages = more GPU work
- Order matters: put most frequently used stages first
- Use simple textures for additive stages
- Avoid too many texture scrolls at once
- Test FPS impact of complex shaders
