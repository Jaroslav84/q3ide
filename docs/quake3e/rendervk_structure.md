# Quake3e Vulkan Renderer - Directory Structure

> **Source:** https://github.com/ec-/Quake3e/tree/main/code/renderervk
> **Fetched:** 2026-03-07
> **Purpose:** File listing for the Vulkan rendering backend (`code/renderervk/`).

---

## Directory: `code/renderervk/`

**Note:** In the Quake3e codebase, the Vulkan renderer lives in `code/renderervk/` (not `rendervk/`).
This is the modern Vulkan-based renderer derived from Quake-III-Arena-Kenny-Edition.

### Files (approximately 29 files + shaders subdirectory)

#### Subdirectories
| Directory | Purpose |
|---|---|
| `shaders/` | GLSL/SPIR-V shader source files for the Vulkan pipeline |

#### Vulkan Core
| File | Purpose |
|---|---|
| `vk.c` | **KEY FILE** - Core Vulkan API interface, device/instance management |
| `vk.h` | Vulkan renderer type definitions and declarations |
| `vk_vbo.c` | Vulkan vertex buffer object management |
| `vk_flares.c` | Vulkan-specific flare rendering |

#### Core Rendering (mirrors OpenGL renderer structure)
| File | Purpose |
|---|---|
| `tr_main.c` | Main renderer entry points, view setup, culling |
| `tr_backend.c` | Vulkan command buffer submission, pipeline state |
| `tr_cmds.c` | Render command buffer management |
| `tr_init.c` | Renderer initialization, cvar registration |

#### Scene Management
| File | Purpose |
|---|---|
| `tr_scene.c` | Scene graph management |
| `tr_world.c` | BSP world rendering |
| `tr_bsp.c` | BSP file loading |

#### Geometry & Surfaces
| File | Purpose |
|---|---|
| `tr_mesh.c` | MD3 mesh rendering |
| `tr_surface.c` | Surface type dispatch |
| `tr_curve.c` | Bezier curve tessellation |

#### Shading & Materials
| File | Purpose |
|---|---|
| `tr_shade.c` | Multi-pass shader rendering via Vulkan pipelines |
| `tr_shade_calc.c` | Shader parameter calculation |
| `tr_shader.c` | Shader script parsing |

#### Lighting & Effects
| File | Purpose |
|---|---|
| `tr_light.c` | Dynamic lighting |
| `tr_shadows.c` | Shadow rendering |
| `tr_sky.c` | Skybox rendering |

#### Textures & Images
| File | Purpose |
|---|---|
| `tr_image.c` | **KEY FILE** - Vulkan texture creation, upload, format management |

#### Animation & Models
| File | Purpose |
|---|---|
| `tr_animation.c` | Skeletal animation |

## Q3IDE Integration Notes

For Q3IDE texture injection via Vulkan, the critical files are:
- **`tr_image.c`** - Contains `R_CreateImage()`, `upload_vk_image()`, uses `vk_create_image()` and `vk_upload_image_data()`
- **`vk.c`** / **`vk.h`** - Core Vulkan functions including `vk_create_image()`, `vk_upload_image_data()`, `vk_update_descriptor_set()`
- **`tr_init.c`** - Renderer initialization
- **`tr_backend.c`** - Command submission, potential hook point

Key difference from OpenGL: Vulkan pre-stages all mipmap levels into a single contiguous buffer before uploading, rather than issuing per-level glTexImage2D calls.
