/* q3ide_log.c — Async structured logging: game thread enqueues, background thread flushes. */

#include "q3ide_log.h"
#include "q3ide_params.h"
#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <time.h>

#ifdef _WIN32
#include <process.h>
#define q3ide_getpid() ((int) _getpid())
#else
#include <unistd.h>
#define q3ide_getpid() ((int) getpid())
#endif

typedef struct {
	char line[Q3IDE_LOG_LINE_LEN];
	int is_event; /* 1 = q3ide_events.jsonl, 0 = q3ide.log */
} log_slot_t;

static log_slot_t g_slots[Q3IDE_LOG_QUEUE_CAP];
static volatile int g_head; /* next write index (producer) */
static volatile int g_tail; /* next read  index (consumer) */
static pthread_mutex_t g_mtx = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t g_cnd = PTHREAD_COND_INITIALIZER;
static pthread_t g_thr;
static volatile int g_thr_run;
static int g_file_disabled; /* 1 when q3ide_log_disable == "1" */
static FILE *g_log_f;
static FILE *g_evt_f;
static int g_pid;

static void *log_writer(void *arg)
{
	(void) arg;
	for (;;) {
		pthread_mutex_lock(&g_mtx);
		while (g_head == g_tail && g_thr_run)
			pthread_cond_wait(&g_cnd, &g_mtx);
		while (g_head != g_tail) {
			log_slot_t s = g_slots[g_tail];
			g_tail = (g_tail + 1) & (Q3IDE_LOG_QUEUE_CAP - 1);
			pthread_mutex_unlock(&g_mtx);
			if (s.is_event) {
				if (g_evt_f)
					fputs(s.line, g_evt_f);
			} else {
				if (g_log_f)
					fputs(s.line, g_log_f);
			}
			pthread_mutex_lock(&g_mtx);
		}
		int done = !g_thr_run;
		pthread_mutex_unlock(&g_mtx);
		if (g_log_f)
			fflush(g_log_f);
		if (g_evt_f)
			fflush(g_evt_f);
		if (done)
			break;
	}
	return NULL;
}

static void enqueue(const char *line, int is_event)
{
	int next;
	pthread_mutex_lock(&g_mtx);
	next = (g_head + 1) & (Q3IDE_LOG_QUEUE_CAP - 1);
	if (next != g_tail) {
		Q_strncpyz(g_slots[g_head].line, line, Q3IDE_LOG_LINE_LEN);
		g_slots[g_head].is_event = is_event;
		g_head = next;
		pthread_cond_signal(&g_cnd);
	}
	pthread_mutex_unlock(&g_mtx);
}

static void q3ide_logdir(char *out, int size)
{
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

/* ── Public API ─────────────────────────────────────────────────── */

void Q3IDE_Log_Init(void)
{
	char dir[1024], path[1100], ts[32];

	g_pid = q3ide_getpid();
	g_file_disabled = (Q_stricmp(Cvar_VariableString("q3ide_log_disable"), "1") == 0);

	if (!g_file_disabled) {
		q3ide_logdir(dir, sizeof(dir));
		q3ide_timebuf(ts, sizeof(ts));

		Com_sprintf(path, sizeof(path), "%s/q3ide.log", dir);
		g_log_f = fopen(path, "a");
		if (g_log_f) {
			fprintf(g_log_f, "\n=== SESSION pid=%d %s ===\n", g_pid, ts);
			fflush(g_log_f);
		} else {
			Com_Printf("[W] q3ide: log open failed: %s\n", path);
		}

		Com_sprintf(path, sizeof(path), "%s/q3ide_events.jsonl", dir);
		g_evt_f = fopen(path, "a");
	}

	g_thr_run = 1;
	pthread_create(&g_thr, NULL, log_writer, NULL);
	Q3IDE_Event("session_start", "");
}

void Q3IDE_Log_Shutdown(void)
{
	char ts[32];

	Q3IDE_Event("session_end", "");

	pthread_mutex_lock(&g_mtx);
	g_thr_run = 0;
	pthread_cond_signal(&g_cnd);
	pthread_mutex_unlock(&g_mtx);
	pthread_join(g_thr, NULL);

	if (g_log_f) {
		q3ide_timebuf(ts, sizeof(ts));
		fprintf(g_log_f, "=== END pid=%d %s ===\n", g_pid, ts);
		fclose(g_log_f);
		g_log_f = NULL;
	}
	if (g_evt_f) {
		fclose(g_evt_f);
		g_evt_f = NULL;
	}
}

void Q3IDE_Log(const char *level, const char *fmt, ...)
{
	char msg[Q3IDE_LOG_LINE_LEN - 32];
	char line[Q3IDE_LOG_LINE_LEN];
	va_list ap;

	if (g_file_disabled || !g_log_f)
		return;

	va_start(ap, fmt);
	Q_vsnprintf(msg, sizeof(msg), fmt, ap);
	va_end(ap);

	Com_sprintf(line, sizeof(line), "[%s] %8.3f %s\n", level, (float) Sys_Milliseconds() / 1000.0f, msg);
	enqueue(line, 0);
}

void Q3IDE_Event(const char *type, const char *extra)
{
	char line[Q3IDE_LOG_LINE_LEN];
	long ts;

	if (g_file_disabled || !g_evt_f)
		return;

	ts = (long) time(NULL);
	if (extra && extra[0])
		Com_sprintf(line, sizeof(line), "{\"ts\":%ld,\"pid\":%d,\"type\":\"%s\",%s}\n", ts, g_pid, type, extra);
	else
		Com_sprintf(line, sizeof(line), "{\"ts\":%ld,\"pid\":%d,\"type\":\"%s\"}\n", ts, g_pid, type);
	enqueue(line, 1);
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
