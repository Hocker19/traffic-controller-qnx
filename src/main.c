/*
 * main.c
 * Creates all POSIX message queues, spawns every task as a pthread with
 * the priority specified in the Problem #31 architecture table, and
 * initializes the (mock, for now) hardware layer.
 *
 * NOTE on priorities: SCHED_FIFO priority-setting typically needs root /
 * CAP_SYS_NICE on Linux dev machines. On the QNX target this is normal
 * and expected to work without extra privileges. If pthread_attr_setschedpolicy
 * fails silently on your dev VM, that's fine for LOGIC testing — just
 * re-verify actual preemption behavior once you're on the real QNX target.
 */
#include "common.h"
#include "gpio_interface.h"

#include <mqueue.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

extern void *vehicle_detection_thread(void *arg);
extern void *traffic_analytics_thread(void *arg);
extern void *emergency_detection_thread(void *arg);
extern void *emergency_task_thread(void *arg);
extern void *signal_controller_thread(void *arg);
extern void *logger_thread(void *arg);

static mqd_t open_or_create(const char *name, long max_msgs, long max_size) {
    struct mq_attr attr;
    memset(&attr, 0, sizeof(attr));
    attr.mq_flags = 0;
    attr.mq_maxmsg = max_msgs;
    attr.mq_msgsize = max_size;

    mq_unlink(name);   /* clean start each run */
    mqd_t q = mq_open(name, O_CREAT | O_RDWR, 0644, &attr);
    if (q == (mqd_t)-1) {
        perror(name);
    }
    return q;
}

static void spawn(pthread_t *tid, void *(*fn)(void *), int priority, const char *label) {
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);
    pthread_attr_setschedpolicy(&attr, SCHED_FIFO);

    struct sched_param sp;
    sp.sched_priority = priority;
    pthread_attr_setschedparam(&attr, &sp);

    int rc = pthread_create(tid, &attr, fn, NULL);
    if (rc != 0) {
        /* Fall back to default attrs if SCHED_FIFO isn't permitted on this
         * host (common on unprivileged Linux dev VMs) so you can still
         * test LOGIC before hardware / QNX target is available. */
        fprintf(stderr, "[main] warning: could not set SCHED_FIFO/prio for %s "
                         "(errno via pthread_create=%d) - retrying with default attrs\n",
                         label, rc);
        pthread_create(tid, NULL, fn, NULL);
    }
    pthread_attr_destroy(&attr);
}

int main(void) {
    printf("=== Adaptive Smart Traffic Signal Controller (software skeleton) ===\n");
    printf("Using MOCK GPIO layer - safe to run with no hardware attached.\n\n");

    /* Message queues */
    open_or_create(Q_VEHICLE_COUNTS,   MQ_MAX_MSGS, sizeof(vehicle_count_msg_t) + 16);
    open_or_create(Q_TIMING_SUGGEST,   MQ_MAX_MSGS, sizeof(timing_suggest_msg_t) + 16);
    open_or_create(Q_EMERGENCY_RAISE,  MQ_MAX_MSGS, sizeof(emergency_raise_msg_t) + 16);
    open_or_create(Q_EMERGENCY_OVERRIDE, MQ_MAX_MSGS, sizeof(emergency_override_msg_t) + 16);
    open_or_create(Q_LOG,              MQ_MAX_MSGS, sizeof(log_msg_t) + 16);

    gpio_init();

    pthread_t t_vehicle, t_analytics, t_ed, t_et, t_sc, t_log;

    /* Priorities per the Problem #31 task/priority architecture table */
    spawn(&t_log,       logger_thread,              PRIO_LOW,     "Logger");
    spawn(&t_vehicle,   vehicle_detection_thread,   PRIO_MEDIUM,  "VehicleDetection");
    spawn(&t_analytics, traffic_analytics_thread,   PRIO_MEDIUM,  "TrafficAnalytics");
    spawn(&t_ed,        emergency_detection_thread, PRIO_HIGHEST, "EmergencyDetection");
    spawn(&t_et,        emergency_task_thread,      PRIO_HIGHEST, "EmergencyTask");
    spawn(&t_sc,        signal_controller_thread,   PRIO_HIGHEST, "SignalController");

    /* Run forever - Ctrl+C to stop */
    pthread_join(t_sc, NULL);

    gpio_shutdown();
    return 0;
}
