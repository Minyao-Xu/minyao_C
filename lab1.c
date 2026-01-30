#include "pico/stdlib.h"
#include "pico/util/queue.h"
#include "hardware/pwm.h"
#include <stdbool.h>

#define BUTTON_DEBOUNCE_DELAY 50

#define LED1_GPIO 0
#define LED2_GPIO 1
#define LED3_GPIO 2
#define LED4_GPIO 3

#define BTN1_PIN 20
#define BTN2_PIN 21
#define BTN3_PIN 22

/* Function pointer primitive */
typedef void (*state_func_t)(void);

typedef struct _state_t {
    uint8_t id;
    state_func_t Enter;
    state_func_t Do;
    state_func_t Exit;
    uint32_t delay_ms;
} state_t;

/* Event type */
typedef enum _event_t {
    b1_evt = 0,
    b2_evt = 1,
    b3_evt = 2,
    no_evt = 3
} event_t;

static queue_t event_queue;

/* ===================== LED helpers ===================== */
void leds_off(void) {
    gpio_put(LED1_GPIO, 0);
    gpio_put(LED2_GPIO, 0);
    gpio_put(LED3_GPIO, 0);
    gpio_put(LED4_GPIO, 0);
}

void leds_on(void) {
    gpio_put(LED1_GPIO, 1);
    gpio_put(LED2_GPIO, 1);
    gpio_put(LED3_GPIO, 1);
    gpio_put(LED4_GPIO, 1);
}

/* ===================== Button ISR with debounce ===================== */
unsigned long button_time = 0;

void button_isr(uint gpio, uint32_t events) 
{
    if ((to_ms_since_boot(get_absolute_time())-button_time) > BUTTON_DEBOUNCE_DELAY) 
    {
        button_time = to_ms_since_boot(get_absolute_time());
        
        event_t evt;
        switch(gpio)
        {
            case BTN1_PIN: 
                evt = b1_evt; 
                queue_try_add(&event_queue, &evt); 
            break; 

            case BTN2_PIN: 
                evt = b2_evt; 
                queue_try_add(&event_queue, &evt); 
            break;

            case BTN3_PIN: 
                evt = b3_evt; 
                queue_try_add(&event_queue, &evt);
            break;
        }
    }
}

/* ===================== Init ===================== */
void private_init(void) {
    /* Event queue setup */
    queue_init(&event_queue, sizeof(event_t), 32);

    /* Button setup: active-low with pull-up */
    gpio_init(BTN1_PIN); gpio_set_dir(BTN1_PIN, GPIO_IN); gpio_pull_up(BTN1_PIN);
    gpio_init(BTN2_PIN); gpio_set_dir(BTN2_PIN, GPIO_IN); gpio_pull_up(BTN2_PIN);
    gpio_init(BTN3_PIN); gpio_set_dir(BTN3_PIN, GPIO_IN); gpio_pull_up(BTN3_PIN);

    /* Enable interrupts: falling edge = press */
    gpio_set_irq_enabled_with_callback(BTN1_PIN, GPIO_IRQ_EDGE_FALL, true, &button_isr);
    gpio_set_irq_enabled(BTN2_PIN, GPIO_IRQ_EDGE_FALL, true);
    gpio_set_irq_enabled(BTN3_PIN, GPIO_IRQ_EDGE_FALL, true);

    /* LED setup */
    gpio_init(LED1_GPIO); gpio_set_dir(LED1_GPIO, GPIO_OUT); gpio_put(LED1_GPIO, 0); 
    gpio_init(LED2_GPIO); gpio_set_dir(LED2_GPIO, GPIO_OUT); gpio_put(LED2_GPIO, 0);
    gpio_init(LED3_GPIO); gpio_set_dir(LED3_GPIO, GPIO_OUT); gpio_put(LED3_GPIO, 0);
    gpio_init(LED4_GPIO); gpio_set_dir(LED4_GPIO, GPIO_OUT); gpio_put(LED4_GPIO, 0);
}

/* ===================== Event get ===================== */
event_t get_event(void)
{
    event_t evt = no_evt; 
    if (queue_try_remove(&event_queue, &evt))
    { 
        return evt; 
    }
    return no_evt; 
}

/* ===================== State implementations ===================== */
/* ---- S0: running light forward ---- */
void enter_state_0(void) { leds_off(); }
void exit_state_0(void)  { leds_off(); }

