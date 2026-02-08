#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>

#define TARGET_MS      3000U
#define DEBOUNCE_MS      50U

/* Button from devicetree alias sw0 */
static const struct gpio_dt_spec button = GPIO_DT_SPEC_GET(DT_ALIAS(sw0), gpios);

static struct gpio_callback button_cb;
static struct k_sem button_sem;

static uint32_t last_accepted_ms;

enum game_state {
    WAIT_FIRST_PRESS = 0,
    WAIT_SECOND_PRESS
};

static enum game_state state;
static uint32_t t_start_ms;

static void button_cb_handler(const struct device *dev,
                              struct gpio_callback *cb,
                              uint32_t pins)
{
    ARG_UNUSED(dev);
    ARG_UNUSED(cb);
    ARG_UNUSED(pins);

    /* ISR: only wake up the main loop */
    k_sem_give(&button_sem);
}

static void print_intro(void)
{
    printk("\n=== Three-second game ===\n");
    printk("Button: GP20\n");
    printk("Press once to START.\n");
    printk("Press again after exactly 3 seconds.\n");
}

static void print_result(uint32_t elapsed_ms)
{
    int32_t error_ms = (int32_t)elapsed_ms - (int32_t)TARGET_MS;
    uint32_t abs_error = (error_ms >= 0) ? (uint32_t)error_ms : (uint32_t)(-error_ms);

    printk("Your time: %u ms\n", elapsed_ms);

    if (error_ms == 0) {
        printk("Perfect! Exactly 3000 ms.\n");
    } else if (error_ms > 0) {
        printk("Too late by %u ms.\n", abs_error);
    } else {
        printk("Too early by %u ms.\n", abs_error);
    }
}

int main(void)
{
    int ret;

    if (!gpio_is_ready_dt(&button)) {
        printk("ERROR: button device not ready (check overlay sw0 / GP20)\n");
        return 0;
    }

    /* Configure button as input (pull-up is handled by DT flags) */
    ret = gpio_pin_configure_dt(&button, GPIO_INPUT);
    if (ret < 0) {
        printk("ERROR: gpio_pin_configure_dt failed: %d\n", ret);
        return 0;
    }

    /* Interrupt on active edge (for ACTIVE_LOW, this is falling edge) */
    ret = gpio_pin_interrupt_configure_dt(&button, GPIO_INT_EDGE_TO_ACTIVE);
    if (ret < 0) {
        printk("ERROR: gpio_pin_interrupt_configure_dt failed: %d\n", ret);
        return 0;
    }
     /* 
     Init callback struct
     Register callback to GPIO controller device
     */

    gpio_init_callback(&button_cb, button_cb_handler, BIT(button.pin));
    ret = gpio_add_callback(button.port, &button_cb);
    if (ret < 0) {
        printk("ERROR: gpio_add_callback failed: %d\n", ret);
        return 0;
    }

    k_sem_init(&button_sem, 0, 1);

    last_accepted_ms = 0;
    state = WAIT_FIRST_PRESS;
    t_start_ms = 0;

    print_intro();

    while (1) {
        /* Wait for a press event */
        k_sem_take(&button_sem, K_FOREVER);

        /* Debounce in task context */
        uint32_t now = k_uptime_get_32();
        if ((now - last_accepted_ms) < DEBOUNCE_MS) {
            continue;
        }
        last_accepted_ms = now;

        if (state == WAIT_FIRST_PRESS) {
            t_start_ms = now;
            state = WAIT_SECOND_PRESS;
            printk("Start! Try to press again after 3 seconds...\n");
        } else {
            uint32_t elapsed = now - t_start_ms;
            print_result(elapsed);

            /* Reset for next round */
            state = WAIT_FIRST_PRESS;
            printk("\nPress once to START a new round.\n");
        }
    }

    return 0;
}
