/*
 * q3ide_placement.h — Area transition placement public API.
 *
 * Stage 4: on area transition, queue all windows for migration to wall slots.
 * Drain is FPS-gated (Q3IDE_PLACEMENT_FPS_GATE).  Streams are paused during drain.
 */
#pragma once

/* Returns non-zero while a placement queue is being drained. */
int Q3IDE_Placement_IsActive(void);

/* Queue all active tunnel windows for placement.
 * Triggered on area transition.  Calls Q3IDE_WM_PauseStreams(). */
void Q3IDE_Placement_QueueAll(void);

/* Per-frame tick: drain one item from the queue if FPS allows.
 * Calls Q3IDE_WM_ResumeStreams() when queue is empty. */
void Q3IDE_Placement_Tick(void);
