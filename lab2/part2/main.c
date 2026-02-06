#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/devicetree.h>

/* ---- LEDs + Button from overlay aliases ----
 * overlay needs:
 *   aliases { led0 = &...; led1 = &...; led2 = &...; led3 = &...; sw0 = &...; }
 */
static const struct gpio_dt_spec led0 = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);
static const struct gpio_dt_spec led1 = GPIO_DT_SPEC_GET(DT_ALIAS(led1), gpios);
static const struct gpio_dt_spec led2 = GPIO_DT_SPEC_GET(DT_ALIAS(led2), gpios);
static const struct gpio_dt_spec led3 = GPIO_DT_SPEC_GET(DT_ALIAS(led3), gpios);

static const struct gpio_dt_spec button = GPIO_DT_SPEC_GET(DT_ALIAS(sw0), gpios);

/* GPIO interrupt callback object */
static struct gpio_callback button_cb;

/* Semaphore: ISR -> button_task event notification */
static struct k_sem button_sem;

/* Mutex: protect shared state (current_led) */
static struct k_mutex led_lock;

#define DEBOUNCE_MS  50
#define BLINK_MS     300

/* Shared state: current LED index: 0..3 (protected by led_lock) */
static uint8_t current_led = 0;

/* Debounce state lives in task context (no need for volatile) */
static uint32_t last_accepted_press_ms = 0;

/* helper: turn all LEDs OFF */
static void leds_all_off(void)
{
    (void)gpio_pin_set_dt(&led0, 0);
    (void)gpio_pin_set_dt(&led1, 0);
    (void)gpio_pin_set_dt(&led2, 0);
    (void)gpio_pin_set_dt(&led3, 0);
}

/* helper: get pointer to selected LED */
static const struct gpio_dt_spec *get_led(uint8_t idx)
{
    switch (idx) {
    case 0:  return &led0;
    case 1:  return &led1;
    case 2:  return &led2;
    default: return &led3;
    }
}

/* -------------------- blinky task -------------------- */
/* Only the "current_led" is allowed to blink */
void blinky_task(void)
{
    while (1) {
        uint8_t idx;

        /* Read shared state under mutex, then use local copy */
        k_mutex_lock(&led_lock, K_FOREVER);
        idx = current_led;
        k_mutex_unlock(&led_lock);


        const struct gpio_dt_spec *led = get_led(idx);


        /*
         * Make sure others stay OFF.
         * This keeps the invariant "only one LED can be ON/blinking".
         */
        if (led != &led0) (void)gpio_pin_set_dt(&led0, 0);
        if (led != &led1) (void)gpio_pin_set_dt(&led1, 0);
        if (led != &led2) (void)gpio_pin_set_dt(&led2, 0);
        if (led != &led3) (void)gpio_pin_set_dt(&led3, 0);

        /* blink selected LED */
        (void)gpio_pin_toggle_dt(led);

        k_msleep(BLINK_MS);//on->wait->off->wait->on
    }
}

/* -------------------- ISR + callback -------------------- */
/*
 * ISR is intentionally minimal:
 * - do NOT debounce here
 * - do NOT modify shared state here
 * Just wake up the button task.
 */
static void button_cb_handler(const struct device *dev,
                              struct gpio_callback *cb,
                              uint32_t pins)
{
    ARG_UNUSED(dev);
    ARG_UNUSED(cb);
    ARG_UNUSED(pins);

    k_sem_give(&button_sem);
}

/* -------------------- button task -------------------- */
/*
 * Debounce is handled in task context:
 * - multiple ISR triggers within DEBOUNCE_MS are ignored
 * - accepted press switches to next LED (0->1->2->3->0)
 */
void button_task(void)
{
    while (1) {
        /* Wait for an interrupt event */
        k_sem_take(&button_sem, K_FOREVER);

        /* Task-level debounce */
        uint32_t now = k_uptime_get_32();
        if ((now - last_accepted_press_ms) < DEBOUNCE_MS) {
            continue;//immediately skips the rest of the current loop iteration and jumps to the next iteration.
        }
        last_accepted_press_ms = now;

        /* Switch to next LED (protected) */
        k_mutex_lock(&led_lock, K_FOREVER);
        current_led = (current_led + 1) & 0x03;//0x03 = 00000011
        k_mutex_unlock(&led_lock);

        /* Enforce: only one LED may blink; reset all to OFF */
        leds_all_off();

        /* Optional tiny delay: makes rapid bouncing less annoying */
        k_msleep(10);
    }
}

/* -------------------- main init -------------------- */
int main(void)
{
    int ret;

    /* Basic readiness checks */
    if (!gpio_is_ready_dt(&led0) || !gpio_is_ready_dt(&led1) ||
        !gpio_is_ready_dt(&led2) || !gpio_is_ready_dt(&led3)) {
        return 0;
    }
    if (!gpio_is_ready_dt(&button)) {
        return 0;
    }

    /* LEDs as outputs, start OFF */
    ret = gpio_pin_configure_dt(&led0, GPIO_OUTPUT_INACTIVE); if (ret < 0) return 0;
    ret = gpio_pin_configure_dt(&led1, GPIO_OUTPUT_INACTIVE); if (ret < 0) return 0;
    ret = gpio_pin_configure_dt(&led2, GPIO_OUTPUT_INACTIVE); if (ret < 0) return 0;
    ret = gpio_pin_configure_dt(&led3, GPIO_OUTPUT_INACTIVE); if (ret < 0) return 0;

    /* Button input */
    ret = gpio_pin_configure_dt(&button, GPIO_INPUT); if (ret < 0) return 0;

    /* Interrupt on press (active edge) */
    ret = gpio_pin_interrupt_configure_dt(&button, GPIO_INT_EDGE_TO_ACTIVE);
    if (ret) return 0;

    /* Callback registration */
    gpio_init_callback(&button_cb, button_cb_handler, BIT(button.pin));
    ret = gpio_add_callback(button.port, &button_cb);
    if (ret) return 0;

    /* Init semaphore (start at 0, max 1) */
    k_sem_init(&button_sem, 0, 1);

    /* Init mutex for shared state protection */
    k_mutex_init(&led_lock);

    /* Start state */
    leds_all_off();

    /* main thread does nothing */
    k_sleep(K_FOREVER);
    return 0;
}

/* -------------------- Threads -------------------- */
#define BLINKY_STACK_SIZE 1024
#define BUTTON_STACK_SIZE 1024

#define BLINKY_PRIORITY  5
#define BUTTON_PRIORITY  4

K_THREAD_DEFINE(blinky_tid,
                BLINKY_STACK_SIZE,
                blinky_task,
                NULL, NULL, NULL,
                BLINKY_PRIORITY,
                0,
                0);

K_THREAD_DEFINE(button_tid,
                BUTTON_STACK_SIZE,
                button_task,
                NULL, NULL, NULL,
                BUTTON_PRIORITY,
                0,
                0);
