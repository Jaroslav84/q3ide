# Quake III Arena Shader Keywords Reference

## Q3MAP-Specific Keywords (Compile-Time)

### tessSize <value>
Controls surface tessellation granularity in game units, affecting triangle density.

- Applies only to solid brushes with `deformVertexes`
- Requires map recompilation for changes
- Lower values = more triangles = smoother deformation
- Typical values: 8-16 for high detail, 32-64 for lower detail

**Example:**
```
tessSize 8
```

### q3map_backshader <shadername>
Allows different shader appearance "when you are inside it looking out." Enables distinct sort orders or visual properties for brush interiors.

- Used for transparent brushes viewed from inside
- Assigns different shader to back-facing surfaces
- Useful for water and fog volumes

**Example:**
```
q3map_backshader textures/water/water_backside
```

### q3map_globaltexture
Resolves texture precision issues across adjacent brushes when using `tcMod scale` functions, though at reduced far-distance precision.

- Prevents texture alignment seams
- Trade-off: slightly lower precision at distance
- Useful for seamless tiling surfaces

**Example:**
```
q3map_globaltexture
```

### q3map_sun <red> <green> <blue> <intensity> <degrees> <elevation>
Simulates distant light sources with parameters:

- **RGB color values**: 0.0-1.0 normalized range
- **Intensity**: 100 = bright sun, higher values = brighter
- **Degrees**: 0=east, 90=north (compass direction)
- **Elevation**: Degrees from horizon (0=horizon, 90=straight up)

**Example:**
```
q3map_sun 1.0 0.95 0.8 150 45 60
```

This creates a bright, warm sun at 45° bearing and 60° elevation.

### q3map_surfaceLight <value>
Emits light from textured surfaces based on specified value and relative surface area.

- **Value**: Light intensity (e.g., 150 for typical bright surface)
- **Color**: Derived from averaged texture colors unless overridden
- Brighter surfaces emit more light per unit area
- Useful for glowing screens, lava, and light panels

**Example:**
```
q3map_surfaceLight 200
```

### q3map_lightimage <filename.tga>
Generates lighting from the average color of the TGA image specified.

- Allows custom light color sources separate from base textures
- Image average color determines light emission color
- Useful for multi-colored light sources

**Example:**
```
q3map_lightimage textures/hell/fire_light.tga
```

### q3map_lightsubdivide <value>
Defines subdivision triangle sizes for lighting calculations.

- **Default**: 120 game units
- **Sky boxes**: 256-512 (lower detail lighting)
- **Detail surfaces**: 8-32 (high detail lighting)
- Lower values = finer lighting gradients but slower compilation

**Example:**
```
q3map_lightsubdivide 32
```

## Surface Parameters (surfaceparm)

Surface parameters define how surfaces behave in gameplay and rendering.

### Navigation & Visibility
- **alphashadow**: Uses texture alpha channels for static shadow casting
- **areaportal**: Signals portal surface for area visibility optimization
- **clusterportal**: Divides map into rendering clusters
- **trans**: Visibility blocking disabled for blended surfaces (allows seeing through semi-transparent surfaces)
- **nodraw**: Invisible geometry (triggers, clips, hidden brushes)

### Material Properties
- **fog**: Surface is fog volume (combined with `surfaceparm trans`)
- **water**: Surface is water (with water physics)
- **lava**: Surface is lava (deals damage)
- **slime**: Surface is toxic slime (deals damage)
- **metalsteps**: Metal footstep sound effect
- **flesh**: Flesh/organic footstep sound effect
- **nosteps**: No footstep sounds

### Projectile & Impact
- **noimpact**: Projectiles pass through without impact effect
- **nomarks**: No bullet marks or impact marks appear
- **playerclip**: Blocks players, not projectiles (invisible barrier)
- **monsterclip**: Blocks monsters (AI), not players

### Lighting
- **nolightmap**: Surface unaffected by ambient lighting
- **fullbright**: Surface always rendered at full brightness

## Renderer Keywords (Appearance)

### cull <side>
Determines which side(s) of a surface are drawn.

- **both**: Draw both front and back faces (default)
- **back** or **backside**: Draw only back faces
- **front** or **frontside**: Draw only front faces
- **disable**: Do not cull (effectively same as "both")

**Example:**
```
cull back
```

### sort <value>
Controls render order and depth sorting.

