/*
 * engine/adapter.h — Abstract engine adapter interface for q3ide.
 *
 * The engine is swappable. Quake3e implements this today.
 * A VR engine fork may implement it tomorrow.
 *
 * ALL engine interaction from capture and spatial logic goes through
 * this interface. NEVER call qgl*, trap_*, cg_* directly outside
 * the adapter implementation.
 *
 * Select implementation at compile time:
 *   #define Q3IDE_ENGINE_QUAKE3E   (default)
 *   #define Q3IDE_ENGINE_VR
 */

#ifndef Q3IDE_ENGINE_ADAPTER_H
#define Q3IDE_ENGINE_ADAPTER_H

#ifdef __cplusplus
extern "C" {
#endif

/* ─── Texture format enum ─────────────────────────────────────────────── */

typedef enum {
	Q3IDE_FORMAT_BGRA8 = 0,   /* ScreenCaptureKit native format */
	Q3IDE_FORMAT_RGBA8 = 1,
} q3ide_pixel_format_t;

/* ─── Engine adapter interface ────────────────────────────────────────── */

typedef struct {
	/* ── Texture management ────────────────────────────────────────── */

	/* Create a texture slot. Returns a texture ID (>0) or 0 on failure. */
	int   (*texture_create)(int width, int height, q3ide_pixel_format_t format);

	/* Upload pixel data to an existing texture. Assumes same dimensions. */
	void  (*texture_upload)(int tex_id, const unsigned char *pixels,
	                        int width, int height, int stride);

	/* Destroy a texture and free GPU resources. */
	void  (*texture_destroy)(int tex_id);

	/* ── Surface placement ─────────────────────────────────────────── */

	/* Create a textured quad surface in the game world. */
	int   (*surface_create)(float x, float y, float z,
	                        float width, float height,
	                        const float *normal);

	/* Bind a texture to a surface. */
	void  (*surface_set_texture)(int surface_id, int tex_id);

	/* Update surface position and orientation. matrix is 4x4 column-major. */
	void  (*surface_set_transform)(int surface_id, const float *matrix4x4);

	/* Destroy a surface. */
	void  (*surface_destroy)(int surface_id);

	/* ── World queries ─────────────────────────────────────────────── */

	/* Trace a ray from origin in direction. Returns 1 on hit, 0 on miss.
	 * hit_pos and hit_normal are filled on hit. */
	int   (*trace_nearest_wall)(const float *origin, const float *direction,
	                            float *hit_pos, float *hit_normal);

	/* Get the current player spawn origin. */
	void  (*get_spawn_origin)(float *origin);

	/* Get the current player view angles (pitch, yaw, roll). */
	void  (*get_view_angles)(float *angles);

	/* Get the current player view origin (eye position). */
	void  (*get_view_origin)(float *origin);

	/* ── Render hooks ──────────────────────────────────────────────── */

	/* Called once per render frame. delta_time in seconds. */
	void  (*on_frame)(float delta_time);

	/* ── Console ───────────────────────────────────────────────────── */

	/* Print a message to the game console. */
	void  (*console_print)(const char *msg);

	/* Register a console command. callback receives argc/argv style args. */
	void  (*cmd_register)(const char *cmd_name,
	                      void (*callback)(void));

} q3ide_engine_adapter_t;

/* ─── Global adapter instance ─────────────────────────────────────────── */

/* Set by the engine-specific init code at startup. */
extern q3ide_engine_adapter_t *q3ide_adapter;

/* Convenience macros for calling through the adapter */
#define Q3IDE_TEXTURE_CREATE(w, h, fmt) \
	q3ide_adapter->texture_create((w), (h), (fmt))

#define Q3IDE_TEXTURE_UPLOAD(id, px, w, h, s) \
	q3ide_adapter->texture_upload((id), (px), (w), (h), (s))

#define Q3IDE_TEXTURE_DESTROY(id) \
	q3ide_adapter->texture_destroy((id))

#define Q3IDE_TRACE_WALL(o, d, hp, hn) \
	q3ide_adapter->trace_nearest_wall((o), (d), (hp), (hn))

#define Q3IDE_CONSOLE_PRINT(msg) \
	q3ide_adapter->console_print((msg))

#ifdef __cplusplus
}
#endif

#endif /* Q3IDE_ENGINE_ADAPTER_H */
