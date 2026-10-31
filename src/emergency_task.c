/*
 * emergency_task.c
 *
 * Two logical units, kept in one file since they're tightly coupled:
 *
 *  1. emergency_detection_thread  - polls gpio_poll_emergency() and posts
 *     raw trigger events onto Q_EMERGENCY_RAISE. This stands in for an
 *     ISR in the real system.
 *
 *  2. emergency_task_thread       - HIGHEST priority. Reads raw triggers,
 *     arbitrates when more than one approach requests emergency passage
 *     at once (Novelty Item #4), and forwards a single override command
 *     to the Signal Controller on Q_EMERGENCY_OVERRIDE. It also owns
 *     nothing about timing recovery — that watchdog lives in the Signal
 *     Controller's Emergency-Override handling (state_machine + timeout),
 *     matching the state diagram.
 */
#include "common.h"
#include "gpio_interface.h"

#include <mqueue.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

static void ms_to_timespec(uint64_t deadline_ms_monotonic, struct timespec *ts) {
    /* mq_timedreceive wants CLOCK_REALTIME; convert via the delta from
     * our monotonic now_ms() to avoid clock-source mismatches. */
    uint64_t now = now_ms();
    int64_t remaining = (int64_t)deadline_ms_monotonic - (int64_t)now;
    if (remaining < 0) remaining = 0;
    clock_gettime(CLOCK_REALTIME, ts);
    ts->tv_sec  += remaining / 1000;
    ts->tv_nsec += (remaining % 1000) * 1000000L;
    if (ts->tv_nsec >= 1000000000L) {
        ts->tv_nsec -= 1000000000L;
        ts->tv_sec  += 1;
    }
}

static void log_line(mqd_t log_q, const char *fmt, uint64_t ts, int approach) {
    log_msg_t m;
    memset(&m, 0, sizeof(m));
    snprintf(m.text, sizeof(m.text), fmt, approach);
    m.timestamp_ms = ts;
    mq_send(log_q, (const char *)&m, sizeof(m), 0);
}

void *emergency_detection_thread(void *arg) {
    (void)arg;
    mqd_t raise_q = mq_open(Q_EMERGENCY_RAISE, O_WRONLY);
    mqd_t log_q   = mq_open(Q_LOG, O_WRONLY);

    while (1) {
        int approach = gpio_poll_emergency();
        if (approach != -1) {
            emergency_raise_msg_t m;
            m.approach = approach;
            m.timestamp_ms = now_ms();
            mq_send(raise_q, (const char *)&m, sizeof(m), 0);
            log_line(log_q, "[EmergencyDetect] trigger on approach %d", m.timestamp_ms, approach);
        }
        usleep(50 * 1000);   /* poll every 50 ms - fast enough for a bounded response */
    }
    return NULL;
}

/*
 * Arbitration rule (Novelty Item #4): if a second trigger arrives while
 * one is already being served, it is queued and served immediately after
 * the first — deterministic, bounded-time, no external coordination.
 */
void *emergency_task_thread(void *arg) {
    (void)arg;
    mqd_t raise_q    = mq_open(Q_EMERGENCY_RAISE, O_RDONLY);
    mqd_t override_q = mq_open(Q_EMERGENCY_OVERRIDE, O_WRONLY);
    mqd_t log_q      = mq_open(Q_LOG, O_WRONLY);

    /* Deterministic 1-deep arbitration: while an override is "active"
     * (mirrors the Signal Controller's EMERGENCY_TIMEOUT_MS watchdog
     * window using the same shared constant, so no extra IPC is needed
     * to know when it's safe to issue the next one), any further trigger
     * is queued rather than immediately re-forwarded, and is released the
     * instant the active window ends (earliest-trigger-wins, bounded
     * wait for the second request, no starvation). */
    int      active = 0;
    uint64_t active_until_ms = 0;
    int      queued_approach = -1;
    uint64_t queued_detected_ms = 0;

    while (1) {
        emergency_raise_msg_t raise;
        ssize_t n;

        if (active && queued_approach != -1) {
            /* Something is queued behind an active window: wake up no
             * later than when that window ends, even if no new trigger
             * arrives, so the queued request is never stuck waiting. */
            struct timespec deadline;
            ms_to_timespec(active_until_ms, &deadline);
            n = mq_timedreceive(raise_q, (char *)&raise, sizeof(raise) + 16, NULL, &deadline);
        } else {
            /* Nothing queued: fine to block indefinitely for the next trigger. */
            n = mq_receive(raise_q, (char *)&raise, sizeof(raise) + 16, NULL);
        }

        uint64_t t = now_ms();
        if (active && t >= active_until_ms) {
            active = 0;   /* previous window has naturally elapsed */
        }

        if (n <= 0) {
            /* Timed out waiting (no new trigger) — just re-check the
             * active/queued release logic below. */
        } else if (!active) {
            emergency_override_msg_t ov;
            ov.approach = raise.approach;
            ov.detected_ms = raise.timestamp_ms;
            mq_send(override_q, (const char *)&ov, sizeof(ov), 0);
            log_line(log_q, "[EmergencyTask] override issued for approach %d",
                     now_ms(), raise.approach);

            active = 1;
            active_until_ms = t + EMERGENCY_TIMEOUT_MS;

            /* If something was queued from before, it stays queued and
             * will be released after THIS window (still bounded, still
             * earliest-first). */
        } else if (queued_approach == -1) {
            queued_approach = raise.approach;
            queued_detected_ms = raise.timestamp_ms;
            log_line(log_q, "[EmergencyTask] approach %d queued (arbitration)",
                     now_ms(), raise.approach);
        }
        /* A third simultaneous request while one is active and one is
         * already queued is dropped (bounded queue depth = 1) — a real
         * deployment could grow this to a small ring buffer if needed. */

        if (!active && queued_approach != -1) {
            /* Window just elapsed and something was waiting: release it now. */
            emergency_override_msg_t ov;
            ov.approach = queued_approach;
            ov.detected_ms = queued_detected_ms;
            mq_send(override_q, (const char *)&ov, sizeof(ov), 0);
            log_line(log_q, "[EmergencyTask] queued approach %d released",
                     now_ms(), queued_approach);
            active = 1;
            active_until_ms = now_ms() + EMERGENCY_TIMEOUT_MS;
            queued_approach = -1;
        }
    }
    return NULL;
}
