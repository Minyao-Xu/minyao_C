#include "pico/stdlib.h"

/* PART 1 only: state 0 "running light" on first 4 LEDs */

/* Recommended: do not set 0 for debounce; but Part 1 doesn't use buttons yet */
#define BUTTON_DEBOUNCE_DELAY  50

/* LED pins for the first 4 LEDs (adjust if your board differs) */
static const uint LED_PINS[4] = {0, 1, 2, 3};

/* Function pointer primitive */
typedef void (*state_func_t)(void);

typedef struct _state_t {
    uint8_t id;
    state_func_t Enter;
    state_func_t Do;
    state_func_t Exit;
    uint32_t delay_ms;
} state_t;

/* ---------- PART 1: LED helpers ---------- */
static void leds_init(void) {
    for (int i = 0; i < 4; i++) {
        gpio_init(LED_PINS[i]);
        gpio_set_dir(LED_PINS[i], GPIO_OUT);
        gpio_put(LED_PINS[i], 0);
    }
}

void leds_off(void) {
    for (int i = 0; i < 4; i++) {
        gpio_put(LED_PINS[i], 0);
    }
}

/* Optional in Part 1; provided for completeness */
void leds_on(void) {
    for (int i = 0; i < 4; i++) {
        gpio_put(LED_PINS[i], 1);
    }
}

/* ---------- PART 1: State 0 implementation ---------- */
static void enter_state_0(void) {
    leds_off();
}

static void exit_state_0(void) {
    leds_off();
}

void do_state_0(void) {
    /* Must be non-blocking: no delay, no loops that wait.
       Use a static index to remember progress between calls. */
    static int idx = 0;

    leds_off();
    gpio_put(LED_PINS[idx], 1);

    idx = (idx + 1) % 4;
}

/* State 0 definition */
const state_t state0 = {
    .id = 0,
    .Enter = enter_state_0,
    .Do = do_state_0,
    .Exit = exit_state_0,
    .delay_ms = 200   /* adjust speed here */
};

static void private_init(void) {
    /* Part 1: only LEDs */
    leds_init();
}

/* ---------- PART 1: Minimal main loop ---------- */
int main(void) {
    private_init();

    state_t current_state = state0;

    /* Enter once */
    if (current_state.Enter) current_state.Enter();

    while (true) {
        /* Do step */
        if (current_state.Do) current_state.Do();

        /* Timing is allowed here (not inside Do) */
        sleep_ms(current_state.delay_ms);
    }

    /* Unreachable in embedded main, but kept for structure */
    if (current_state.Exit) current_state.Exit();
    return 0;
}
