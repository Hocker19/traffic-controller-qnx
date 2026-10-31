/*
 * common.h
 * Shared definitions used by every task: queue names, message formats,
 * priority levels, and timing constants.
 *
 * Priorities below use POSIX SCHED_FIFO numbering (1-99, higher = more
 * urgent). On the QNX target these map directly to QNX thread priorities;
 * on a Linux/x86 dev VM you may need elevated privileges (sudo / capability
 * CAP_SYS_NICE) to actually apply SCHED_FIFO.
 */
#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>

/* ---------- Priorities (Problem #31 spec) ---------- */
#define PRIO_HIGHEST   50   /* Signal Controller, Emergency Task   */
#define PRIO_MEDIUM    30   /* Vehicle Detection, Traffic Analytics,
                                Diagnostics                        */
#define PRIO_LOW       10   /* Logger                              */

/* ---------- Approaches (4-way intersection) ---------- */
typedef enum {
    APPROACH_NS = 0,   /* North-South */
    APPROACH_EW = 1,   /* East-West   */
    APPROACH_COUNT
} approach_t;

/* ---------- Timing (milliseconds) ---------- */
#define BASE_GREEN_MS        6000   /* default green duration            */
#define MIN_GREEN_MS          3000   /* density adaptation lower bound    */
#define MAX_GREEN_MS         12000   /* density adaptation upper bound    */
#define YELLOW_MS             2000
#define ALL_RED_CLEARANCE_MS  1000
#define EMERGENCY_TIMEOUT_MS  8000   /* watchdog: force-exit override     */
#define VEHICLE_SAMPLE_MS      500   /* Vehicle Sampling timer period     */

/* ---------- POSIX message queue names ---------- */
#define Q_VEHICLE_COUNTS   "/tsc_vehicle_counts"   /* VehicleDetect -> Analytics       */
#define Q_TIMING_SUGGEST   "/tsc_timing_suggest"    /* Analytics -> SignalController    */
#define Q_EMERGENCY_RAISE  "/tsc_emergency_raise"   /* EmergencyDetect -> EmergencyTask */
#define Q_EMERGENCY_OVERRIDE "/tsc_emergency_override" /* EmergencyTask -> SignalController */
#define Q_LOG              "/tsc_log"               /* all tasks -> Logger              */

/* Kept <= 10 to stay within the default Linux /proc/sys/fs/mqueue/msg_max
 * limit on unprivileged dev machines. Raise this later if your QNX target
 * or dev VM allows a higher msg_max (check `cat /proc/sys/fs/mqueue/msg_max`
 * on Linux; QNX has no such default cap). */
#define MQ_MAX_MSGS   10

/* ---------- Message payloads ---------- */

/* Vehicle Detection -> Traffic Analytics: raw per-approach counts */
typedef struct {
    int      approach;
    int      count;         /* vehicles sensed in this sampling window */
    uint64_t timestamp_ms;
} vehicle_count_msg_t;

/* Traffic Analytics -> Signal Controller: proposed green duration */
typedef struct {
    int      approach;
    int      suggested_green_ms;
    uint64_t timestamp_ms;
} timing_suggest_msg_t;

/* Emergency Detection -> Emergency Task: raw trigger event */
typedef struct {
    int      approach;
    uint64_t timestamp_ms;
} emergency_raise_msg_t;

/* Emergency Task -> Signal Controller: arbitrated override command */
typedef struct {
    int      approach;       /* approach to grant emergency green    */
    uint64_t detected_ms;    /* original detection time (for latency measurement) */
} emergency_override_msg_t;

/* Generic log line sent to the Logger task */
typedef struct {
    char     text[200];
    uint64_t timestamp_ms;
} log_msg_t;

/* ---------- Helpers ---------- */
uint64_t now_ms(void);

#endif /* COMMON_H */
