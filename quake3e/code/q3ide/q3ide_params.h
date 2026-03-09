/*
 * q3ide_params.h — Read-only design parameters for q3ide.
 *
 * These values are set once at compile time and never mutated at runtime.
 * Input (keyboard, mouse, "K" mode) must never write to this struct.
 */
#pragma once

typedef struct {
	/* Laser pointer ribbon width in world units (screen-space ~2 px at typical distance). */
	float laserPointerWidth;
} q3ide_params_t;

/* Singleton — defined in q3ide_laser.c, read-only everywhere else. */
extern const q3ide_params_t q3ide_params;
