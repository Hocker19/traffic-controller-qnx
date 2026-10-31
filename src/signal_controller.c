/*
 * signal_controller.c
 * HIGHEST priority. Owns the conflict-free state machine (state_machine.c),
 * the Signal Cycle timer, and physical light output via gpio_interface.
 *
 * Key technique: instead of sleep()-ing for the state duration, it blocks
 * on mq_timedreceive() on the emergency override queue with that duration
 * as the timeout. That means:
 *   - if no emergency arrives, the timeout fires and it's exactly as if
 *     it had slept for the Signal Cycle duration (normal cycling).
 *   - if an emergency DOES arrive, mq_timedreceive returns immediately
 *     with the message, so the emergency is handled with effectively zero
 *     extra latency instead of waiting for the current phase to finish.
 * This is what gives Novelty Item #2 (bounded, measured emergency latency)
 * teeth: the response time is architecturally near-instant, not "next
 * time we check".
 */
#include "common.h"
#include "state_machine.h"
#include "gpio_interface.h"

#include <mqueue.h>
#include <string.h>
#include <time.h>
#include <stdio.h>

static void log_line(mqd_t log_q, const char *text, uint64_t ts) {
    log_msg_t m;
    memset(&m, 0, sizeof(m));
    strncpy(m.text, text, sizeof(m.text) - 1);
    m.timestamp_ms = ts;
    mq_send(log_q, (const char *)&m, sizeof(m), 0);
}

static void apply_lights(signal_state_t s, int emergency_approach) {
    int ns, ew;
    lights_for_state(s, emergency_approach, &ns, &ew);
    gpio_set_light(APPROACH_NS, (light_color_t)ns);
    gpio_set_light(APPROACH_EW, (light_color_t)ew);
}

/* Duration (ms) the current state should hold, before any override. */
static int duration_for_state(signal_state_t s, const int suggested_green_ms[APPROACH_COUNT]) {
    switch (s) {
        case STATE_NS_GREEN: return suggested_green_ms[APPROACH_NS];
        case STATE_EW_GREEN: return suggested_green_ms[APPROACH_EW];
        case STATE_NS_YELLOW:
        case STATE_EW_YELLOW: return YELLOW_MS;
        case STATE_ALL_RED:   return ALL_RED_CLEARANCE_MS;
        default:              return BASE_GREEN_MS;
    }
}

/* Convert an absolute deadline (ms since CLOCK_MONOTONIC) into the
 * struct timespec mq_timedreceive needs — note mq_timedreceive wants
 * CLOCK_REALTIME on Linux/glibc, so we convert via now_ms() delta. */
static void deadline_to_timespec(int remaining_ms, struct timespec *ts) {
    clock_gettime(CLOCK_REALTIME, ts);
    ts->tv_sec  += remaining_ms / 1000;
    ts->tv_nsec += (remaining_ms % 1000) * 1000000L;
    if (ts->tv_nsec >= 1000000000L) {
        ts->tv_nsec -= 1000000000L;
        ts->tv_sec  += 1;
    }
}

