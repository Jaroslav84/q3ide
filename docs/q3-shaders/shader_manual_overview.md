# Quake III Arena Shader Manual: Overview

## Definition and Purpose

A shader in Quake III Arena is a short text script that defines the properties of a surface as it appears and functions in a game world. These scripts grant designers and artists considerable control over texture surface qualities within the game environment.

## File Organization and Naming

Shader scripts reside in the `quake3/baseq3/scripts` directory. Naming conventions typically mirror texture file paths (such as base, hell, or castle), though they can be independent identifiers. Use lowercase filenames with forward slashes for cross-platform compatibility, particularly between Windows and Unix systems where case sensitivity differs.

## Core Structural Elements

A shader file contains surface attributes and rendering instructions enclosed in braces. The structure begins with a shader name (maximum 63 characters), followed by global parameters and processing stages.

Example structure:
```
textures/base/base_wall
{
    // Global shader parameters here

    {
        // Stage 1
        map textures/base/base_wall.tga
        rgbGen identity
    }
    {
        // Stage 2 (optional, for effects)
        map $lightmap
        blendfunc GL_DST_COLOR GL_ZERO
    }
}
```

## Two Classes of Keywords

Shader keywords fall into two primary categories:

### Global Parameters
- Processed by Q3MAP during map compilation
- Alter physical surface attributes affecting gameplay
- Require map recompilation to take effect
- Include: `q3map_sun`, `q3map_surfaceLight`, `surfaceparm`, `tessSize`

### Renderer Keywords
- Create appearance-only changes
- Take effect upon level transitions or console commands like `vid_restart`
- Include: `cull`, `deformVertexes`, `sort`, texture blend modes

## Key Technical Concepts

### Surface, Content, and Deformation Effects

- **Surface Effects**: Glows, transparency, and visual appearances
- **Content Effects**: Determine brush behavior (water, fog, lava, slime)
- **Deformation Effects**: Alter brush geometry at render time

### Performance Considerations

Each shader stage requires an additional rendering pass. Layered effects compound processing demands—for example, transparent fog over a light effect requires three rendering cycles. Optimize shader complexity for target hardware.

### Color Representation

The engine normalizes RGB values to a 0.0-1.0 range rather than the standard 0-255 scale. When converting colors:
- Black = 0.0
- White = 1.0
- For 8-bit (0-255): divide by 255 to get normalized value
- For 16-bit (0-65535): divide by 65535 to get normalized value

## Measurement Systems

Three measurement types structure shader parameters:

### Game Units
- Eight units equal one foot in-world
- Used for size, deformation, and positioning parameters
- Map coordinates typically range 0-32768 game units per axis

### Color Units
- Range from 0.0 (black) to 1.0 (unchanged)
- Linear RGB color space
- Represents intensity or blend factor

### Texture Units
- Normalized coordinates where 1.0 represents a complete texture
- Used for texture coordinate modifications (tcMod)
- Range from 0.0 to 1.0 for standard single-pass texturing

## Waveform Functions

Five waveform types modulate shader properties over time:

### 1. Sine Wave
- Smooth oscillation
- Range: -1.0 to 1.0
- Natural, wave-like motion
- Parameters: base, amplitude, phase (0.0-1.0), frequency (Hz)

### 2. Triangle Wave
- Sharp peaks and valleys
- Range: 0.0 to 1.0
- Used for pulsing effects
- Parameters: base, amplitude, phase, frequency

### 3. Square Wave
- Instantaneous switching
- Range: 0.0 to 1.0
- Creates strobing effects
- Parameters: base, amplitude, phase, frequency

### 4. Sawtooth Wave
- Linear ascent, sharp descent
- Range: 0.0 to 1.0
- Used for cycling animations
- Parameters: base, amplitude, phase, frequency

### 5. Inverse Sawtooth Wave
- Sharp ascent, linear descent
- Range: 0.0 to 1.0
- Reverse cycling animation
- Parameters: base, amplitude, phase, frequency

Each waveform involves configurable base values, amplitude (peak variation), phase shifts (0.0-1.0), and frequency parameters controlling cycle repetitions per second.

## Example: Lava Shader with Effects

```
textures/hell/hellfire
{
    qer_editorimage textures/hell/hellfire.tga
    surfaceparm nomarks
    surfaceparm lava
    deformVertexes wave 100 sin 0 4 0 0.5
    q3map_surfaceLight 150
    {
        map textures/hell/hellfire.tga
        rgbGen wave sin 0.5 0.5 0 1
        tcMod scroll 1 -1
    }
    {
        map $lightmap
        rgbGen identity
        blendfunc GL_DST_COLOR GL_ZERO
    }
}
```

This example demonstrates vertex deformation, dynamic color waves, texture scrolling, and multi-stage rendering for a realistic lava surface.
