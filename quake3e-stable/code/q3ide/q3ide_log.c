/*
 * q3ide_log.c — Structured logging for Q3IDE.
 *
 * Writes two files in the project logs/ directory alongside engine.log:
 *   q3ide.log          — levelled text log with session boundaries
 *   q3ide_events.jsonl — one JSON object per line for agent tooling
 *
 * Log directory is resolved as <fs_basepath>/../logs (project root/logs).
 */

#include "q3ide_log.h"
#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"
#include <stdarg.h>
#include <stdio.h>
#include <time.h>

#ifdef _WIN32
#include <process.h>
#define q3ide_pid_t() ((int) _getpid())
#else
#include <unistd.h>
#define q3ide_pid_t() ((int) getpid())
#endif

static FILE *q3ide_log_f = NULL;
static FILE *q3ide_evt_f = NULL;
static int q3ide_pid = 0;

/* ── path resolution ─────────────────────────────────────────── */

static void q3ide_logdir(char *out, int size)
{
	/* fs_basepath = build/release-<os>-<arch>/; logs are 3 levels up at project root */
	const char *base = Cvar_VariableString("fs_basepath");
	if (base && base[0])
		Com_sprintf(out, size, "%s/../../../logs", base);
	else
		Com_sprintf(out, size, "../../../logs");
}

static void q3ide_timebuf(char *out, int size)
{
	time_t t = time(NULL);
	const struct tm *tm = localtime(&t);
	strftime(out, (size_t) size, "%Y-%m-%dT%H:%M:%S", tm);
}

/* ── public API ───────────────────────────────────────────────── */

void Q3IDE_Log_Init(void)
{
	char logdir[1024], path[1100], timebuf[32];

	q3ide_pid = q3ide_pid_t();
	q3ide_logdir(logdir, sizeof(logdir));
	q3ide_timebuf(timebuf, sizeof(timebuf));

	Com_sprintf(path, sizeof(path), "%s/q3ide.log", logdir);
	q3ide_log_f = fopen(path, "a");
	if (q3ide_log_f) {
		fprintf(q3ide_log_f, "\n=== SESSION pid=%d %s ===\n", q3ide_pid, timebuf);
		fflush(q3ide_log_f);
	} else {
		Com_Printf("[W] q3ide: log open failed: %s\n", path);
	}

	Com_sprintf(path, sizeof(path), "%s/q3ide_events.jsonl", logdir);
	q3ide_evt_f = fopen(path, "a");

	Q3IDE_Event("session_start", "");
}

void Q3IDE_Log_Shutdown(void)
{
	char timebuf[32];
	q3ide_timebuf(timebuf, sizeof(timebuf));
	Q3IDE_Event("session_end", "");

	if (q3ide_log_f) {
		fprintf(q3ide_log_f, "=== END pid=%d %s ===\n", q3ide_pid, timebuf);
		fclose(q3ide_log_f);
		q3ide_log_f = NULL;
	}
	if (q3ide_evt_f) {
		fclose(q3ide_evt_f);
		q3ide_evt_f = NULL;
	}
}

void Q3IDE_Log(const char *level, const char *fmt, ...)
{
	char msg[1024];
	va_list ap;
	va_start(ap, fmt);
	Q_vsnprintf(msg, sizeof(msg), fmt, ap);
	va_end(ap);

	if (q3ide_log_f) {
		float t = (float) Sys_Milliseconds() / 1000.0f;
		fprintf(q3ide_log_f, "[%s] %8.3f %s\n", level, t, msg);
		fflush(q3ide_log_f);
	}
}

void Q3IDE_Event(const char *type, const char *extra)
{
	long ts;
	if (!q3ide_evt_f)
		return;
	ts = (long) time(NULL);
	if (extra && extra[0])
		fprintf(q3ide_evt_f, "{\"ts\":%ld,\"pid\":%d,\"type\":\"%s\",%s}\n", ts, q3ide_pid, type, extra);
	else
		fprintf(q3ide_evt_f, "{\"ts\":%ld,\"pid\":%d,\"type\":\"%s\"}\n", ts, q3ide_pid, type);
	fflush(q3ide_evt_f);
}

void Q3IDE_Eventf(const char *type, const char *fmt, ...)
{
	char extra[256];
	va_list ap;
	va_start(ap, fmt);
	Q_vsnprintf(extra, sizeof(extra), fmt, ap);
	va_end(ap);
	Q3IDE_Event(type, extra);
}
