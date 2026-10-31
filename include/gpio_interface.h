/*
 * gpio_interface.h
 *
 * Hardware abstraction layer. Every task talks ONLY to these functions,
 * never directly to GPIO/PWM registers or sensor pins. That means:
 *
 *   - Before hardware arrives: link against gpio_mock.c (console-based).
 *   - After hardware arrives: link against gpio_hw.c (real GPIO/PWM +
 *     IR sensor reads) instead. No other source file changes.
 *
 * Swap is a one-line change in the Makefile (see GPIO_SRC variable).
 */
#ifndef GPIO_INTERFACE_H
#define GPIO_INTERFACE_H

typedef enum {
    LIGHT_RED = 0,
    LIGHT_YELLOW,
    LIGHT_GREEN,
    LIGHT_OFF
} light_color_t;

/* Must be called once at startup before any other gpio_* call. */
void gpio_init(void);

/* Set the physical/simulated signal head for one approach. */
void gpio_set_light(int approach, light_color_t color);

/*
 * Read the current vehicle-density sensor value for one approach.
 * Returns an integer "vehicle count in this window" — real IR/inductive
 * implementation debounces pulses; mock implementation fabricates a
 * plausible value (or reads from stdin for manual testing).
 */
int gpio_read_sensor(int approach);

/*
 * Poll the emergency-trigger input (button / IR beacon / siren detector).
 * Returns the approach number (0..APPROACH_COUNT-1) if a trigger is
 * active this call, or -1 if none.
 */
int gpio_poll_emergency(void);

/* Clean shutdown (release GPIO lines / close mock resources). */
void gpio_shutdown(void);

#endif /* GPIO_INTERFACE_H */
