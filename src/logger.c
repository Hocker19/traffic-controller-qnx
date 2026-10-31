/*
 * logger.c
 * LOW priority (deliberately — it must never delay the safety-critical
 * tasks). Drains the shared log queue and prints the mandatory CLI
 * "Signal Timeline" view. Also demonstrates deadline-miss instrumentation
 * (Novelty Item #5): any log line whose processing is delayed past a
 * threshold gets flagged.
 */
#include "common.h"

#include <mqueue.h>
#include <stdio.h>
#include <string.h>

#define DEADLINE_MISS_THRESHOLD_MS 200

void *logger_thread(void *arg) {
    (void)arg;
    mqd_t log_q = mq_open(Q_LOG, O_RDONLY);

    int deadline_misses = 0;
    int lines_printed = 0;

    printf("\n===== Signal Timeline / CLI Dashboard =====\n");

    while (1) {
        log_msg_t m;
        ssize_t n = mq_receive(log_q, (char *)&m, sizeof(m) + 16, NULL);
        if (n <= 0) continue;

        uint64_t received_at = now_ms();
        uint64_t age = (received_at >= m.timestamp_ms) ? (received_at - m.timestamp_ms) : 0;
        if (age > DEADLINE_MISS_THRESHOLD_MS) {
            deadline_misses++;
            printf("[DEADLINE MISS #%d, delayed %llums] ", deadline_misses,
                   (unsigned long long)age);
        }

        printf("t=%8llu ms | %s\n", (unsigned long long)m.timestamp_ms, m.text);
        fflush(stdout);
        lines_printed++;

        if (lines_printed % 20 == 0) {
            printf("---- (deadline misses so far: %d) ----\n", deadline_misses);
        }
    }
    return NULL;
}
