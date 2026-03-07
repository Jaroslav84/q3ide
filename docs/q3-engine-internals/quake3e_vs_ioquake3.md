# Quake3e vs ioquake3: Features and Differences

> **Sources:**
> - [Quake3e GitHub Repository](https://github.com/ec-/Quake3e)
> - [Quake3e README](https://github.com/ec-/Quake3e/blob/master/README.md)
> - [ioquake3 GitHub Repository](https://github.com/ioquake/ioq3)
> - [ioquake3 Website](https://ioquake3.org/)
> - [lspeed.org - ioquake3 / Quake3e / OSP / CPMA / CNQ3](https://lspeed.org/2020/04/ioquake3-quake3e-osp-cpma-cnq3/)
> - [Steam Community Discussion - Quake3e](https://steamcommunity.com/app/2200/discussions/0/4513254648328082055/)
> - [id Tech 3 - Wikipedia](https://en.wikipedia.org/wiki/Id_Tech_3)

## Overview

Both **Quake3e** and **ioquake3** are forks of the original id Tech 3 engine source code released under GPL v2. They take different approaches:

- **ioquake3**: Community-maintained, focuses on cross-platform support, security, and modern OS integration
- **Quake3e**: Performance-focused fork by "ec-", based on the last non-SDL source dump of ioquake3 with latest upstream fixes, aims for maximum FPS and mod compatibility

**Q3IDE uses Quake3e** as its engine base due to superior performance and modern rendering support.

## Quake3e Features

### Rendering

#### OpenGL Renderer
- OpenGL 1.1 compatible, uses features from newer versions when available
- **High-quality per-pixel dynamic lighting**
- **Merged lightmaps (atlases)** -- reduces draw calls by packing lightmaps into larger textures
- **Static world surfaces cached in VBO** (Vertex Buffer Objects)
- **Bloom post-processing** effect
- **Reflection post-processing** effect
- Optimized surface sorting and batching

#### Vulkan Renderer
- Complete Vulkan rendering backend (selectable via `cl_renderer` cvar)
- **30% to 200%+ FPS increase** depending on map, compared to OpenGL
- Multi-threaded command buffer generation (in Quake3e-HD variant)
- Optimized for current GPU architectures
- Can be switched at runtime: `cl_renderer opengl` or `cl_renderer vulkan`

### Input System
- **Raw mouse input** enabled automatically (instead of DirectInput on Windows)
- **Unlagged mouse event processing** (reversible via `\in_lagged 1`)
- Hotkey for minimize/restore main window (Windows)
- Improved input latency overall

### Virtual Machine (QVM)
- **Significantly reworked QVM** implementation
- **Very fast bytecode compilers** for 32/64-bit ARM processors
- Slight improvements for x86 bytecode compilation
- Better security and stability in VM execution
- Optimized system call dispatch

### Filesystem
- **Raised filesystem limits**: up to 20,000 maps can be handled in a single directory
- Improved file handling and search performance

### Memory
- **Reworked Zone memory allocator** to eliminate out-of-memory errors
- More efficient memory usage patterns
- Reduced server-side memory usage

### Networking
- **Improved server-side DoS protection**
- More robust network handling
- Better connection stability

### Platform Support
- **SDL2 backend** for video, audio, and input (enforced for macOS)
- **Raspberry Pi 4** support with Vulkan renderer
- **ARM64 / Apple Silicon** support via SDL2
- Windows, Linux, macOS, FreeBSD support
- Native Apple platform integration through SDL2

### Compatibility
- Aimed to be **compatible with all existing Q3A mods**
- Works with Team Arena out of the box (ioquake3 had issues)
- Compatible with CPMA, OSP, and other popular mods

## ioquake3 Features

### Platform Integration
- Can be installed **system-wide** with per-user config files in home directory
- More traditional Unix-like installation model
- Better integration with package managers

### Security
- Multiple security fixes over the years
- Sandboxed file access
- Protection against malicious QVMs

### Modern Features
- OpenAL sound backend
- Mumble VoIP integration
- IPv6 support
- SDL2 backend
- VoIP support (Speex codec)
- Ogg Vorbis audio support
- PNG texture support
- Anaglyph stereo 3D rendering
- GUID support

### Community
- Large community of mod developers
- Many commercial games built on ioquake3
- Well-documented for mod development

## Key Differences

| Feature | Quake3e | ioquake3 |
|---------|---------|----------|
| **Primary focus** | Performance | Compatibility/Features |
| **Vulkan support** | Yes (full renderer) | No |
| **FPS improvement** | 10-200%+ over original | Modest improvements |
| **Lightmap atlases** | Yes | No |
| **VBO world surfaces** | Yes | No |
| **ARM64 JIT** | Yes (fast) | Basic |
| **Install model** | Portable (game directory) | System-wide + per-user |
| **Team Arena** | Works out of the box | May need configuration |
| **Bloom/Reflection** | Yes | No (without mods) |
| **Raw mouse input** | Automatic | Manual configuration |
| **DoS protection** | Improved | Basic |
| **File system limits** | 20,000 maps | Lower limits |
| **Memory allocator** | Reworked (more stable) | Original with patches |
| **SDL backend** | SDL2 (enforced on macOS) | SDL2 |
| **VoIP** | No | Yes (Speex) |
| **OpenAL** | No | Yes |
| **IPv6** | Basic | Yes |

## Why Quake3e for Q3IDE

Quake3e was chosen as the Q3IDE engine base for several reasons:

1. **Performance**: The 10-200% FPS improvement leaves more headroom for the additional texture upload and rendering overhead of Q3IDE
2. **VBO caching**: Static world surfaces in VBOs mean the engine spends less time on normal rendering, leaving more budget for dynamic Q3IDE textures
3. **Vulkan renderer**: Provides a modern rendering path that could enable more efficient texture upload strategies
4. **ARM64 support**: Native Apple Silicon support is essential for macOS development
5. **SDL2 on macOS**: Provides proper macOS window management and event handling
6. **Mod compatibility**: Ensures Q3IDE works with existing maps and content
7. **Active development**: Quake3e receives regular updates and bug fixes
8. **Lightmap atlases**: Reduced draw calls means more rendering budget for Q3IDE overlays

## Quake3e-Specific Cvars (Relevant to Q3IDE)

| Cvar | Description |
|------|-------------|
| `cl_renderer` | Select renderer: `opengl` or `vulkan` |
| `r_bloom` | Enable bloom post-processing |
| `r_fbo` | Enable framebuffer objects (needed for some effects) |
| `r_hdr` | Enable HDR rendering |
| `r_ext_multisample` | MSAA anti-aliasing samples |
| `r_ext_supersample` | Super-sampling factor |
| `r_modeFullscreen` | Fullscreen resolution mode |
| `r_noborder` | Borderless window mode |
| `in_lagged` | Restore lagged mouse input (0 = unlagged, default) |

## Building Quake3e

### macOS (ARM64 / Apple Silicon)

```bash
cd quake3e
make ARCH=arm64
# Or for x86_64:
make ARCH=x86_64
```

### Platform-Specific Notes

- macOS uses SDL2 backend (enforced)
- Requires Xcode Command Line Tools
- Screen Recording permission needed for Q3IDE's ScreenCaptureKit integration
- Game data (`pak0.pk3`) must be in the `baseq3/` directory alongside the engine binary

## Source Code Structure (Quake3e)

The Quake3e codebase follows the same structure as the original id Tech 3:

```
code/
  client/        # Client-side engine code
  server/        # Server-side engine code
  qcommon/       # Shared common code (cmd, cvar, filesystem, VM)
  renderer/      # OpenGL renderer
  renderer2/     # Enhanced/alternate renderer
  renderercommon/ # Shared renderer code
  renderergl2/   # OpenGL 2.0+ renderer (some forks)
  renderervk/    # Vulkan renderer
  cgame/         # Client game module
  game/          # Server game module
  ui/            # UI module
  botlib/        # Bot AI library
  sys/           # Platform-specific code (SDL2, filesystem)
  sdl/           # SDL2 integration
```
