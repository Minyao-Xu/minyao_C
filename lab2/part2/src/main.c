#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>

#define DEBOUNCE_MS 50
#define BLINK_DELAY_MS 200

/* get LED specs from devicetree aliases */
static const struct gpio_dt_spec leds[] = {
	GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios),
	GPIO_DT_SPEC_GET(DT_ALIAS(led1), gpios),
	GPIO_DT_SPEC_GET(DT_ALIAS(led2), gpios),
	GPIO_DT_SPEC_GET(DT_ALIAS(led3), gpios),
};

static const struct gpio_dt_spec button = GPIO_DT_SPEC_GET(DT_ALIAS(sw0), gpios); // button specs
static struct gpio_callback button_cb;

// sync primitives
static struct k_sem btn_sem;
static struct k_mutex led_mutex;

static int current_led = 0; //shared variable current blinking LED
static uint32_t last_accepted_press_ms = 0; // for debounce

void blinky_task(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	while (1) {
		int idx;

		// read current_led atomically
		k_mutex_lock(&led_mutex, K_FOREVER);
		idx = current_led;
		k_mutex_unlock(&led_mutex);

		// blink selected LED only
		gpio_pin_toggle_dt(&leds[idx]);
		k_msleep(BLINK_DELAY_MS);
	}
}

/* ISR: deferred handling -> only signal the button thread */
static void button_isr(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(cb);
	ARG_UNUSED(pins);

	k_sem_give(&btn_sem); // signal the button task
}

void button_task(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	while (1) {
		k_sem_take(&btn_sem, K_FOREVER); // wait for button press signal

		// button debouncing 
        uint32_t now = k_uptime_get_32();
        if ((now - last_accepted_press_ms) < DEBOUNCE_MS) {
            continue;
        }
        last_accepted_press_ms = now;

		k_mutex_lock(&led_mutex, K_FOREVER);
		current_led = (current_led + 1) % 4;
		k_mutex_unlock(&led_mutex);
	}
}

#define STACK_SIZE 1024
#define PRIO_BLINK 5
#define PRIO_BTN   5

K_THREAD_DEFINE(blink_tid, STACK_SIZE, blinky_task, NULL, NULL, NULL, PRIO_BLINK, 0, 0);
K_THREAD_DEFINE(btn_tid,   STACK_SIZE, button_task, NULL, NULL, NULL, PRIO_BTN,   0, 0);

int main(void)
{
	// init semaphore and mutex
	k_sem_init(&btn_sem, 0, 1);
	k_mutex_init(&led_mutex);

	// configure LEDs
	for (int i = 0; i < 4; i++) {
		if (!gpio_is_ready_dt(&leds[i])) return 0;
		gpio_pin_configure_dt(&leds[i], GPIO_OUTPUT_INACTIVE);
	}

    // configure button with interrupt
	if (!gpio_is_ready_dt(&button)) return 0;
	gpio_pin_configure_dt(&button, GPIO_INPUT);
	gpio_pin_interrupt_configure_dt(&button, GPIO_INT_EDGE_TO_ACTIVE); //trigger on pin state change to logical level 1
	gpio_init_callback(&button_cb, button_isr, BIT(button.pin));
	gpio_add_callback(button.port, &button_cb);

	return 0;
}