void *signal_controller_thread(void *arg) {
    (void)arg;
    mqd_t override_q = mq_open(Q_EMERGENCY_OVERRIDE, O_RDONLY);
    mqd_t suggest_q  = mq_open(Q_TIMING_SUGGEST, O_RDONLY | O_NONBLOCK);
    mqd_t log_q      = mq_open(Q_LOG, O_WRONLY);

    signal_state_t state = STATE_NS_GREEN;
    clearance_source_t clearance_from = FROM_NS;
    int suggested_green_ms[APPROACH_COUNT] = { BASE_GREEN_MS, BASE_GREEN_MS };

    apply_lights(state, -1);
    log_line(log_q, "[SignalController] start -> NS-Green", now_ms());

    while (1) {
        /* Drain any pending density-based timing suggestions (non-blocking,
         * bounded cost) before deciding this state's hold duration. */
        timing_suggest_msg_t sug;
        while (mq_receive(suggest_q, (char *)&sug, sizeof(sug) + 16, NULL) > 0) {
            if (sug.approach >= 0 && sug.approach < APPROACH_COUNT) {
                suggested_green_ms[sug.approach] = sug.suggested_green_ms;
            }
        }

        int wait_ms = duration_for_state(state, suggested_green_ms);
        struct timespec deadline;
        deadline_to_timespec(wait_ms, &deadline);

        emergency_override_msg_t ov;
        ssize_t n = mq_timedreceive(override_q, (char *)&ov, sizeof(ov) + 16, NULL, &deadline);

        if (n > 0) {
            /* ---- Emergency path: preempt immediately ---- */
            uint64_t latency = now_ms() - ov.detected_ms;
            char buf[160];
            snprintf(buf, sizeof(buf),
                     "[SignalController] EMERGENCY override approach=%d "
                     "detect-to-green latency=%llums",
                     ov.approach, (unsigned long long)latency);
            log_line(log_q, buf, now_ms());

            state = STATE_EMERGENCY_OVERRIDE;
            apply_lights(state, ov.approach);

            /* Emergency Timeout watchdog: hold override at most
             * EMERGENCY_TIMEOUT_MS per approach, then check for recovery
             * (Novelty Item #3). If a DIFFERENT approach's override
             * arrives right at the boundary (e.g. a queued request the
             * Emergency Task just released — see emergency_task.c), we
             * switch to serving it immediately instead of losing it to
             * the race between "watchdog timed out" and "message just
             * arrived" — each new approach still gets its own full,
             * bounded EMERGENCY_TIMEOUT_MS window. */
            int keep_serving = 1;
            while (keep_serving) {
                struct timespec wd_deadline;
                deadline_to_timespec(EMERGENCY_TIMEOUT_MS, &wd_deadline);
                emergency_override_msg_t drain;

                while (mq_timedreceive(override_q, (char *)&drain, sizeof(drain) + 16, NULL, &wd_deadline) > 0) {
                    if (drain.approach != ov.approach) {
                        /* A different approach needs service - switch to it
                         * and restart the watchdog window for it. */
                        ov = drain;
                        uint64_t latency2 = now_ms() - ov.detected_ms;
                        char buf2[160];
                        snprintf(buf2, sizeof(buf2),
                                 "[SignalController] EMERGENCY switch to approach=%d "
                                 "detect-to-green latency=%llums",
                                 ov.approach, (unsigned long long)latency2);
                        log_line(log_q, buf2, now_ms());
                        apply_lights(state, ov.approach);
                        deadline_to_timespec(EMERGENCY_TIMEOUT_MS, &wd_deadline);
                    }
                    /* same approach repeated: just keep draining/holding */
                }

                /* Watchdog window timed out. One last non-blocking check
                 * to close the race where a release lands at the exact
                 * boundary instant (see emergency_task.c arbitration). */
                struct timespec immediate;
                deadline_to_timespec(0, &immediate);
                emergency_override_msg_t race_msg;
                if (mq_timedreceive(override_q, (char *)&race_msg, sizeof(race_msg) + 16, NULL, &immediate) > 0) {
                    ov = race_msg;
                    uint64_t latency3 = now_ms() - ov.detected_ms;
                    char buf3[160];
                    snprintf(buf3, sizeof(buf3),
                             "[SignalController] EMERGENCY switch to approach=%d "
                             "(boundary race) detect-to-green latency=%llums",
                             ov.approach, (unsigned long long)latency3);
                    log_line(log_q, buf3, now_ms());
                    apply_lights(state, ov.approach);
                    keep_serving = 1;   /* loop again with a fresh window */
                } else {
                    keep_serving = 0;   /* genuinely done - recover */
                }
            }

            log_line(log_q, "[SignalController] Emergency cleared / watchdog "
                             "timeout -> recovering to All-Red", now_ms());
            state = next_normal_state(state, &clearance_from);
            apply_lights(state, -1);
            continue;
        }

        /* ---- Normal path: timeout fired, advance the cycle ---- */
        state = next_normal_state(state, &clearance_from);
        apply_lights(state, -1);

        char buf[100];
        snprintf(buf, sizeof(buf), "[SignalController] -> %s", state_name(state));
        log_line(log_q, buf, now_ms());
    }
    return NULL;
}
