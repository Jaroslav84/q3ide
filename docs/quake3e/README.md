# Quake3e - Project README

> **Source:** https://github.com/ec-/Quake3e
> **Fetched:** 2026-03-07
> **Purpose:** Main project README for the Quake3e engine, a modernized Quake III Arena engine.

---

## Quake3e

This is a modern Quake III Arena engine aimed to be fast, secure and compatible with all existing Q3A mods. It builds upon the final non-SDL source release from ioquake3 with contemporary upstream corrections integrated.

Download compiled binaries from the [Releases](https://github.com/ec-/Quake3e/releases) section or consult [BUILD.md](https://github.com/ec-/Quake3e/blob/main/BUILD.md) for compilation guidance.

**Note:** This repository contains only engine code without game assets. You must place compiled binaries into an existing Quake III Arena installation to play.

### Key Features

- Optimized OpenGL rendering backend
- Optimized Vulkan rendering backend
- Native mouse input handling (automatically replaces DirectInput when available via `\in_mouse 1`)
- Unlagged mouse event processing (revert with `\in_lagged 1`)
- `\in_minimize` - window minimize/restore hotkey for Windows systems
- `\video-pipe` - external ffmpeg integration for superior video encoding
- Substantially redesigned QVM (Quake Virtual Machine) architecture
- Enhanced server-side denial-of-service protections with minimal memory footprint
- Expanded filesystem capacity (supports up to 20,000 maps per directory)
- Reimplemented Zone memory system eliminating out-of-memory failures
- Optional SDL2 backend (video, audio, input) selectable during compilation
- Extensive bugfixes and performance enhancements

## Vulkan Renderer

Derives from the Quake-III-Arena-Kenny-Edition with substantial enhancements:

- Advanced per-pixel dynamic illumination
- Rapid flare rendering (`\r_flares 1`)
- Anisotropic texture filtering (`\r_ext_texture_filter_anisotropic`)
- Minimized graphics API overhead
- Adaptive vertex buffer allocation for massive map support
- Multiple command buffer execution to eliminate processing bottlenecks
- Reversed depth technique eliminating z-fighting artifacts on expansive maps
- Consolidated lightmap atlases
- Multitexture optimization
- Cached world geometry in vertex buffer objects (`\r_vbo 1`)
- RenderDoc-compatible debug annotations
- Intel iGPU framebuffer corruption resolution
- Framebuffer rendering mode (`\r_fbo 1`) enabling:
  - Screen environment mapping for authentic reflections
  - Multisample anti-aliasing (`\r_ext_multisample`)
  - Supersampling anti-aliasing (`\r_ext_supersample`)
  - Per-window gamma adjustment (benefits streaming software like OBS)
  - Minimizable window during video recording
  - High dynamic range targets preventing color banding (`\r_hdr 1`)
  - Bloom post-processing
  - Custom resolution rendering
  - Monochrome mode

Typical performance shows 10-200%+ framerate improvement versus the original.

Optimal for contemporary systems.

## OpenGL Renderer

Based on traditional OpenGL implementations from idq3, ioquake3, cnq3, and OpenArena:

- OpenGL 1.1 base compatibility with optional modern extensions
- Superior per-pixel dynamic lighting (controlled by `\r_dlightMode`)
- Consolidated lightmap atlases
- Cached geometry in vertex buffers (`\r_vbo 1`)
- All offscreen rendering capabilities present in Vulkan renderer, plus:
- Bloom reflection post-processing

Generally matches or exceeds competing OpenGL1 implementations.

## OpenGL2 Renderer

The original ioquake3 renderer with suboptimal non-NVIDIA performance and no ongoing maintenance.

## Build Instructions

Consult [BUILD.md](https://github.com/ec-/Quake3e/blob/main/BUILD.md) for comprehensive compilation instructions.

## Contacts

Discord community: https://discordapp.com/invite/X3Exs4C

## Links

- https://bitbucket.org/CPMADevs/cnq3
- https://github.com/ioquake/ioq3
- https://github.com/kennyalive/Quake-III-Arena-Kenny-Edition
- https://github.com/OpenArena/engine

## License

GPL-2.0 (see COPYING.txt)
