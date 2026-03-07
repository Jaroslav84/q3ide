# MoltenVK -- Vulkan on macOS Texture Upload Considerations

> **Sources:**
> - [MoltenVK - GitHub (KhronosGroup)](https://github.com/KhronosGroup/MoltenVK)
> - [MoltenVK Runtime User Guide](https://github.com/KhronosGroup/MoltenVK/blob/main/Docs/MoltenVK_Runtime_UserGuide.md)
> - [MoltenVK for Advanced Vulkan Renderers on macOS (Vulkanised 2024)](https://vulkan.org/user/pages/09.events/vulkanised-2024/vulkanised-2024-roman-kuznetsov-meta.pdf)
> - [3 Years of Metal - Arseny Kapoulkine](https://medium.com/@zeuxcg/3-years-of-metal-22d74969a21)
> - [MoltenVK Releases](https://github.com/KhronosGroup/MoltenVK/releases)
> - [Texture sharing between OpenGL and Vulkan on macOS - MoltenVK Issue #2288](https://github.com/KhronosGroup/MoltenVK/issues/2288)
> - [MoltenVK User Guide (moltengl.com)](https://www.moltengl.com/docs/readme/moltenvk-readme-user-guide.html)

## Overview

MoltenVK is a layered implementation of Vulkan 1.4 graphics and compute functionality built on
Apple's Metal graphics framework. It enables Vulkan applications to run on macOS, iOS, tvOS, and
visionOS. For Q3IDE, MoltenVK is the bridge that allows Quake3e's Vulkan renderer to run on
macOS while also enabling Metal/IOSurface interop for zero-copy screen capture.

## Texture Upload Considerations

### Memory Model on macOS

- **Apple Silicon (M1/M2/M3/M4):** Unified memory architecture. CPU and GPU share the same
  physical memory. Host-visible device-local memory is naturally available, making staging
  buffers less critical (though still recommended for API correctness).
- **Discrete GPUs (older Macs):** Separate CPU and GPU memory pools. Staging buffers are
  essential for optimal performance.

### Host-Coherent Memory Limitation

On macOS versions **prior to macOS 10.15.6**, native host-coherent image device memory is not
available. Applications using `vkMapMemory()` with VkImage `VK_MEMORY_PROPERTY_HOST_COHERENT_BIT`
device memory must call either:

- `vkUnmapMemory()`, or
- `vkFlushMappedMemoryRanges()` / `vkInvalidateMappedMemoryRanges()`

to ensure memory changes are coherent between the CPU and GPU. This limitation does **not** apply
on macOS 10.15.6 and later.

**For Q3IDE:** Target macOS 12.3+ (ScreenCaptureKit requirement), so this limitation does not
apply.

### PVRTC Compressed Formats

Image content in PVRTC compressed formats must be loaded directly into a VkImage using
host-visible memory mapping. Loading via a staging buffer will result in malformed image content.

**For Q3IDE:** Not applicable -- screen capture frames are uncompressed BGRA.

### Metal Shared Memory

Metal shared memory is treated as having the Vulkan `VK_MEMORY_PROPERTY_HOST_CACHED_BIT` flag.
This affects how MoltenVK handles shared memory resources.

## Metal Backend Texture Upload Methods

Under the hood, MoltenVK translates Vulkan calls to Metal. Metal provides two built-in ways to
upload texture data:

1. **`MTLTexture.replaceRegion`** -- Direct CPU-to-texture write. Can be used if the GPU is not
   currently reading the texture. Suitable for initialization or when synchronization is
   guaranteed.

2. **`MTLBlitCommandEncoder`** -- Asynchronous blit operation via command buffer. This is what
   MoltenVK uses internally for `vkCmdCopyBufferToImage`. Proper for per-frame updates with
   GPU synchronization.

## Descriptor Limits

`VK_EXT_descriptor_indexing` is initially limited to Metal Tier 1:

- 96/128 textures
- 16 samplers

Exceptions (higher limits available):

- macOS 11.0 (Big Sur) or later
- Older macOS with Intel GPU and Metal argument buffers enabled

**For Q3IDE:** With macOS 12.3+ as a minimum, higher descriptor limits are available.

## Performance Considerations

### Impedance Mismatch

Implementing one low-level API as a layer above another introduces potential impedance mismatch:

- Vulkan and Metal have different object models, synchronization primitives, and memory
  management approaches.
- MoltenVK does a good job translating, but certain patterns may be less optimal than native
  Metal.

### Optimization Tips

1. **Disable Metal Validation in Production:** When running from Xcode, the default Scheme
   settings enable Metal API Validation and GPU Frame Capture. Both reduce performance
   significantly and should be disabled for benchmarking and release builds.

2. **Pre-compiled Shaders:** Metal supports pre-compiled shaders via `MoltenShaderConverter`,
   which can improve shader loading and setup performance, reducing scene loading time.

3. **Memoryless Texture Storage:** When available, use memoryless texture storage for transient
   render targets (not applicable to screen capture textures which need persistent content).

4. **Asynchronous Texture Uploads:** Use asynchronous uploads (via transfer queue) to reduce
   stuttering during heavy texture streaming.

## VK_EXT_metal_objects Extension

This extension is critical for Q3IDE's zero-copy path. It enables:

- **Importing IOSurfaces** as VkImages via `VkImportMetalIOSurfaceInfoEXT`
- **Exporting IOSurfaces** from VkImages via `VkExportMetalIOSurfaceInfoEXT`
- **Importing MTLTextures** via `VkImportMetalTextureInfoEXT`
- **Shared synchronization** via MTLSharedEvent

See `vk_metal_interop.md` for detailed usage.

## Known Limitations Relevant to Q3IDE

1. **No geometry shaders.** Metal does not support geometry shaders. Quake3e does not use them,
   so this is not an issue.

2. **Texture format support.** Not all Vulkan texture formats map directly to Metal formats.
   `VK_FORMAT_B8G8R8A8_UNORM` (the format matching ScreenCaptureKit output) is well-supported.

3. **Queue family limitations.** Metal has different queue semantics than Vulkan. MoltenVK
   typically exposes a single queue family. Transfer operations share the graphics queue.

4. **Synchronization differences.** Metal fences and Vulkan fences have different semantics.
   MoltenVK translates between them, but complex multi-queue synchronization patterns may behave
   differently than on native Vulkan implementations.

## Configuration

MoltenVK behavior can be configured via:

- **Environment variables:** `MVK_CONFIG_*` variables
- **`vk_mvk_moltenvk.h` API:** Direct configuration of MoltenVK-specific settings
- **`MVKConfiguration` struct:** Passed via `vkSetMoltenVKConfigurationMVK()`

Key configuration options for texture streaming:

```c
MVKConfiguration config;
config.synchronousQueueSubmits = VK_FALSE;   // Allow async queue submission
config.prefillMetalCommandBuffers = VK_FALSE; // Don't prefill for dynamic workloads
```

## Relevance to Q3IDE

For Q3IDE running on macOS:

1. **Zero-copy path:** Use `VK_EXT_metal_objects` to import ScreenCaptureKit IOSurfaces directly
   as VkImages. This eliminates all data copies.
2. **Fallback path:** Standard Vulkan staging buffer upload. On Apple Silicon's unified memory,
   the staging buffer copy is fast because CPU and GPU share the same physical memory.
3. **Format:** Use `VK_FORMAT_B8G8R8A8_UNORM` to match ScreenCaptureKit's native BGRA output.
4. **Minimum macOS version:** 12.3+ means all host-coherent memory limitations are resolved.
5. **Quake3e integration:** Quake3e already has both OpenGL and Vulkan renderers. The Vulkan
   path through MoltenVK is preferred for modern macOS.
