/*
 * gpio_mock.c
 *
 * Console-based stand-in for real GPIO/PWM + IR sensors. Use this build
 * until hardware arrives Monday. It:
 *   - prints every light change instead of driving a real LED
 *   - fabricates a slowly-varying vehicle count per approach
 *   - lets you simulate an emergency trigger by typing "e0" or "e1" +
 *     Enter in the terminal (approach 0 = NS, 1 = EW)
 *
 * ON MONDAY: implement gpio_hw.c with the same function signatures
 * (real wiringPi/libgpiod calls + real IR reads), then change GPIO_SRC
 * in the Makefile from gpio_mock.c to gpio_hw.c. Nothing else changes.
 */
#include "gpio_interface.h"
#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>

static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;
static int g_pending_emergency_approach = -1;   /* set by stdin-reader thread */
static int g_running = 1;

static const char *color_name(light_color_t c) {
    switch (c) {
        case LIGHT_RED:    return "RED";
        case LIGHT_YELLOW: return "YELLOW";
        case LIGHT_GREEN:  return "GREEN";
        default:           return "OFF";
    }
}

/* Background thread: reads simulated emergency triggers from stdin so the
 * rest of the system behaves exactly as it will with a real IR/siren
 * detector raising gpio_poll_emergency(). */
static void *stdin_reader(void *arg) {
    (void)arg;
    char line[64];
    printf("[gpio_mock] Type 'e0' + Enter to simulate an emergency vehicle on "
           "NS approach, 'e1' for EW approach.\n");
    while (g_running && fgets(line, sizeof(line), stdin)) {
        if (strncmp(line, "e0", 2) == 0) {
            pthread_mutex_lock(&g_lock);
            g_pending_emergency_approach = 0;
            pthread_mutex_unlock(&g_lock);
            printf("[gpio_mock] Simulated EMERGENCY trigger on NS approach\n");
        } else if (strncmp(line, "e1", 2) == 0) {
            pthread_mutex_lock(&g_lock);
            g_pending_emergency_approach = 1;
            pthread_mutex_unlock(&g_lock);
            printf("[gpio_mock] Simulated EMERGENCY trigger on EW approach\n");
        }
    }
    return NULL;
}

static pthread_t g_stdin_thread;

void gpio_init(void) {
    srand((unsigned int)now_ms());
    pthread_create(&g_stdin_thread, NULL, stdin_reader, NULL);
    printf("[gpio_mock] Initialized (console simulation mode)\n");
}

void gpio_set_light(int approach, light_color_t color) {
    const char *name = (approach == 0) ? "NS" : "EW";
    printf("[gpio_mock] %-2s signal -> %s\n", name, color_name(color));
}

int gpio_read_sensor(int approach) {
    /* Fabricate a plausible, slowly varying vehicle count (0-9) */
    static int last[2] = {2, 2};
    int delta = (rand() % 3) - 1;      /* -1, 0, or +1 */
    last[approach] += delta;
    if (last[approach] < 0) last[approach] = 0;
    if (last[approach] > 9) last[approach] = 9;
    return last[approach];
}

int gpio_poll_emergency(void) {
    int result = -1;
    pthread_mutex_lock(&g_lock);
    if (g_pending_emergency_approach != -1) {
        result = g_pending_emergency_approach;
        g_pending_emergency_approach = -1;   /* consume it */
    }
    pthread_mutex_unlock(&g_lock);
    return result;
}

void gpio_shutdown(void) {
    g_running = 0;
    printf("[gpio_mock] Shutdown\n");
}
