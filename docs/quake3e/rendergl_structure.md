# Quake3e OpenGL Renderer - Directory Structure

> **Source:** https://github.com/ec-/Quake3e/tree/main/code/renderer
> **Fetched:** 2026-03-07
> **Purpose:** File listing for the OpenGL renderer (`code/renderer/`), the primary GL1-based rendering backend.

---

## Directory: `code/renderer/`

**Note:** In the Quake3e codebase, the OpenGL renderer lives in `code/renderer/` (not `rendergl/`).
This is the traditional OpenGL 1.x renderer with modern extensions.

### Files (approximately 28 files)

#### Header Files
| File | Purpose |
|---|---|
| `tr_local.h` | Renderer-internal type definitions and function declarations |
| `tr_common.h` | Common renderer definitions shared across files |
| `qgl.h` | OpenGL function pointer declarations (dynamic GL loading) |
| `iqm.h` | Inter-Quake Model (IQM) format structure definitions |

#### Core Rendering
| File | Purpose |
|---|---|
| `tr_main.c` | Main renderer entry points, view setup, culling |
| `tr_backend.c` | Low-level OpenGL state management, draw call submission |
| `tr_cmds.c` | Render command buffer management |
| `tr_init.c` | Renderer initialization, cvar registration, GL context setup |

#### Scene Management
| File | Purpose |
|---|---|
| `tr_scene.c` | Scene graph management, entity/light submission |
| `tr_world.c` | BSP world rendering, visibility determination |
| `tr_bsp.c` | BSP file loading and processing |

#### Geometry & Surfaces
| File | Purpose |
|---|---|
| `tr_mesh.c` | MD3 mesh rendering |
| `tr_surface.c` | Surface type dispatch and rendering |
| `tr_curve.c` | Bezier curve/patch tessellation |
| `tr_vbo.c` | Vertex Buffer Object management for cached geometry |

#### Shading & Materials
| File | Purpose |
|---|---|
| `tr_shade.c` | Multi-pass shader rendering |
| `tr_shade_calc.c` | Shader parameter calculation (deforms, texmods, colors) |
| `tr_shader.c` | Shader script parsing and compilation |

#### Lighting & Effects
| File | Purpose |
|---|---|
| `tr_light.c` | Dynamic lighting calculations |
| `tr_shadows.c` | Stencil shadow volume rendering |
| `tr_flares.c` | Lens flare rendering |
| `tr_sky.c` | Skybox rendering |

#### Textures & Images
| File | Purpose |
|---|---|
| `tr_image.c` | **KEY FILE** - Texture loading, creation, upload (GL texture management) |

#### Animation & Models
| File | Purpose |
|---|---|
| `tr_animation.c` | Skeletal animation processing |

#### Other
| File | Purpose |
|---|---|
| `tr_arb.c` | ARB assembly shader program support |

## Q3IDE Integration Notes

For Q3IDE texture injection, the most critical files are:
- **`tr_image.c`** - Contains `R_CreateImage()`, `Upload32()`, `R_UploadSubImage()` for texture management
- **`tr_init.c`** - Renderer initialization where custom hooks could be added
- **`tr_backend.c`** - Where draw calls happen, potential hook point for custom quad rendering
- **`tr_main.c`** - Per-frame rendering entry, where custom drawing could be injected
