/*
 * q3ide_poll.c — Frame upload, PollFrames, PollChanges dispatch.
 * Window scene rendering: q3ide_scene.c.  Mirror rendering: q3ide_mirror.c.
 */

#include "q3ide_win_mngr.h"
#include "q3ide_log.h"
#include "q3ide_win_mngr_internal.h"
#include "../qcommon/qcommon.h"
#include "../client/client.h"
#include "../renderercommon/tr_public.h"
#include <string.h>

extern void Q3IDE_WM_DrainPendingChanges(void);
extern void Q3IDE_WM_RestartEvictedStreams(void);

static void q3ide_upload_frame(q3ide_win_t *win, const Q3ideFrame *frame)
{
	if (!re.UploadCinematic)
		return;

	/* Pass BGRA pixels directly to GL — no CPU swizzle. GL_BGRA = 0x80E1.
	 * If stride != width*4 the rows aren't contiguous; pack into fbuf first. */
	if (frame->stride != frame->width * 4) {
		int needed = (int) (frame->width * frame->height * 4);
		unsigned int r;
		if (needed > q3ide_wm.fbuf_size) {
			if (q3ide_wm.fbuf)
				Z_Free(q3ide_wm.fbuf);
			q3ide_wm.fbuf = Z_Malloc(needed);
			q3ide_wm.fbuf_size = needed;
		}
		for (r = 0; r < frame->height; r++)
			memcpy(q3ide_wm.fbuf + r * frame->width * 4, frame->pixels + r * frame->stride, frame->width * 4);
		re.UploadCinematic((int) frame->width, (int) frame->height, (int) frame->width, (int) frame->height,
		                   q3ide_wm.fbuf, win->scratch_slot, qtrue, 0x80E1);
	} else {
		re.UploadCinematic((int) frame->width, (int) frame->height, (int) frame->width, (int) frame->height,
		                   (byte *) frame->pixels, win->scratch_slot, qtrue, 0x80E1);
	}
	win->tex_w = (int) frame->width;
	win->tex_h = (int) frame->height;

	if (win->frames == 0) {
		char name[32];
		/* Try the explicit shader first (defined in q3ide.shader) */
		Com_sprintf(name, sizeof(name), "q3ide/win%d", win->scratch_slot);
		win->shader = re.RegisterShader(name);

		if (win->shader == 0)
			Q3IDE_LOGE("win[%d] id=%u slot=%d shader not found — add q3ide/win%d to q3ide.shader",
			           (int) (win - q3ide_wm.wins), win->capture_id, win->scratch_slot, win->scratch_slot);

		Q3IDE_LOGI("win[%d] id=%u slot=%d shader=%d %dx%d", (int) (win - q3ide_wm.wins), win->capture_id,
		           win->scratch_slot, win->shader, frame->width, frame->height);
	}
}

void Q3IDE_WM_PollFrames(void)
{
	int                i;
	unsigned long long now_ms;
	static unsigned long long last_restart_ms = 0;
	if (!q3ide_win_mngr.cap || !q3ide_win_mngr.cap_get_frame)
		return;
	now_ms = Sys_Milliseconds();

	/* Drain changes fetched by background poll thread (1s interval). */
	Q3IDE_WM_DrainPendingChanges();

	/* Restore evicted streams every 5s when pool has free slots. */
	if (now_ms - last_restart_ms > 5000) {
		Q3IDE_WM_RestartEvictedStreams();
		last_restart_ms = now_ms;
	}

	for (i = 0; i < Q3IDE_MAX_WIN; i++) {
		q3ide_win_t *win = &q3ide_wm.wins[i];
		Q3ideFrame frame;
		if (!win->active)
			continue;

		frame = q3ide_win_mngr.cap_get_frame(q3ide_win_mngr.cap, win->capture_id);
		if (!frame.pixels)
			continue;
		if (frame.source_wid != win->capture_id) {
			if (win->frames < 5)
				Q3IDE_LOGI("MISMATCH win[%d] id=%u got wid=%u", i, win->capture_id, frame.source_wid);
			continue;
		}
		win->last_frame_ms = now_ms;
		if (win->first_frame_ms == 0)
			win->first_frame_ms = now_ms;
		win->status = Q3IDE_WIN_STATUS_ACTIVE;

		/* Timestamp-based throttle: works for any fps_target, adapts to game framerate. */
		if (win->fps_target > 0 && win->fps_target < Q3IDE_FPS_FULL) {
			unsigned long long interval_ms = 1000ULL / (unsigned long long) win->fps_target;
			if (now_ms - win->last_upload_ms < interval_ms)
				continue;
		}
		win->last_upload_ms = now_ms;

		/* Use UV slice width for aspect correction (slice panels show 1/N of display). */
		{
			float uv_span = (win->uv_x1 > win->uv_x0) ? (win->uv_x1 - win->uv_x0) : 1.0f;
			if (win->tex_w > 0 && ((int) frame.width != win->tex_w || (int) frame.height != win->tex_h)) {
				float slice_w = (float) frame.width * uv_span;
				win->world_w = (frame.height > 0) ? win->world_h * (slice_w / (float) frame.height) : win->world_w;
				Q3IDE_LOGI("win %u resized %dx%d -> %dx%d (world %.0fx%.0f)", win->capture_id, win->tex_w, win->tex_h,
				           (int) frame.width, (int) frame.height, win->world_w, win->world_h);
			}
			if (win->frames == 0 && frame.height > 0) {
				float slice_w = (float) frame.width * uv_span;
				win->world_w = win->world_h * (slice_w / (float) frame.height);
			}
		}
		q3ide_upload_frame(win, &frame);
		q3ide_wm.frame_uploads++;
		win->frames++;
	}
}
