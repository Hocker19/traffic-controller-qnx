#include <pthread.h>
#include <fcntl.h>
#include <unistd.h>
#include <mqueue.h>
#include <stdlib.h>
#include "common.h"

void *stress_task(void *arg) {
    mqd_t q = mq_open(Q_VEHICLE_COUNTS, O_WRONLY);
    while (1) {
                       vehicle_count_msg_t msg = { .approach = APPROACH_NS, .count = rand() % 100, .timestamp_ms = now_ms() };
        mq_send(q, (char*)&msg, sizeof(msg), 0);
        usleep(1000);
    }
    return NULL;
}