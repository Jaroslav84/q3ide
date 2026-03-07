# Vulkan Dynamic Texture Update Per Frame

> **Sources:**
> - [How to Update Texture for Every Frame in Vulkan - Best Practices](https://copyprogramming.com/howto/how-to-update-texture-for-every-frame-in-vulkan)
> - [Writing an efficient Vulkan renderer - zeux.io](https://zeux.io/2020/02/27/writing-an-efficient-vulkan-renderer/)
> - [Resource Updates - Diligent Graphics](https://diligentgraphics.com/diligent-engine/using-the-api/resource-updates/)
> - [Vulkan SDK - Rotating Texture (ARM)](https://arm-software.github.io/vulkan-sdk/rotating_texture.html)
> - [Host image copy - Vulkan Documentation Project](https://docs.vulkan.org/samples/latest/samples/extensions/host_image_copy/README.html)
> - [VK_EXT_host_image_copy - Vulkan Docs](https://docs.vulkan.org/features/latest/features/proposals/VK_EXT_host_image_copy.html)
> - [Uploading Textures to GPU - The Good Way](https://erfan-ahmadi.github.io/blog/Nabla/imageupload)

## Overview

Updating textures every frame in Vulkan requires careful management of synchronization, memory,
and command buffer submission. Unlike OpenGL, Vulkan provides no implicit synchronization, so the
application must explicitly prevent the CPU from modifying data that the GPU is still reading.

## Strategy Comparison

| Strategy | Overhead | Compatibility | Memory Usage | Best For |
|----------|----------|---------------|--------------|----------|
| Staging buffer ring | ~10% | All GPUs | 2-3x staging | Universal fallback |
| Host Image Copy | Minimal | Vulkan 1.4 / ext | 1x (no staging) | Modern GPUs |
| Compute shader | GPU-only | All GPUs | Varies | Procedural textures |
| IOSurface import (macOS) | Zero-copy | MoltenVK | Shared | macOS capture |

## Ring Buffering with Staging Buffers

The most universal approach for per-frame texture updates.

### Concept

Maintain 2-3 staging buffers in a ring. Each frame uses a different staging buffer, and fences
ensure the CPU never modifies a buffer the GPU is still reading.

```
Frame N:   Write to staging[0], GPU reads staging[2]
Frame N+1: Write to staging[1], GPU reads staging[0]
Frame N+2: Write to staging[2], GPU reads staging[1]
```

### Implementation

```c
#define FRAMES_IN_FLIGHT 2

typedef struct {
    VkBuffer      stagingBuffer[FRAMES_IN_FLIGHT];
    VkDeviceMemory stagingMemory[FRAMES_IN_FLIGHT];
    void*         mappedPtr[FRAMES_IN_FLIGHT];  // persistently mapped
    VkFence       fence[FRAMES_IN_FLIGHT];
    uint32_t      currentFrame;
} DynamicTextureUploader;

void uploadFrame(DynamicTextureUploader* uploader, VkImage image,
                 const void* pixelData, VkDeviceSize dataSize) {
    uint32_t idx = uploader->currentFrame % FRAMES_IN_FLIGHT;

    // Wait for this staging buffer's previous use to complete
    vkWaitForFences(device, 1, &uploader->fence[idx], VK_TRUE, UINT64_MAX);
    vkResetFences(device, 1, &uploader->fence[idx]);

    // Copy pixel data into staging buffer (persistently mapped)
    memcpy(uploader->mappedPtr[idx], pixelData, dataSize);

    // Record command buffer: transition + copy + transition
    VkCommandBuffer cmd = beginSingleTimeCommands();

    // Transition: SHADER_READ_ONLY -> TRANSFER_DST
    transitionImageLayout(cmd, image,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    // Copy staging buffer to image
    VkBufferImageCopy region = { /* ... */ };
    vkCmdCopyBufferToImage(cmd, uploader->stagingBuffer[idx],
        image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    // Transition: TRANSFER_DST -> SHADER_READ_ONLY
    transitionImageLayout(cmd, image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    endSingleTimeCommands(cmd, uploader->fence[idx]);

    uploader->currentFrame++;
}
```

### Frame Count Guidelines

- **Double buffering (2 frames):** Sufficient for most cases. Lower latency.
- **Triple buffering (3 frames):** Better resilience to CPU-GPU load variations.
- **Beyond 3 frames:** Introduces input latency without significant performance gains.

## Image Layout Management

### Best Practices

1. **Transition once before the render loop if possible.** If the texture will always be used for
   sampling, keep it in `VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL` and only transition to
   `VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL` during transfer operations.

2. **Batch barriers together.** Multiple image barriers in one `vkCmdPipelineBarrier2()` call are
   more efficient than separate calls.

3. **Use `VK_IMAGE_LAYOUT_GENERAL` sparingly.** It works for everything but optimizes nothing.
   Use specific layouts for best performance.

## VK_EXT_host_image_copy (Vulkan 1.4)

A modern extension that eliminates staging buffers entirely for texture uploads.

### Key Benefits

- **No staging buffer needed:** Copy directly from host memory to device-optimal images.
- **Halved peak memory usage:** No temporary staging allocation required.
- **Reduced stuttering:** Eliminates the staging buffer allocation/deallocation overhead during
  level loads or heavy streaming.

### Usage

```c
// Image creation: use HOST_TRANSFER_BIT instead of TRANSFER_DST_BIT
VkImageCreateInfo imageInfo = {
    // ...
    .usage = VK_IMAGE_USAGE_HOST_TRANSFER_BIT_EXT | VK_IMAGE_USAGE_SAMPLED_BIT,
};

// Direct copy from host memory
VkMemoryToImageCopyEXT region = {
    .sType = VK_STRUCTURE_TYPE_MEMORY_TO_IMAGE_COPY_EXT,
    .pHostPointer = pixelData,
    .imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
    .imageOffset = { 0, 0, 0 },
    .imageExtent = { width, height, 1 }
};

VkCopyMemoryToImageInfoEXT copyInfo = {
    .sType = VK_STRUCTURE_TYPE_COPY_MEMORY_TO_IMAGE_INFO_EXT,
    .dstImage = textureImage,
    .dstImageLayout = VK_IMAGE_LAYOUT_GENERAL,
    .regionCount = 1,
    .pRegions = &region
};

vkCopyMemoryToImageEXT(device, &copyInfo);
```

### Availability

- Mandatory in Vulkan 1.4.
- Available as `VK_EXT_host_image_copy` extension on older implementations.
- Check support via `VkPhysicalDeviceHostImageCopyFeaturesEXT`.

## Synchronization Considerations

### Fences

Use fences to ensure the CPU does not modify a staging buffer that the GPU is still reading:

```c
// Submit with fence
vkQueueSubmit(queue, 1, &submitInfo, frameFence);

// Before reusing the staging buffer, wait for the fence
vkWaitForFences(device, 1, &frameFence, VK_TRUE, UINT64_MAX);
```

### Timeline Semaphores (Vulkan 1.2+)

For more flexible synchronization with multiple in-flight frames:

```c
VkSemaphoreTypeCreateInfo timelineInfo = {
    .sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
    .semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE,
    .initialValue = 0
};
```

Timeline semaphores allow waiting for specific frame values without managing multiple fence
objects.

## Memory Considerations

- **Host-visible memory on discrete GPUs is scarce and slow.** Always use device-local memory
  for textures and transfer data via staging buffers.
- **Integrated GPUs** (common on mobile, Apple Silicon) share memory between CPU and GPU. In this
  case, direct host-visible device-local memory may be available, potentially allowing direct
  texture writes without staging.
- On Apple Silicon via MoltenVK, Metal's shared memory model provides efficient host-visible
  device-local memory.

## Compute Shader Alternative

For procedurally generated textures or GPU-side image processing, compute shaders can update
textures without any CPU-GPU data transfer:

```glsl
layout(set = 0, binding = 0, rgba8) uniform writeonly image2D outputImage;

void main() {
    ivec2 coord = ivec2(gl_GlobalInvocationID.xy);
    vec4 color = computeColor(coord);
    imageStore(outputImage, coord, color);
}
```

This avoids staging buffers entirely and leverages the GPU's parallel processing power.

## Relevance to Q3IDE

For Q3IDE's per-frame window capture streaming:

1. **Primary path (macOS):** Import IOSurface via `VK_EXT_metal_objects` for zero-copy (see
   `vk_metal_interop.md`).
2. **Fallback path:** Use ring-buffered staging with 2 frames in flight. The Rust capture
   dylib's ring buffer maps naturally to a double-buffered staging approach.
3. **Future optimization:** If Vulkan 1.4 is available, `VK_EXT_host_image_copy` can eliminate
   staging buffers.
4. Use `VK_FORMAT_B8G8R8A8_UNORM` to match ScreenCaptureKit's BGRA output format.
