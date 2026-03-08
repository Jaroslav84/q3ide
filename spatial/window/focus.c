/*
 * spatial/window/focus.c — Crosshair focus system implementation.
 *
 * Ray-casting, dwell tracking, and interaction mode transitions.
 * Pure math, no engine dependencies (only math.h).
 */

#include "focus.h"
#include <math.h>
#include <string.h>

/* ─── Math Helpers ────────────────────────────────────────────────────────── */

static void vec3_normalize(float *v) {
	float len = sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
	if (len > 0.0001f) {
		v[0] /= len;
		v[1] /= len;
		v[2] /= len;
	}
}

static float vec3_dot(const float *a, const float *b) {
	return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

static void vec3_cross(const float *a, const float *b, float *out) {
	out[0] = a[1] * b[2] - a[2] * b[1];
	out[1] = a[2] * b[0] - a[0] * b[2];
	out[2] = a[0] * b[1] - a[1] * b[0];
}

static void vec3_sub(const float *a, const float *b, float *out) {
	out[0] = a[0] - b[0];
	out[1] = a[1] - b[1];
	out[2] = a[2] - b[2];
}

static float fclamp(float x, float lo, float hi) {
	return x < lo ? lo : (x > hi ? hi : x);
}

/* ─── Initialization ──────────────────────────────────────────────────────── */

void focus_init(focus_state_t *fs) {
	memset(fs, 0, sizeof(focus_state_t));
	fs->focused_win = -1;
	fs->mode = MODE_FPS;
	fs->dwell_start_ms = -1.0f;
	fs->hover_progress = 0.0f;
	fs->pointer_active = 0;
	fs->keyboard_active = 0;
}

/* ─── Ray-Window Intersection ─────────────────────────────────────────────── */

int focus_ray_hit_window(const window_entity_t *w, const float *eye,
                         const float *fwd, float *out_uv, float *out_dist) {
	/* Plane-ray intersection: (hit - origin) · normal = 0 */
	float d_dot_n = vec3_dot(fwd, w->normal);
	if (fabsf(d_dot_n) < 0.0001f) {
		return 0; /* Ray parallel to plane */
	}

	float to_origin[3];
	vec3_sub(w->origin, eye, to_origin);
	float dist = vec3_dot(to_origin, w->normal) / d_dot_n;

	if (dist <= 0.0f || dist > 100000.0f) {
		return 0; /* Behind eye or too far */
	}

	/* Hit point */
	float hit[3];
	hit[0] = eye[0] + fwd[0] * dist;
	hit[1] = eye[1] + fwd[1] * dist;
	hit[2] = eye[2] + fwd[2] * dist;

	/* Compute local basis (right, up) */
	float right[3], up[3];

	/* up = (0, 0, 1) always (upright window) */
	up[0] = 0.0f;
	up[1] = 0.0f;
	up[2] = 1.0f;

	/* right = -normal.y, normal.x, 0 (perpendicular in XY plane) */
	right[0] = -w->normal[1];
	right[1] = w->normal[0];
	right[2] = 0.0f;
	vec3_normalize(right);

	/* Check if normal is nearly vertical (ceiling/floor) */
	if (fabsf(w->normal[2]) > 0.9f) {
		/* Fallback: right = (1, 0, 0), up = (0, 1, 0) */
		right[0] = 1.0f;
		right[1] = 0.0f;
		right[2] = 0.0f;
		up[0] = 0.0f;
		up[1] = 1.0f;
		up[2] = 0.0f;
	}

	/* Local coordinates of hit point */
	float rel[3];
	vec3_sub(hit, w->origin, rel);

	float local_x = vec3_dot(rel, right);
	float local_y = vec3_dot(rel, up);

	/* Test bounds */
	float hx = w->world_w * 0.5f;
	float hy = w->world_h * 0.5f;

	if (fabsf(local_x) > hx || fabsf(local_y) > hy) {
		return 0; /* Outside rectangle */
	}

	/* Compute UV (0..1) */
	out_uv[0] = (local_x + hx) / w->world_w;
	out_uv[1] = (local_y + hy) / w->world_h;
	*out_dist = dist;

	return 1;
}

/* ─── Focus Update ────────────────────────────────────────────────────────── */

void focus_update(focus_state_t *fs, const window_entity_t *wins, int n,
                  const float *eye, const float *fwd, float dt_ms) {
	int best_idx = -1;
	float best_dist = 1e10f;
	float best_uv[2] = {0.0f, 0.0f};

	/* Ray-cast all active windows */
	for (int i = 0; i < n && i < Q3IDE_MAX_WINDOWS; i++) {
		if (!wins[i].active || wins[i].status == WIN_STATUS_INACTIVE) {
			continue;
		}

		float hit_uv[2];
		float hit_dist = 0.0f;

		if (focus_ray_hit_window(&wins[i], eye, fwd, hit_uv, &hit_dist)) {
			if (hit_dist < best_dist) {
				best_dist = hit_dist;
				best_idx = i;
				best_uv[0] = hit_uv[0];
				best_uv[1] = hit_uv[1];
			}
		}
	}

	/* Update dwell and hover progress */
	if (best_idx == fs->focused_win && best_idx >= 0) {
		/* Same window, continue dwell */
		if (fs->dwell_start_ms < 0.0f) {
			fs->dwell_start_ms = 0.0f; /* Mock: Sys_Milliseconds() would go here */
		}
	} else {
		/* Different window or no hit, reset dwell */
		fs->dwell_start_ms = -1.0f;
		fs->hover_progress = 0.0f;
	}

	/* Accumulate dwell time (mock: frame-based) */
	if (fs->dwell_start_ms >= 0.0f && best_idx >= 0) {
		fs->dwell_start_ms += dt_ms;
		float dwell_elapsed = fs->dwell_start_ms;
		fs->hover_progress = fclamp(dwell_elapsed / Q3IDE_DWELL_MS, 0.0f, 1.0f);
	}

	/* Update focus state */
	fs->focused_win = best_idx;
	fs->hit_uv[0] = best_uv[0];
	fs->hit_uv[1] = best_uv[1];
	fs->hit_dist = best_dist;

	if (best_idx >= 0 && best_idx < n && wins[best_idx].active) {
		fs->hit_point[0] = wins[best_idx].origin[0];
		fs->hit_point[1] = wins[best_idx].origin[1];
		fs->hit_point[2] = wins[best_idx].origin[2];
	}
}

/* ─── Mode Transitions ────────────────────────────────────────────────────── */

void focus_enter_pointer(focus_state_t *fs) {
	fs->mode = MODE_POINTER;
	fs->pointer_active = 1;
}

void focus_exit_pointer(focus_state_t *fs) {
	fs->mode = MODE_FPS;
	fs->pointer_active = 0;
}

void focus_enter_keyboard(focus_state_t *fs) {
	fs->mode = MODE_KEYBOARD;
	fs->keyboard_active = 1;
}

void focus_exit_keyboard(focus_state_t *fs) {
	fs->mode = MODE_FPS;
	fs->keyboard_active = 0;
}

/* ─── Pointer Movement ────────────────────────────────────────────────────── */

int focus_pointer_move(focus_state_t *fs, float dx, float dy,
                       float window_w_px, float window_h_px) {
	/* Convert pixel deltas to UV deltas */
	float du = dx / window_w_px;
	float dv = dy / window_h_px;

	fs->pointer_uv[0] += du;
	fs->pointer_uv[1] += dv;

	/* Clamp to [0, 1] */
	fs->pointer_uv[0] = fclamp(fs->pointer_uv[0], 0.0f, 1.0f);
	fs->pointer_uv[1] = fclamp(fs->pointer_uv[1], 0.0f, 1.0f);

	/* Check edge zone: edge_px as fraction of window */
	float edge_zone_u = Q3IDE_EDGE_ZONE_PX / window_w_px;
	float edge_zone_v = Q3IDE_EDGE_ZONE_PX / window_h_px;

	int at_edge = (fs->pointer_uv[0] < edge_zone_u ||
	               fs->pointer_uv[0] > 1.0f - edge_zone_u ||
	               fs->pointer_uv[1] < edge_zone_v ||
	               fs->pointer_uv[1] > 1.0f - edge_zone_v);

	return at_edge ? 0 : 1; /* 1 = inside safe zone, 0 = at edge */
}
