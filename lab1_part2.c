#include "pico/stdlib.h"
#include "pico/util/queue.h"
#include <stdbool.h>

/* ===================== Hardware configuration ===================== */
#define LED_COUNT 4
static const uint LED_PINS[LED_COUNT] = {0, 1, 2, 3};

#define BTN1_PIN 20
#define BTN2_PIN 21
#define BTN3_PIN 22   // Part 3 will use this

#define BUTTON_DEBOUNCE_DELAY 50   // ms

/* ===================== State machine types ===================== */
typedef void (*state_func_t)(void);

typedef struct _state_t {
    uint8_t id;
    state_func_t Enter;
    state_func_t Do;
    state_func_t Exit;
    uint32_t delay_ms;
} state_t;

typedef enum _event_t {
    b1_evt = 0,
    b2_evt = 1,
    b3_evt = 2,
    no_evt = 3
} event_t;

/* ===================== Event queue ===================== */
static queue_t event_queue;

/* ===================== LED helpers ===================== */
static void leds_init(void) {
    for (int i = 0; i < LED_COUNT; i++) {
        gpio_init(LED_PINS[i]);
        gpio_set_dir(LED_PINS[i], GPIO_OUT);
        gpio_put(LED_PINS[i], 0);
    }
}

void leds_off(void) {
    for (int i = 0; i < LED_COUNT; i++) {
        gpio_put(LED_PINS[i], 0);
    }
}

void leds_on(void) {
    for (int i = 0; i < LED_COUNT; i++) {
        gpio_put(LED_PINS[i], 1);
    }
}

/* ===================== Button ISR with debounce ===================== */
void button_isr(uint gpio, uint32_t events) {
    (void)events;

    static absolute_time_t last_t1, last_t2, last_t3;
    absolute_time_t now = get_absolute_time();

    event_t evt = no_evt;

    if (gpio == BTN1_PIN) {
        if (absolute_time_diff_us(last_t1, now) < (int64_t)BUTTON_DEBOUNCE_DELAY * 1000) return;
        last_t1 = now;
        evt = b1_evt;
    } else if (gpio == BTN2_PIN) {
        if (absolute_time_diff_us(last_t2, now) < (int64_t)BUTTON_DEBOUNCE_DELAY * 1000) return;
        last_t2 = now;
        evt = b2_evt;
    } else if (gpio == BTN3_PIN) {
        if (absolute_time_diff_us(last_t3, now) < (int64_t)BUTTON_DEBOUNCE_DELAY * 1000) return;
        last_t3 = now;
        evt = b3_evt;
    } else {
        return;
    }

    /* Push event to queue (non-blocking) */
    queue_try_add(&event_queue, &evt);
}

/* ===================== Init ===================== */
void private_init(void) {
    /* Event queue */
    queue_init(&event_queue, sizeof(event_t), 32);

    /* Buttons: active-low with pull-up */
    gpio_init(BTN1_PIN); gpio_set_dir(BTN1_PIN, GPIO_IN); gpio_pull_up(BTN1_PIN);
    gpio_init(BTN2_PIN); gpio_set_dir(BTN2_PIN, GPIO_IN); gpio_pull_up(BTN2_PIN);
    gpio_init(BTN3_PIN); gpio_set_dir(BTN3_PIN, GPIO_IN); gpio_pull_up(BTN3_PIN);

    /* Enable interrupts (falling edge = button press) */
    gpio_set_irq_enabled_with_callback(BTN1_PIN, GPIO_IRQ_EDGE_FALL, true, &button_isr);
    gpio_set_irq_enabled(BTN2_PIN, GPIO_IRQ_EDGE_FALL, true);
    gpio_set_irq_enabled(BTN3_PIN, GPIO_IRQ_EDGE_FALL, true);

    /* LEDs */
    leds_init();
}

/* ===================== Event fetch ===================== */
event_t get_event(void) {
    event_t evt = no_evt;
    if (queue_try_remove(&event_queue, &evt)) {
        return evt;
    }
    return no_evt;
}

/* ===================== State implementations ===================== */
/* ---- S0: running light forward ---- */
void enter_state_0(void) { leds_off(); }
void exit_state_0(void)  { leds_off(); }

void do_state_0(void) {
    static int idx = 0;              // remembers progress across calls
    leds_off();
    gpio_put(LED_PINS[idx], 1);
    idx = (idx + 1) % LED_COUNT;     // 0→1→2→3→0
}

/* ---- S1: all LEDs blink ---- */
void enter_state_1(void) { leds_off(); }
void exit_state_1(void)  { leds_off(); }

void do_state_1(void) {
    static bool on = false;
    if (on) leds_off();
    else    leds_on();
    on = !on;
}

/* ---- S2: running light backward (faster) ---- */
void enter_state_2(void) { leds_off(); }
void exit_state_2(void)  { leds_off(); }

void do_state_2(void) {
    static int idx = LED_COUNT - 1;
    leds_off();
    gpio_put(LED_PINS[idx], 1);
    idx = (idx - 1 + LED_COUNT) % LED_COUNT; // 3→2→1→0→3
}

/* ===================== State objects ===================== */
const state_t state0 = { .id=0, .Enter=enter_state_0, .Do=do_state_0, .Exit=exit_state_0, .delay_ms=200 };
const state_t state1 = { .id=1, .Enter=enter_state_1, .Do=do_state_1, .Exit=exit_state_1, .delay_ms=300 };
const state_t state2 = { .id=2, .Enter=enter_state_2, .Do=do_state_2, .Exit=exit_state_2, .delay_ms=100 };

/* ===================== State table (Part 2) ===================== */
/* Order of events: { b1_evt, b2_evt, b3_evt, no_evt } */
static const state_t* state_table[3][4] = {
    /* From S0 */ { &state1, &state2, &state0, &state0 },
    /* From S1 */ { &state2, &state0, &state1, &state1 },
    /* From S2 */ { &state0, &state1, &state2, &state2 }
};

/* ===================== Main ===================== */
int main(void) {
    private_init();

    const state_t* current_state = &state0;
    const state_t* next_state = current_state;

    if (current_state->Enter) current_state->Enter();

    while (true) {
        /* One non-blocking step of current state */
        if (current_state->Do) current_state->Do();

        /* Timing control here (allowed) */
        sleep_ms(current_state->delay_ms);

        /* Handle event and possibly switch state */
        event_t evt = get_event();
        if (evt != no_evt) {
            next_state = state_table[current_state->id][evt];
            if (next_state != current_state) {
                if (current_state->Exit) current_state->Exit();
                current_state = next_state;
                if (current_state->Enter) current_state->Enter();
            }
        }
    }
}
