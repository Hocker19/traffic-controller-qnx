/*
 * traffic_analytics.c
 * MEDIUM priority. Consumes raw vehicle counts and proposes a green-time
 * duration per approach using a simple EWMA (Novelty Item #6: no deep
 * learning, fixed and bounded execution cost, RTOS-appropriate).
 */
#include "common.h"

#include <mqueue.h>
#include <string.h>

#define EWMA_ALPHA_NUM 3   /* alpha = 0.3, expressed as integer fraction */
#define EWMA_ALPHA_DEN 10

static int clamp(int v, int lo, int hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

void *traffic_analytics_thread(void *arg) {
    (void)arg;
    mqd_t counts_q  = mq_open(Q_VEHICLE_COUNTS, O_RDONLY);
    mqd_t suggest_q = mq_open(Q_TIMING_SUGGEST, O_WRONLY);

    int ewma[APPROACH_COUNT];
    for (int i = 0; i < APPROACH_COUNT; i++) ewma[i] = 2;   /* seed value */

    while (1) {
        vehicle_count_msg_t m;
        ssize_t n = mq_receive(counts_q, (char *)&m, sizeof(m) + 16, NULL);
        if (n <= 0) continue;

        int a = m.approach;
        /* EWMA update: fixed-cost integer arithmetic, bounded execution time */
        ewma[a] = ((EWMA_ALPHA_NUM * m.count) + ((EWMA_ALPHA_DEN - EWMA_ALPHA_NUM) * ewma[a]))
                  / EWMA_ALPHA_DEN;

        /* Map density (0-9) linearly onto [MIN_GREEN_MS, MAX_GREEN_MS] */
        int span = MAX_GREEN_MS - MIN_GREEN_MS;
        int suggested = MIN_GREEN_MS + (ewma[a] * span) / 9;
        suggested = clamp(suggested, MIN_GREEN_MS, MAX_GREEN_MS);

        timing_suggest_msg_t s;
        s.approach = a;
        s.suggested_green_ms = suggested;
        s.timestamp_ms = now_ms();
        mq_send(suggest_q, (const char *)&s, sizeof(s), 0);
    }
    return NULL;
}
