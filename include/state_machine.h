/*
 * state_machine.h
 *
 * The conflict-free traffic signal state machine (Novelty Item #1).
 * Only the transitions implemented in state_machine.c are reachable —
 * there is no code path that can set two conflicting greens at once.
 */
#ifndef STATE_MACHINE_H
#define STATE_MACHINE_H

#include <stdint.h>

typedef enum {
    STATE_NS_GREEN = 0,
    STATE_NS_YELLOW,
    STATE_ALL_RED,        /* shared clearance / recovery state */
    STATE_EW_GREEN,
    STATE_EW_YELLOW,
    STATE_EMERGENCY_OVERRIDE,
    STATE_COUNT
} signal_state_t;

/* Which normal-cycle phase All-Red is clearing FROM, so it knows what
 * to advance to next ("clearance done" -> EW-Green or -> NS-Green). */
typedef enum {
    FROM_NS = 0,
    FROM_EW = 1
} clearance_source_t;

const char *state_name(signal_state_t s);

/*
 * Returns the next state in the NORMAL (non-emergency) cycle, given the
 * current state and which side All-Red is clearing from. Does not know
 * about emergencies — the caller (Signal Controller) handles the
 * emergency interrupt separately, exactly as shown in the state diagram.
 */
signal_state_t next_normal_state(signal_state_t current, clearance_source_t *clearance_from);

/* Which physical lights should be ON for a given state. */
void lights_for_state(signal_state_t s, int emergency_approach,
                       int *ns_light, int *ew_light);

#endif /* STATE_MACHINE_H */
