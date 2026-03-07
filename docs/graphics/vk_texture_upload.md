# Vulkan Staging Buffer Texture Upload -- vkCmdCopyBufferToImage

> **Sources:**
> - [Images - Vulkan Tutorial](https://vulkan-tutorial.com/Texture_mapping/Images)
> - [Loading Images - Vulkan Guide](https://vkguide.dev/docs/chapter-5/loading_images/)
> - [Textures - Vulkan Guide (new chapter)](https://vkguide.dev/docs/new_chapter_4/textures/)
> - [Images - Vulkan Documentation Project](https://docs.vulkan.org/tutorial/latest/06_Texture_mapping/00_Images.html)
> - [Staging buffer - Vulkan Tutorial](https://vulkan-tutorial.com/Vertex_buffers/Staging_buffer)
> - [Memory transfers - Vulkan Guide](https://vkguide.dev/docs/chapter-5/memory_transfers/)
> - [Vulkan Memory Allocator - Recommended usage patterns](https://gpuopen-librariesandsdks.github.io/VulkanMemoryAllocator/html/usage_patterns.html)

## Overview

In Vulkan, uploading texture data to the GPU is an explicit, multi-step process involving staging
buffers, image layout transitions, and copy commands. Unlike OpenGL's `glTexSubImage2D` which
handles everything internally, Vulkan requires the application to manage each step.

## The Staging Buffer Pattern

The standard Vulkan texture upload pipeline:

```
CPU Memory --> Staging Buffer (HOST_VISIBLE) --> vkCmdCopyBufferToImage --> VkImage (DEVICE_LOCAL)
```

### Why Staging Buffers?

- GPU-local memory (`VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT`) provides the fastest rendering
  performance but is not directly accessible from the CPU.
- Host-visible memory (`VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT`) can be written by the CPU but is
  slower for GPU access (especially on discrete GPUs).
- The staging buffer pattern uses host-visible memory as a temporary holding area, then performs
  a GPU-side copy to device-local memory.

## Step-by-Step Texture Upload

### Step 1: Create the Staging Buffer

```c
VkBufferCreateInfo bufferInfo = {
    .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
    .size = imageSize,  // width * height * 4 for RGBA8
    .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
    .sharingMode = VK_SHARING_MODE_EXCLUSIVE
};

// Allocate with HOST_VISIBLE | HOST_COHERENT
VkBuffer stagingBuffer;
vkCreateBuffer(device, &bufferInfo, NULL, &stagingBuffer);

// Map and copy pixel data
void* data;
vkMapMemory(device, stagingBufferMemory, 0, imageSize, 0, &data);
memcpy(data, pixels, imageSize);
vkUnmapMemory(device, stagingBufferMemory);
```

### Step 2: Create the VkImage

```c
VkImageCreateInfo imageInfo = {
    .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
    .imageType = VK_IMAGE_TYPE_2D,
    .extent = { width, height, 1 },
    .mipLevels = 1,
    .arrayLayers = 1,
    .format = VK_FORMAT_R8G8B8A8_SRGB,  // or VK_FORMAT_B8G8R8A8_SRGB
    .tiling = VK_IMAGE_TILING_OPTIMAL,
    .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    .usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
    .samples = VK_SAMPLE_COUNT_1_BIT,
    .sharingMode = VK_SHARING_MODE_EXCLUSIVE
};

VkImage textureImage;
vkCreateImage(device, &imageInfo, NULL, &textureImage);
// Allocate with DEVICE_LOCAL memory
```

### Step 3: Transition Image Layout (UNDEFINED -> TRANSFER_DST_OPTIMAL)

```c
VkImageMemoryBarrier barrier = {
    .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
    .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
    .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
    .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
    .image = textureImage,
    .subresourceRange = {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .baseMipLevel = 0,
        .levelCount = 1,
        .baseArrayLayer = 0,
        .layerCount = 1
    },
    .srcAccessMask = 0,
    .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT
};

vkCmdPipelineBarrier(commandBuffer,
    VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,    // src stage
    VK_PIPELINE_STAGE_TRANSFER_BIT,        // dst stage
    0, 0, NULL, 0, NULL, 1, &barrier);
```

**Note:** Since the image starts with `VK_IMAGE_LAYOUT_UNDEFINED`, the source access mask is 0
and the source stage is `TOP_OF_PIPE` (writes don't need to wait on anything).

### Step 4: Copy Buffer to Image

```c
VkBufferImageCopy region = {
    .bufferOffset = 0,
    .bufferRowLength = 0,    // 0 = tightly packed
    .bufferImageHeight = 0,  // 0 = tightly packed
    .imageSubresource = {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .mipLevel = 0,
        .baseArrayLayer = 0,
        .layerCount = 1
    },
    .imageOffset = { 0, 0, 0 },
    .imageExtent = { width, height, 1 }
};

vkCmdCopyBufferToImage(commandBuffer,
    stagingBuffer,
    textureImage,
    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
    1, &region);
```

### Step 5: Transition Image Layout (TRANSFER_DST -> SHADER_READ_ONLY)

```c
VkImageMemoryBarrier barrier = {
    .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
    .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
    .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    .image = textureImage,
    .subresourceRange = { /* same as above */ },
    .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
    .dstAccessMask = VK_ACCESS_SHADER_READ_BIT
};

vkCmdPipelineBarrier(commandBuffer,
    VK_PIPELINE_STAGE_TRANSFER_BIT,
    VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
    0, 0, NULL, 0, NULL, 1, &barrier);
```

## VkBufferImageCopy Structure Reference

```c
typedef struct VkBufferImageCopy {
    VkDeviceSize         bufferOffset;       // Byte offset into the buffer
    uint32_t             bufferRowLength;    // Row length in texels (0 = tightly packed)
    uint32_t             bufferImageHeight;  // Image height in texels (0 = tightly packed)
    VkImageSubresourceLayers imageSubresource;
    VkOffset3D           imageOffset;        // Offset within the image
    VkExtent3D           imageExtent;        // Size of the region to copy
} VkBufferImageCopy;
```

- `bufferRowLength` and `bufferImageHeight` allow uploading from buffers with padding/stride.
  Setting both to 0 means the data is tightly packed with no extra padding.

## Image Layout Transition Summary

| Transition | Src Stage | Dst Stage | Src Access | Dst Access |
|-----------|-----------|-----------|-----------|-----------|
| UNDEFINED -> TRANSFER_DST | TOP_OF_PIPE | TRANSFER | 0 | TRANSFER_WRITE |
| TRANSFER_DST -> SHADER_READ_ONLY | TRANSFER | FRAGMENT_SHADER | TRANSFER_WRITE | SHADER_READ |
| UNDEFINED -> GENERAL | TOP_OF_PIPE | HOST | 0 | HOST_WRITE |

## Memory Allocation Best Practices (VMA)

When using Vulkan Memory Allocator (VMA):

- **Staging buffers:** Use `VMA_MEMORY_USAGE_CPU_ONLY` or `VMA_MEMORY_USAGE_AUTO` with
  `VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT`.
- **GPU textures:** Use `VMA_MEMORY_USAGE_GPU_ONLY` or `VMA_MEMORY_USAGE_AUTO` (defaults to
  device-local).
- For per-frame staging buffers, consider a dedicated staging buffer pool to avoid
  allocation/deallocation overhead.

## Relevance to Q3IDE

For Q3IDE's Vulkan renderer path (Quake3e supports both OpenGL and Vulkan):

1. Each captured window frame must be uploaded via the staging buffer pattern.
2. For per-frame updates, maintain a ring of staging buffers (2-3) synchronized with fences
   to prevent CPU-GPU conflicts.
3. On macOS via MoltenVK, consider using `VK_EXT_metal_objects` to import IOSurfaces directly,
   bypassing the staging buffer entirely (see `vk_metal_interop.md`).
4. The image format should match ScreenCaptureKit output: `VK_FORMAT_B8G8R8A8_UNORM` or
   `VK_FORMAT_B8G8R8A8_SRGB`.
