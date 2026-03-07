# Quake 3 Renderer Pipeline

> **Sources:**
> - [Fabien Sanglard - Quake 3 Source Code Review: Renderer](https://fabiensanglard.net/quake3/renderer.php)
> - [Fabien Sanglard - Quake 3 Source Code Review: Architecture](https://fabiensanglard.net/quake3/)
> - [Quake III Arena Shader Manual](https://icculus.org/gtkradiant/documentation/Q3AShader_Manual/ch01/pg1_1.htm)
> - [id Tech 3 - Wikipedia](https://en.wikipedia.org/wiki/Id_Tech_3)
> - [Michael Abrash - Quake's 3-D Engine: The Big Picture](https://www.bluesnews.com/abrash/chap70.shtml)

## Overview

The Quake III renderer is built on a material-based shader system on top of the OpenGL 1.X fixed pipeline. It uses a BSP/PVS/Lightmap combination for efficient scene rendering. The renderer is implemented as a separate static library (`renderer.lib`) that is theoretically pluggable, allowing for alternate implementations (Direct3D, software, Vulkan in modern forks).

## Architecture: Frontend and Backend

The renderer is split into a **frontend** and **backend** that communicate via a Producer/Consumer design pattern.

### Frontend

The frontend is responsible for:

- **Scene setup**: Receiving render commands from the cgame VM via `RE_RenderScene`
- **Visibility determination**: BSP tree traversal and PVS (Potentially Visible Set) queries
- **Surface sorting**: Collecting and sorting draw surfaces by shader/material
- **Culling**: Frustum culling of entities and surfaces
- **Entity processing**: MD3 model animation and interpolation

### Backend

The backend is responsible for:

- **OpenGL state management**: Setting blend modes, texture bindings, depth testing
- **Draw surface rendering**: Iterating through sorted surfaces and issuing OpenGL draw calls
- **Shader stage execution**: Multi-pass rendering for complex materials
- **Lightmap application**: Combining lightmaps with diffuse textures

### SMP (Symmetric Multiprocessing)

When `r_smp` is set to 1, drawing surfaces are stored in a **double buffer** located in RAM. The main thread writes to one buffer while the backend renderer thread reads from the other. This partially eliminates the blocking nature of the OpenGL client/server model.

## Rendering Loop

The main rendering flow per frame:

1. **`CG_DRAW_ACTIVE_FRAME`** message is sent to the cgame VM
2. The cgame VM performs entity culling and prediction
3. The cgame VM calls `CG_R_RENDERSCENE` system call
4. `quake3.exe` receives the system call and invokes `RE_RenderScene`
5. The renderer frontend:
   - Walks the BSP tree using the PVS
   - Determines visible surfaces and entities
   - Sorts draw surfaces by shader sort key
   - Submits the render command list to the backend
6. The renderer backend:
   - Processes the command list
   - Sets up OpenGL state per shader/stage
   - Issues `glDrawElements` / `glDrawArrays` calls
   - Swaps buffers

## Key Source Files

| File | Purpose |
|------|---------|
| `tr_main.c` | Frontend entry, scene rendering, entity processing |
| `tr_scene.c` | Scene setup, `RE_RenderScene` implementation |
| `tr_backend.c` | Backend rendering, OpenGL state, draw calls |
| `tr_shade.c` | Shader stage rendering, multi-pass logic |
| `tr_shader.c` | Shader script parsing and management |
| `tr_image.c` | Texture/image loading and upload |
| `tr_bsp.c` | BSP loading and world surface setup |
| `tr_light.c` | Dynamic lighting calculations |
| `tr_surface.c` | Surface type rendering (world, MD3, curves) |
| `tr_cmds.c` | Render command buffer management |
| `tr_init.c` | Renderer initialization, cvar registration |

## Shader System

The graphical technology of the engine is tightly built around a **shader system** where surface appearances are defined in text-based shader scripts (`.shader` files).

### Shader Script Structure

```
textures/base_wall/concrete
{
    {
        map textures/base_wall/concrete.tga
        rgbGen identity
    }
    {
        map $lightmap
        rgbGen identity
        blendfunc GL_DST_COLOR GL_ZERO
    }
}
```

### Shader Stages

Each shader consists of one or more **stages** (also called passes). Each stage specifies:

- **Texture map**: The image to use (`map`, `animMap`, `clampMap`)
- **Blend function**: How this stage composites over previous stages (`blendfunc`)
- **Color generation**: How vertex colors are computed (`rgbGen`)
- **Alpha generation**: How alpha values are computed (`alphaGen`)
- **Texture coordinate generation**: Modifications to UV coordinates (`tcGen`, `tcMod`)
- **Alpha test**: Conditional pixel discard (`alphaFunc`)
- **Depth writing**: Whether this stage writes to the depth buffer (`depthWrite`)

### Rendering Order

Stages are rendered in order, one on top of another. The typical two-pass lightmapped surface:

1. **Pass 1**: Draw the diffuse texture
2. **Pass 2**: Draw the lightmap with `blendfunc GL_DST_COLOR GL_ZERO` (multiplicative blend)

When the hardware supports multitexturing (ARB_multitexture), both passes can be combined into a single draw call.

### Sort Keys

The `sort` keyword controls the depth sorting order:

| Sort Value | Name | Usage |
|-----------|------|-------|
| 1 | portal | Portal surfaces |
| 2 | sky | Sky surfaces |
| 3 | opaque | Default for non-blended shaders |
| 6 | banner | Translucent but not additive |
| 9 | underwater | Underwater surfaces |
| 16 | additive | Default for blended shaders |
| 98 | nearest | Closest to camera (HUD elements) |

## BSP/PVS Rendering

### Binary Space Partitioning (BSP)

The map compiler (`q3bsp.exe`) slices the map geometry into a binary tree of convex regions. At runtime:

1. The renderer determines which **leaf** the camera is in
2. It queries the **PVS** (Potentially Visible Set) for that leaf
3. Only surfaces in visible leaves are submitted for rendering

### Lightmaps

Lightmaps are precalculated by `q3light.exe` and stored in the BSP file. They represent light and shadow information as textures that are multiplied with the diffuse textures at runtime.

In the Quake3e engine, lightmaps are merged into **atlases** for improved batching performance.

## Draw Surfaces

Draw surfaces are the fundamental unit of rendering. Each draw surface represents a piece of geometry that shares the same shader/material. Draw surfaces are:

1. **Collected** during BSP traversal
2. **Sorted** by a composite sort key (shader index + sort order + entity number)
3. **Batched** where possible (consecutive surfaces with the same shader)
4. **Rendered** by the backend in sorted order

## Special Surface Types

- **Sky surfaces**: Rendered using a skybox with optional cloud layers
- **Portal surfaces**: Render-to-texture for mirrors and portals
- **Fog surfaces**: Volumetric fog applied to geometry within fog brushes
- **Cinematic surfaces**: Dynamic textures updated per-frame from ROQ video playback

## Performance Characteristics

The renderer's performance comes from:

1. **PVS culling**: Only visible geometry is processed
2. **Sort-based batching**: Minimizes OpenGL state changes
3. **Multitexture combining**: Reduces draw call count
4. **Static VBO caching** (Quake3e): World surfaces cached in vertex buffer objects
5. **SMP rendering**: Backend runs on a separate thread
