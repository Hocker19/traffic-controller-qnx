/*
 * vehicle_detection.c
 * MEDIUM priority. Periodic (Vehicle Sampling timer) read of each
 * approach's sensor via the gpio_interface abstraction, forwarded to
 * Traffic Analytics.
 */
#include "common.h"
#include "gpio_interface.h"

#include <mqueue.h>
#include <unistd.h>
#include <string.h>

void *vehicle_detection_thread(void *arg) {
    (void)arg;
    mqd_t counts_q = mq_open(Q_VEHICLE_COUNTS, O_WRONLY);

    while (1) {
        for (int approach = 0; approach < APPROACH_COUNT; approach++) {
            vehicle_count_msg_t m;
            m.approach = approach;
            m.count = gpio_read_sensor(approach);
            m.timestamp_ms = now_ms();
            mq_send(counts_q, (const char *)&m, sizeof(m), 0);
        }
        usleep(VEHICLE_SAMPLE_MS * 1000);
    }
    return NULL;
}
