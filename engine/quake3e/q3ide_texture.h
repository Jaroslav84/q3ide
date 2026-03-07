/*
 * q3ide_texture.h — Quake3e texture management for q3ide.
 *
 * Handles creating, uploading, and destroying dynamic textures
 * for both OpenGL and Vulkan renderers.
 */

#ifndef Q3IDE_TEXTURE_H
#define Q3IDE_TEXTURE_H

#include "../adapter.h"

/* Initialize texture subsystem. */
void q3ide_tex_init(void);

/* Shut down texture subsystem, freeing all textures. */
void q3ide_tex_shutdown(void);

/* Create a texture slot. Returns texture ID > 0, or 0 on failure. */
int q3ide_tex_create(int width, int height, q3ide_pixel_format_t format);

/* Upload pixel data to a texture. */
void q3ide_tex_upload(int tex_id, const unsigned char *pixels,
                      int width, int height, int stride);

/* Destroy a texture. */
void q3ide_tex_destroy(int tex_id);

#endif /* Q3IDE_TEXTURE_H */
