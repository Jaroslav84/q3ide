/*
 * q3ide_params.h — Tunable sizing parameters for Q3IDE windows.
 *
 * default_window_size:     0.0 → 13"   0.5 → ~100"   1.0 → 200"
 * default_window_distance: float fallback distance in meters
 */

#ifndef Q3IDE_PARAMS_H
#define Q3IDE_PARAMS_H

/* Window sizing: 0.0–1.0 maps linearly from SIZE_MIN to SIZE_MAX */
#define Q3IDE_DEFAULT_WINDOW_SIZE      0.5f
#define Q3IDE_DEFAULT_WINDOW_DISTANCE  3.14f  /* meters */

/* Size range in inches (Q3 units ≈ inches) */
#define Q3IDE_SIZE_MIN_INCHES    13.0f
#define Q3IDE_SIZE_MAX_INCHES   287.0f

/* 1 meter ≈ 39.37 Q3 units */
#define Q3IDE_METERS_TO_UNITS   39.37f

#endif /* Q3IDE_PARAMS_H */