- **portal**: Render before world geometry (for portals)
- **sky**: Render very early (sky surfaces)
- **opaque**: Standard opaque surfaces (default)
- **banner/underwater/additive**: Medium-depth surfaces
- **nearest**: Render last, on top of everything (HUD, closest elements)

**Example:**
```
sort nearest
```

### deformVertexes <type> <parameters>

Deforms brush geometry at render time. Types include:

#### wave <div> <func> <base> <amplitude> <phase> <freq>
Oscillates vertices in a wave pattern.
- **div**: Distance between wave centers (game units)
- **func**: Waveform (sin, triangle, square, sawtooth, inversesawtooth)
- **base**: Base offset (game units)
- **amplitude**: Oscillation magnitude (game units)
- **phase**: Initial phase offset (0.0-1.0)
- **freq**: Frequency (cycles per second)

**Example:**
```
deformVertexes wave 64 sin 0 4 0 1
```

#### bulge <width> <height> <speed>
Creates a bulging wave effect.
- **width**: Wavelength (game units)
- **height**: Bulge magnitude (game units)
- **speed**: Animation speed

#### move <x> <y> <z> <func> <base> <amplitude> <phase> <freq>
Moves vertices along a direction.

**Example:**
```
deformVertexes move 0 0 1 sin 0 8 0 0.5
```

## Editor-Specific Keywords

### qer_editorimage <filename.tga>
Displays a specific TGA image in the editor while mapping actual textures to its dimensions.

- Visual representation in GTKRadiant/Q3Radiant
- Does not affect in-game appearance
- Base textures (base_light, gothic_light) contain numerous uses

**Example:**
```
qer_editorimage textures/base/base_wall_ed.tga
```

### qer_nocarve
Prevents brushes from being affected by CSG (Constructive Solid Geometry) subtract operations.

- Especially useful for water and fog textures
- Protects brush from being carved by other brushes
- Maintains brush integrity in complex maps

**Example:**
```
qer_nocarve
```

### qer_trans <value>
Controls editor-view transparency between 0.0 and 1.0.

- **0.0**: Fully transparent in editor
- **1.0**: Fully opaque in editor
- No gameplay effect (editor only)
- Helps visualize semi-transparent surfaces while mapping

**Example:**
```
qer_trans 0.5
```

## Stage Keywords

Stage-specific keywords control individual texture stages within a shader.

### map <texture>
Specifies which texture to display.

- **format**: `map textures/path/filename.tga`
- **built-ins**: `$whiteimage`, `$lightmap`, `*<name>` (dynamic texture)
- Controls what texture is visible in this stage

### rgbGen <mode> [parameters]
Controls RGB color generation for the stage.

- **identity**: Use texture color unchanged
- **const (<r> <g> <b>)**: Constant color overlay
- **wave <func> <base> <amplitude> <phase> <freq>**: Animated color
- **entity**: Use entity color (for colored entities)

### alphaGen <mode> [parameters]
Controls alpha transparency.

- **identity**: Use texture alpha unchanged
- **const <value>**: Constant alpha (0.0-1.0)
- **wave <func> <base> <amplitude> <phase> <freq>**: Animated alpha
- **entity**: Use entity alpha

### blendfunc <src> <dst>
Blending mode for combining stages. Common modes:

- **GL_ONE GL_ZERO**: Opaque (replaces previous)
- **GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA**: Standard alpha blend
- **GL_ONE GL_ONE**: Additive (brightens)
- **GL_ZERO GL_SRC_COLOR**: Multiply
- **GL_DST_COLOR GL_ZERO**: Modulate (darkens with texture)

### tcMod <operation> [parameters]
Texture coordinate modification.

- **scroll <s> <t>**: Scroll texture (units per second)
- **scale <s> <t>**: Scale texture coordinates
- **rotate <degrees>**: Rotate texture (degrees per second)
- **turb <base> <amplitude> <phase> <freq>**: Turbulent distortion

## Complete Shader Example

```
textures/base/metal_wall
{
    qer_editorimage textures/base/metal_wall_ed.tga
    surfaceparm metalsteps
    sort opaque
    {
        map textures/base/metal_wall.tga
        rgbGen identity
    }
    {
        map textures/base/metal_shine.tga
        blendfunc GL_ONE GL_ONE
        rgbGen wave sin 0.5 0.5 0 2
    }
    {
        map $lightmap
        blendfunc GL_DST_COLOR GL_ZERO
    }
}
```

This creates a shiny metal surface with animated gloss highlights and proper lightmap blending.
