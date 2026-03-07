/*
 * q3ide_adapter.c — Quake3e implementation of the engine adapter.
 *
 * This file bridges q3ide to the Quake3e engine. It implements
 * q3ide_engine_adapter_t using Quake3e's internal APIs.
 *
 * Build: compiled as part of the Quake3e client binary.
 * This is the ONLY file that may call Quake3e internals (qgl*, trap_*, RE_*).
 */

#include "../adapter.h"
#include "q3ide_texture.h"
#include "q3ide_placement.h"

/* ─── Forward declarations for Quake3e internals ──────────────────────── */
/* These will be resolved from Quake3e headers when integrated into
 * the engine build. For now, we declare the interface we need. */

/* From renderer: */
/* extern int RE_RegisterShaderFromImage(const char *name, ...); */
/* From client: */
/* extern void Com_Printf(const char *fmt, ...); */
/* extern void Cmd_AddCommand(const char *cmd_name, void (*func)(void)); */

/* ─── Adapter function implementations ────────────────────────────────── */

static int quake3e_texture_create(int width, int height,
                                  q3ide_pixel_format_t format)
{
	return q3ide_tex_create(width, height, format);
}

static void quake3e_texture_upload(int tex_id, const unsigned char *pixels,
                                   int width, int height, int stride)
{
	q3ide_tex_upload(tex_id, pixels, width, height, stride);
}

static void quake3e_texture_destroy(int tex_id)
{
	q3ide_tex_destroy(tex_id);
}

static int quake3e_surface_create(float x, float y, float z,
                                  float width, float height,
                                  const float *normal)
{
	return q3ide_place_surface_create(x, y, z, width, height, normal);
}

static void quake3e_surface_set_texture(int surface_id, int tex_id)
{
	q3ide_place_surface_set_texture(surface_id, tex_id);
}

static void quake3e_surface_set_transform(int surface_id,
                                          const float *matrix4x4)
{
	q3ide_place_surface_set_transform(surface_id, matrix4x4);
}

static void quake3e_surface_destroy(int surface_id)
{
	q3ide_place_surface_destroy(surface_id);
}

static int quake3e_trace_nearest_wall(const float *origin,
                                      const float *direction,
                                      float *hit_pos, float *hit_normal)
{
	return q3ide_place_trace_wall(origin, direction, hit_pos, hit_normal);
}

static void quake3e_get_spawn_origin(float *origin)
{
	/* TODO: Read from cgame spawn data (cg.predictedPlayerState.origin)
	 * Stub: return map origin */
	origin[0] = 0.0f;
	origin[1] = 0.0f;
	origin[2] = 0.0f;
}

static void quake3e_get_view_angles(float *angles)
{
	/* TODO: Read from cg.refdefViewAngles */
	angles[0] = 0.0f;
	angles[1] = 0.0f;
	angles[2] = 0.0f;
}

static void quake3e_get_view_origin(float *origin)
{
	/* TODO: Read from cg.refdef.vieworg */
	origin[0] = 0.0f;
	origin[1] = 0.0f;
	origin[2] = 0.0f;
}

static void quake3e_on_frame(float delta_time)
{
	/* Called from the render loop each frame.
	 * This is where q3ide_main.c hooks in for per-frame updates. */
	(void)delta_time;
}

static void quake3e_console_print(const char *msg)
{
	/* TODO: Call Com_Printf when integrated into engine build */
	/* Com_Printf("%s", msg); */
	(void)msg;
}

static void quake3e_cmd_register(const char *cmd_name,
                                 void (*callback)(void))
{
	/* TODO: Call Cmd_AddCommand when integrated into engine build */
	/* Cmd_AddCommand(cmd_name, callback); */
	(void)cmd_name;
	(void)callback;
}

/* ─── Adapter instance ────────────────────────────────────────────────── */

static q3ide_engine_adapter_t quake3e_adapter = {
	/* Texture management */
	.texture_create      = quake3e_texture_create,
	.texture_upload      = quake3e_texture_upload,
	.texture_destroy     = quake3e_texture_destroy,

	/* Surface placement */
	.surface_create        = quake3e_surface_create,
	.surface_set_texture   = quake3e_surface_set_texture,
	.surface_set_transform = quake3e_surface_set_transform,
	.surface_destroy       = quake3e_surface_destroy,

	/* World queries */
	.trace_nearest_wall  = quake3e_trace_nearest_wall,
	.get_spawn_origin    = quake3e_get_spawn_origin,
	.get_view_angles     = quake3e_get_view_angles,
	.get_view_origin     = quake3e_get_view_origin,

	/* Render hooks */
	.on_frame            = quake3e_on_frame,

	/* Console */
	.console_print       = quake3e_console_print,
	.cmd_register        = quake3e_cmd_register,
};

/* Global adapter pointer — set during init */
q3ide_engine_adapter_t *q3ide_adapter = NULL;

/* ─── Init / Shutdown ─────────────────────────────────────────────────── */

void q3ide_adapter_init(void)
{
	q3ide_adapter = &quake3e_adapter;
	q3ide_tex_init();
	q3ide_place_init();
}

void q3ide_adapter_shutdown(void)
{
	q3ide_place_shutdown();
	q3ide_tex_shutdown();
	q3ide_adapter = NULL;
}
