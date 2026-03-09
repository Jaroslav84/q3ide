/*
 * q3ide_log.h — Structured logging for Q3IDE.
 *
 * Two output streams written alongside Com_Printf (engine.log):
 *   logs/q3ide.log          — levelled text, session boundaries, timestamps
 *   logs/q3ide_events.jsonl — newline-delimited JSON for agent tooling
 *
 * Usage:
 *   Q3IDE_LOGI("dylib loaded ok");
 *   Q3IDE_LOGW("shader slot full");
 *   Q3IDE_LOGE("capture start failed id=%u", id);
 *   Q3IDE_Event("window_attached", "\"wid\":1234,\"wall\":0,\"x\":64");
 */

#ifndef Q3IDE_LOG_H
#define Q3IDE_LOG_H

/* Open log files and write session-start marker. Call from Q3IDE_Init. */
void Q3IDE_Log_Init(void);

/* Write session-end marker and close files. Call from Q3IDE_Shutdown. */
void Q3IDE_Log_Shutdown(void);

/*
 * Write a levelled message to q3ide.log AND Com_Printf.
 * level: "I" info | "W" warning | "E" error
 */
void Q3IDE_Log(const char *level, const char *fmt, ...);

/*
 * Append one JSON event to q3ide_events.jsonl.
 * extra: additional "key":value pairs (comma-separated, no outer braces).
 *        Pass "" for events with no extra fields.
 */
void Q3IDE_Event(const char *type, const char *extra);

/* Convenience wrapper: format extra JSON fields inline. */
void Q3IDE_Eventf(const char *type, const char *fmt, ...);

#define Q3IDE_LOGI(...) Q3IDE_Log("I", __VA_ARGS__)
#define Q3IDE_LOGW(...) Q3IDE_Log("W", __VA_ARGS__)
#define Q3IDE_LOGE(...) Q3IDE_Log("E", __VA_ARGS__)

#endif /* Q3IDE_LOG_H */
