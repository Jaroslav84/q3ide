/*
 * q3ide_poll.c — Frame upload, PollFrames, PollChanges dispatch.
 * Window scene rendering: q3ide_scene.c.  Mirror rendering: q3ide_mirror.c.
 */

#include "q3ide_wm.h"
#include "q3ide_log.h"
#include "q3ide_wm_internal.h"
#include "../qcommon/qcommon.h"
#include "../client/client.h"
#include "../renderercommon/tr_public.h"
#include <string.h>

extern void Q3IDE_WM_PollChanges(void);

static void q3ide_upload_frame(q3ide_win_t *win, const Q3ideFrame *frame)
{
	int needed = (int) (frame->width * frame->height * 4);
	int row = (int) frame->width * 4;
	unsigned int r, c;

	if (!re.UploadCinematic)
		return;

	if (needed > q3ide_wm.fbuf_size) {
		if (q3ide_wm.fbuf)
			Z_Free(q3ide_wm.fbuf);
		q3ide_wm.fbuf = Z_Malloc(needed);
		q3ide_wm.fbuf_size = needed;
	}

	for (r = 0; r < frame->height; r++) {
		const unsigned char *s = frame->pixels + r * frame->stride;
		byte *d = q3ide_wm.fbuf + r * row;
		for (c = 0; c < frame->width; c++, s += 4, d += 4) {
			d[0] = s[2];
			d[1] = s[1];
			d[2] = s[0];
			d[3] = s[3]; /* BGRA→RGBA */
		}
	}

	re.UploadCinematic((int) frame->width, (int) frame->height, (int) frame->width, (int) frame->height, q3ide_wm.fbuf,
	                   win->scratch_slot, qtrue);
	win->tex_w = (int) frame->width;
	win->tex_h = (int) frame->height;

	if (win->frames == 0) {
		char name[32];
		/* Try the explicit shader first (defined in q3ide.shader) */
		Com_sprintf(name, sizeof(name), "q3ide/win%d", win->scratch_slot);
		win->shader = re.RegisterShader(name);

		/* Fallback: register the scratch image as an implicit shader.
		 * The scratch image was pre-created in Q3IDE_OnVidRestart, so
		 * R_FindImageFile("*scratchN") succeeds and Q3 builds a simple
		 * opaque shader for it. */
		if (win->shader == 0) {
			Com_sprintf(name, sizeof(name), "*scratch%d", win->scratch_slot);
			win->shader = re.RegisterShader(name);
		}

		Q3IDE_LOGI("win[%d] id=%u slot=%d shader=%d %dx%d", (int) (win - q3ide_wm.wins), win->capture_id,
		           win->scratch_slot, win->shader, frame->width, frame->height);
	}
}

void Q3IDE_WM_PollFrames(void)
{
	int i;
	unsigned long long now_ms;
	if (!q3ide_wm.cap || !q3ide_wm.cap_get_frame)
		return;
	now_ms = Sys_Milliseconds();

	/* Only poll window changes when auto_attach is active — enumerating all
	 * windows via SCShareableContent blocks the main thread briefly each call. */
	if (q3ide_wm.auto_attach && now_ms - q3ide_wm.last_scan_ms >= 5000) {
		Q3IDE_WM_PollChanges();
		q3ide_wm.last_scan_ms = now_ms;
	}

	for (i = 0; i < Q3IDE_MAX_WIN; i++) {
		q3ide_win_t *win = &q3ide_wm.wins[i];
		Q3ideFrame frame;
		if (!win->active)
			continue;
		frame = q3ide_wm.cap_get_frame(q3ide_wm.cap, win->capture_id);
		if (!frame.pixels)
			continue;
		if (frame.source_wid != win->capture_id) {
			if (win->frames < 5)
				Com_Printf("q3ide: MISMATCH [%d] want=%u got=%u\n", i, win->capture_id, frame.source_wid);
			continue;
		}
		win->last_frame_ms = now_ms;
		if (win->first_frame_ms == 0)
			win->first_frame_ms = now_ms;
		win->status = Q3IDE_WIN_STATUS_ACTIVE;

		if (win->fps_target > 0 && win->fps_target < Q3IDE_FPS_FULL) {
			int skip_ratio = (Q3IDE_FPS_FULL / win->fps_target) - 1;
			if (win->skip_counter++ < skip_ratio)
				continue;
			win->skip_counter = 0;
		}

		if (win->tex_w > 0 && ((int) frame.width != win->tex_w || (int) frame.height != win->tex_h)) {
			float frame_asp = (frame.height > 0) ? (float) frame.width / (float) frame.height : 1.0f;
			win->world_w = win->world_h * frame_asp;
			Com_Printf("q3ide: win %u resized %dx%d -> %dx%d (world %.0fx%.0f)\n", win->capture_id, win->tex_w,
			           win->tex_h, (int) frame.width, (int) frame.height, win->world_w, win->world_h);
		}
		/* First frame: correct world_w to match actual capture aspect ratio. */
		if (win->frames == 0 && frame.height > 0) {
			float frame_asp = (float) frame.width / (float) frame.height;
			win->world_w = win->world_h * frame_asp;
		}
		q3ide_upload_frame(win, &frame);
		win->frames++;
	}
}
