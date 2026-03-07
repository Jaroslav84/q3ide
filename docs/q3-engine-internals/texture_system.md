# Quake 3 Texture System

> **Sources:**
> - [Quake III Arena Source Code - tr_image.c](https://github.com/id-Software/Quake-III-Arena/blob/master/code/renderer/tr_image.c)
> - [Fabien Sanglard - Quake 3 Renderer Review](https://fabiensanglard.net/quake3/renderer.php)
> - [Quake III Arena Shader Manual](https://icculus.org/gtkradiant/documentation/Q3AShader_Manual/ch01/pg1_1.htm)
> - [id Tech 3 - Wikipedia](https://en.wikipedia.org/wiki/Id_Tech_3)

## Overview

The Quake 3 texture system manages loading, uploading, and referencing of all textures (called "images" internally) used by the renderer. The system supports 32-bit RGBA textures, lightmaps, dynamic cinematic textures, and various texture formats. The primary implementation resides in `tr_image.c`.

## Image Management

### image_t Structure

All textures in the engine are represented by the `image_t` structure, which contains:

- **Texture name**: File path or identifier string
- **OpenGL texture object**: The `texnum` (GL texture ID)
- **Dimensions**: `width`, `height`, `uploadWidth`, `uploadHeight`
- **Mipmap flag**: Whether mipmaps are generated
- **Wrap mode**: `GL_CLAMP` or `GL_REPEAT`
- **Frame usage tracking**: `frameUsed` counter to track which textures are active per frame

### Image Hash Table

Images are stored in a hash table for fast lookup by name. When a shader requests a texture, the system first checks the hash table before loading from disk.

## Texture Upload Pipeline

### R_CreateImage

`R_CreateImage` is the primary function for creating a new texture:

```c
image_t *R_CreateImage(const char *name, const byte *pic, int width, int height,
                       qboolean mipmap, qboolean allowPicmip,
                       int glWrapClampMode);
```

The function:

1. Allocates an `image_t` structure
2. Sets up texture parameters (name, width, height, wrap mode)
3. Generates a GL texture object (`qglGenTextures`)
4. Binds the texture with `GL_Bind(image)`
5. Calls `Upload32` to upload pixel data to OpenGL
6. Sets texture wrap parameters with `qglTexParameterf`

### Upload32 (GL_Upload32)

The core upload function that handles:

1. **Resampling**: Scales textures to power-of-2 dimensions if needed
2. **Picmip**: Reduces texture resolution based on `r_picmip` cvar
3. **Mipmap generation**: Generates mipmap chain for filtered textures
4. **Format selection**: Chooses internal format (GL_RGB, GL_RGBA, compressed variants)
5. **glTexImage2D call**: Actual GPU upload using `GL_RGBA` with `GL_UNSIGNED_BYTE`

```c
qglTexImage2D(GL_TEXTURE_2D, miplevel, internalFormat,
              scaledWidth, scaledHeight, 0,
              GL_RGBA, GL_UNSIGNED_BYTE, scaledBuffer);
```

### GL_Upload8

Handles uploading 8-bit paletted textures by converting them to 32-bit RGBA first, then calling `Upload32`.

## Texture Formats

The engine supports loading textures from:

- **TGA** (Targa): Primary format, supports 24-bit and 32-bit
- **JPEG**: Compressed format for larger textures
- **PCX**: Legacy format (limited support)

## Shader-Based Textures

### Texture Maps in Shaders

Shader stages reference textures through several keywords:

```
map textures/base_wall/concrete.tga    // Standard texture map
map $lightmap                          // Lightmap (auto-assigned)
map $whiteimage                        // Solid white (1x1)
animMap 10 tex1.tga tex2.tga tex3.tga  // Animated sequence
clampMap textures/decals/blood.tga     // Clamped (no repeat)
```

### Special Textures

| Name | Purpose |
|------|---------|
| `$lightmap` | References the surface's lightmap |
| `$whiteimage` | 1x1 white pixel, used for solid colors |
| `*scratch` | Scratch texture for cinematic playback |
| `*white` | White image alias |
| `*default` | Default texture (grey checkerboard) |

## Lightmap System

Lightmaps are loaded from the BSP file during map load:

1. Each lightmap is 128x128 pixels
2. Stored as RGB (24-bit) in the BSP lightmap lump
3. Uploaded as individual textures
4. Referenced via `$lightmap` in shader stages
5. In Quake3e, lightmaps are merged into **atlases** for better batching

## Dynamic Textures

### Cinematic Textures (RE_StretchRaw)

The engine supports dynamic per-frame texture updates through the cinematic system:

```c
void RE_StretchRaw(int x, int y, int w, int h,
                   int cols, int rows,
                   const byte *data, int client,
                   qboolean dirty);
```

`RE_StretchRaw` is called by `CIN_DrawCinematic` after translating a ROQ video frame:

1. Receives raw 8-bit pixel data from the cinematic decoder
2. Converts the byte array into a 16-bit image
3. Adds a "stretch raw" command to the backend rendering list
4. The backend renders it as a textured quad

### RE_UploadCinematic

Used for cinematic textures applied to world surfaces (rather than fullscreen):

1. Takes raw pixel data from the cinematic decoder
2. Uploads directly to an existing GL texture object
3. Allows in-game surfaces to display playing video

### Key Insight for Q3IDE

The cinematic texture path (`RE_StretchRaw` / `RE_UploadCinematic`) demonstrates that Quake 3 already has infrastructure for per-frame texture updates. The Q3IDE capture system can follow a similar pattern:

1. Create a dedicated texture with `R_CreateImage`
2. Each frame, upload new pixel data via `glTexSubImage2D` (more efficient than `glTexImage2D` for same-size updates)
3. Reference this texture in a custom shader applied to a wall surface

## Texture Memory Management

### Hunk Allocator

Texture pixel data loaded from disk is allocated from the **Hunk allocator**, which handles large, long-lived allocations from pak files (geometry, maps, textures, animations). This memory is freed when the map is unloaded.

### GPU Memory

GPU texture memory is managed by OpenGL. The engine tracks:

- Total textures loaded (`tr.numImages`)
- Frame-based usage (`image->frameUsed`) to identify unused textures
- Texture dimensions after upload (may differ from source due to picmip/resampling)

## Texture Coordinate Generation

The shader system supports multiple texture coordinate generation modes:

| tcGen Mode | Description |
|-----------|-------------|
| `tcGen base` | Use surface UV coordinates (default) |
| `tcGen lightmap` | Use lightmap UV coordinates |
| `tcGen environment` | Spherical environment mapping |
| `tcGen vector` | Project from world-space vectors |

### Texture Coordinate Modification (tcMod)

| tcMod | Description |
|-------|-------------|
| `tcMod scroll <sSpeed> <tSpeed>` | Scroll texture coordinates |
| `tcMod scale <sScale> <tScale>` | Scale texture coordinates |
| `tcMod rotate <degsPerSec>` | Rotate texture coordinates |
| `tcMod turb <base> <amp> <phase> <freq>` | Turbulence distortion |
| `tcMod stretch <func> <base> <amp> <phase> <freq>` | Stretch/compress |
| `tcMod transform <m00> <m01> <m10> <m11> <t0> <t1>` | Arbitrary matrix |

## Frame-Based Texture Tracking

The renderer tracks texture usage per frame:

```c
image->frameUsed = tr.frameCount;
```

This is used to determine which textures were actually referenced during rendering, which could be useful for diagnostics and memory optimization.
