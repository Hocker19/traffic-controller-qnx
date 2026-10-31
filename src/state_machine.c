#include "state_machine.h"
#include <stddef.h>

const char *state_name(signal_state_t s) {
    switch (s) {
        case STATE_NS_GREEN:            return "NS-Green";
        case STATE_NS_YELLOW:           return "NS-Yellow";
        case STATE_ALL_RED:             return "All-Red";
        case STATE_EW_GREEN:            return "EW-Green";
        case STATE_EW_YELLOW:           return "EW-Yellow";
        case STATE_EMERGENCY_OVERRIDE:  return "Emergency-Override";
        default:                        return "UNKNOWN";
    }
}

/*
 * Explicit transition table. This function is the ONLY place that decides
 * what the next normal-cycle state is — every legal transition is listed
 * here and nowhere else, which is what makes conflicting greens
 * structurally unreachable rather than merely "checked for".
 */
signal_state_t next_normal_state(signal_state_t current, clearance_source_t *clearance_from) {
    switch (current) {
        case STATE_NS_GREEN:
            return STATE_NS_YELLOW;

        case STATE_NS_YELLOW:
            *clearance_from = FROM_NS;
            return STATE_ALL_RED;

        case STATE_ALL_RED:
            /* All-Red is shared: which side it advances to depends on
             * which side it was clearing from. */
            return (*clearance_from == FROM_NS) ? STATE_EW_GREEN : STATE_NS_GREEN;

        case STATE_EW_GREEN:
            return STATE_EW_YELLOW;

        case STATE_EW_YELLOW:
            *clearance_from = FROM_EW;
            return STATE_ALL_RED;

        case STATE_EMERGENCY_OVERRIDE:
            /* Recovery always lands on All-Red first (safe state), then
             * the normal cycle resumes from there. Default to resuming
             * toward NS-Green. */
            *clearance_from = FROM_EW;   /* so All-Red -> NS-Green next */
            return STATE_ALL_RED;

        default:
            return STATE_ALL_RED;
    }
}

void lights_for_state(signal_state_t s, int emergency_approach,
                       int *ns_light, int *ew_light) {
    /* light values: 0=RED 1=YELLOW 2=GREEN, matches light_color_t in
     * gpio_interface.h (kept as plain int here to avoid a header cycle) */
    switch (s) {
        case STATE_NS_GREEN:
            *ns_light = 2; *ew_light = 0; break;
        case STATE_NS_YELLOW:
            *ns_light = 1; *ew_light = 0; break;
        case STATE_ALL_RED:
            *ns_light = 0; *ew_light = 0; break;
        case STATE_EW_GREEN:
            *ns_light = 0; *ew_light = 2; break;
        case STATE_EW_YELLOW:
            *ns_light = 0; *ew_light = 1; break;
        case STATE_EMERGENCY_OVERRIDE:
            /* Only the emergency approach gets green; everything else red */
            *ns_light = (emergency_approach == 0) ? 2 : 0;
            *ew_light = (emergency_approach == 1) ? 2 : 0;
            break;
        default:
            *ns_light = 0; *ew_light = 0; break;
    }
}
