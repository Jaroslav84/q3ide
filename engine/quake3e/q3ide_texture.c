/*
 * q3ide_texture.c — Quake3e texture management for q3ide.
 *
 * Implements dynamic texture creation and per-frame upload for both
 * OpenGL (glTexSubImage2D) and Vulkan (staging buffer) renderers.
 *
 * The renderer path is selected at runtime via Quake3e's \cl_renderer cvar.
 *
 * Build: compiled as part of the Quake3e client binary.
 */

#include "q3ide_texture.h"
#include <string.h>

/* ─── Constants ───────────────────────────────────────────────────────── */

#define Q3IDE_MAX_TEXTURES  16  /* MVP: only need 1, but allow a few */

/* ─── Texture slot ────────────────────────────────────────────────────── */

typedef struct {
	int               active;
	int               width;
	int               height;
	q3ide_pixel_format_t format;

	/* OpenGL */
	unsigned int      gl_tex_id;

	/* Vulkan */
	/* TODO: VkImage, VkDeviceMemory, VkImageView, staging buffer */
} q3ide_texture_slot_t;

static q3ide_texture_slot_t q3ide_textures[Q3IDE_MAX_TEXTURES];
static int q3ide_tex_initialized = 0;

/* ─── Init / Shutdown ─────────────────────────────────────────────────── */

void q3ide_tex_init(void)
{
	memset(q3ide_textures, 0, sizeof(q3ide_textures));
	q3ide_tex_initialized = 1;
}

void q3ide_tex_shutdown(void)
{
	int i;
	for (i = 0; i < Q3IDE_MAX_TEXTURES; i++) {
		if (q3ide_textures[i].active) {
			q3ide_tex_destroy(i + 1);
		}
	}
	q3ide_tex_initialized = 0;
}

/* ─── Create ──────────────────────────────────────────────────────────── */

int q3ide_tex_create(int width, int height, q3ide_pixel_format_t format)
{
	int i;
	q3ide_texture_slot_t *slot;

	if (!q3ide_tex_initialized) {
		return 0;
	}

	/* Find free slot */
	for (i = 0; i < Q3IDE_MAX_TEXTURES; i++) {
		if (!q3ide_textures[i].active) {
			break;
		}
	}
	if (i >= Q3IDE_MAX_TEXTURES) {
		return 0;  /* No free slots */
	}

	slot = &q3ide_textures[i];
	slot->active = 1;
	slot->width = width;
	slot->height = height;
	slot->format = format;

	/* ── OpenGL path ──────────────────────────────────────────────── */
	/* TODO: When integrated into Quake3e renderer build:
	 *
	 * qglGenTextures(1, &slot->gl_tex_id);
	 * qglBindTexture(GL_TEXTURE_2D, slot->gl_tex_id);
	 * qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	 * qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	 * qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	 * qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	 *
	 * GLenum gl_format = (format == Q3IDE_FORMAT_BGRA8)
	 *     ? GL_BGRA : GL_RGBA;
	 *
	 * // Allocate texture memory (NULL data = no initial upload)
	 * qglTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0,
	 *               gl_format, GL_UNSIGNED_BYTE, NULL);
	 */

	/* ── Vulkan path ──────────────────────────────────────────────── */
	/* TODO: When integrated into Quake3e Vulkan renderer:
	 *
	 * 1. Create VkImage (VK_FORMAT_B8G8R8A8_UNORM for BGRA)
	 * 2. Allocate VkDeviceMemory (device-local)
	 * 3. Create VkImageView
	 * 4. Create staging buffer (host-visible, host-coherent)
	 * 5. Transition image layout to VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
	 */

	return i + 1;  /* Texture IDs are 1-based */
}

/* ─── Upload ──────────────────────────────────────────────────────────── */

void q3ide_tex_upload(int tex_id, const unsigned char *pixels,
                      int width, int height, int stride)
{
	q3ide_texture_slot_t *slot;

	if (tex_id < 1 || tex_id > Q3IDE_MAX_TEXTURES) {
		return;
	}

	slot = &q3ide_textures[tex_id - 1];
	if (!slot->active || !pixels) {
		return;
	}

	/* ── OpenGL path ──────────────────────────────────────────────── */
	/* TODO: When integrated into Quake3e renderer:
	 *
	 * qglBindTexture(GL_TEXTURE_2D, slot->gl_tex_id);
	 *
	 * GLenum gl_format = (slot->format == Q3IDE_FORMAT_BGRA8)
	 *     ? GL_BGRA : GL_RGBA;
	 *
	 * // Handle stride != width * 4
	 * if (stride != width * 4) {
	 *     qglPixelStorei(GL_UNPACK_ROW_LENGTH, stride / 4);
	 * }
	 *
	 * // Fast path: glTexSubImage2D (no reallocation)
	 * qglTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height,
	 *                  gl_format, GL_UNSIGNED_BYTE, pixels);
	 *
	 * if (stride != width * 4) {
	 *     qglPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
	 * }
	 *
	 * // For PBO-based async upload (Phase 2 optimization):
	 * // 1. Bind PBO
	 * // 2. glBufferData with NULL (orphan)
	 * // 3. glMapBuffer + memcpy
	 * // 4. glUnmapBuffer
	 * // 5. glTexSubImage2D with offset 0 (async DMA)
	 */

	/* ── Vulkan path ──────────────────────────────────────────────── */
	/* TODO: When integrated into Quake3e Vulkan renderer:
	 *
	 * 1. Map staging buffer
	 * 2. memcpy pixels into staging buffer (handle stride)
	 * 3. Unmap staging buffer
	 * 4. Record command buffer:
	 *    a. Transition image: SHADER_READ_ONLY -> TRANSFER_DST
	 *    b. vkCmdCopyBufferToImage
	 *    c. Transition image: TRANSFER_DST -> SHADER_READ_ONLY
	 * 5. Submit to graphics queue
	 *
	 * Note: For MoltenVK on macOS, this works via Metal under the hood.
	 * Future zero-copy path: VK_EXT_metal_objects to import IOSurface directly.
	 */

	(void)width;
	(void)height;
	(void)stride;
}

/* ─── Destroy ─────────────────────────────────────────────────────────── */

void q3ide_tex_destroy(int tex_id)
{
	q3ide_texture_slot_t *slot;

	if (tex_id < 1 || tex_id > Q3IDE_MAX_TEXTURES) {
		return;
	}

	slot = &q3ide_textures[tex_id - 1];
	if (!slot->active) {
		return;
	}

	/* ── OpenGL path ──────────────────────────────────────────────── */
	/* TODO: qglDeleteTextures(1, &slot->gl_tex_id); */

	/* ── Vulkan path ──────────────────────────────────────────────── */
	/* TODO: vkDestroyImageView, vkDestroyImage, vkFreeMemory,
	 *       destroy staging buffer */

	memset(slot, 0, sizeof(*slot));
}