void do_state_0(void) {
    static int i = 0;
    leds_off();
    switch(i) {
        case 0: gpio_put(LED1_GPIO, 1); break;
        case 1: gpio_put(LED2_GPIO, 1); break;
        case 2: gpio_put(LED3_GPIO, 1); break;
        case 3: gpio_put(LED4_GPIO, 1); break;
    }
    i = (i + 1) % 4;
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

/* ---- S2: running light backward ---- */
void enter_state_2(void) { leds_off(); }
void exit_state_2(void)  { leds_off(); }

void do_state_2(void) {
    static int i = 3;   // start from last LED
    leds_off();

    switch(i) {
        case 0: gpio_put(LED1_GPIO, 1); break;
        case 1: gpio_put(LED2_GPIO, 1); break;
        case 2: gpio_put(LED3_GPIO, 1); break;
        case 3: gpio_put(LED4_GPIO, 1); break;
    }

    i = (i - 1 + 4) % 4;
}

/* ---- S3: PWM on LED1 (LED1_GPIO) ---- */
static uint pwm_slice = 0;
static uint pwm_chan  = 0;

void enter_state_3(void) {
    leds_off();

    /* switch LED1_GPIO to PWM function */
    gpio_set_function(LED1_GPIO, GPIO_FUNC_PWM);

    pwm_slice = pwm_gpio_to_slice_num(LED1_GPIO);
    pwm_chan  = pwm_gpio_to_channel(LED1_GPIO);

    pwm_config cfg = pwm_get_default_config();

    /* PWM clock ~ 125 MHz with wrap=62500 and clkdiv=4
       freq ~ 125e6 / 4 / 62500 ≈ 500 Hz */
    pwm_config_set_clkdiv(&cfg, 4.0f);
    pwm_config_set_wrap(&cfg, 62500);

    pwm_init(pwm_slice, &cfg, true);

    pwm_set_chan_level(pwm_slice, pwm_chan, 0);
}

void exit_state_3(void) {
    /* disable PWM and restore GPIO */
    pwm_set_enabled(pwm_slice, false);

    gpio_set_function(LED1_GPIO, GPIO_FUNC_SIO);
    gpio_set_dir(LED1_GPIO, GPIO_OUT);
    gpio_put(LED1_GPIO, 0);

    leds_off();
}

void do_state_3(void) {
    static int level = 0;
    static int step  = 500;  // tune speed = higher -> faster
    static int dir   = 1;

    level += dir * step;

    if (level >= 65535) { level = 65535; dir = -1; }
    if (level <= 0)     { level = 0;     dir = 1;  }

    pwm_set_chan_level(pwm_slice, pwm_chan, (uint16_t)level);
}

/* ===================== State objects ===================== */
const state_t state0 = { 0, enter_state_0, do_state_0, exit_state_0, 500 };
const state_t state1 = { 1, enter_state_1, do_state_1, exit_state_1, 300 };
const state_t state2 = { 2, enter_state_2, do_state_2, exit_state_2, 100 };
const state_t state3 = { 3, enter_state_3, do_state_3, exit_state_3, 10 };

/* ===================== State table ===================== */
static const state_t* state_table[4][4] = {
    /*       {  b1_evt,  b2_evt,  b3_evt,  no_evt } */
    /* S0 */ { &state2, &state1, &state3, &state0 },
    /* S1 */ { &state0, &state2, &state3, &state1 },
    /* S2 */ { &state1, &state0, &state3, &state2 },
    /* S3 */ { &state0, &state0, &state0, &state3 }  // any button -> S0, none -> stay S3
};

/* ===================== Main ===================== */
int main(void) {
    private_init();

    const state_t* current_state = &state0;
    const state_t* next_state    = current_state;

    /* Enter only once at startup */
    if (current_state->Enter) {
        current_state->Enter();
    }

    while (1) {
        /* One non-blocking step */
        if (current_state->Do) {
            current_state->Do();
        }

        /* Pace the loop (allowed here, not inside Do) */
        sleep_ms(current_state->delay_ms);

        /* Fetch event */
        event_t evt = get_event();

        /* Decide next state */
        if (evt != no_evt) {
            next_state = state_table[current_state->id][evt];

            /* Transition only if state actually changes */
            if (next_state != current_state) {
                if (current_state->Exit) {
                    current_state->Exit();
                }

                current_state = next_state;

                if (current_state->Enter) {
                    current_state->Enter();
                }
            }
        }
    }
}