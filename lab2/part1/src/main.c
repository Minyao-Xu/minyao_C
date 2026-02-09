#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>

/* get LED specs from devicetree aliases */
static const struct gpio_dt_spec led0 = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);
static const struct gpio_dt_spec led1 = GPIO_DT_SPEC_GET(DT_ALIAS(led1), gpios);
static const struct gpio_dt_spec led2 = GPIO_DT_SPEC_GET(DT_ALIAS(led2), gpios);
static const struct gpio_dt_spec led3 = GPIO_DT_SPEC_GET(DT_ALIAS(led3), gpios);

struct blinky_arg {
	const struct gpio_dt_spec *led;
	uint32_t delay_ms;
};

static void blinky_task(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	const struct blinky_arg *arg = p1;

	// sanity check
	if (!gpio_is_ready_dt(arg->led)) {
		return;
	}

	// configure LED pin as output
	int ret = gpio_pin_configure_dt(arg->led, GPIO_OUTPUT_INACTIVE);
	if (ret != 0) return;

	while (1) {
		gpio_pin_toggle_dt(arg->led);
		k_msleep(arg->delay_ms);
	}
}

#define STACK_SIZE 512
#define PRIORITY   5

static struct blinky_arg a0 = { &led0, 100 };
static struct blinky_arg a1 = { &led1, 200 };
static struct blinky_arg a2 = { &led2, 300 };
static struct blinky_arg a3 = { &led3, 500 };

// create threads with different args
K_THREAD_DEFINE(t0, STACK_SIZE, blinky_task, &a0, NULL, NULL, PRIORITY, 0, 0);
K_THREAD_DEFINE(t1, STACK_SIZE, blinky_task, &a1, NULL, NULL, PRIORITY, 0, 0);
K_THREAD_DEFINE(t2, STACK_SIZE, blinky_task, &a2, NULL, NULL, PRIORITY, 0, 0);
K_THREAD_DEFINE(t3, STACK_SIZE, blinky_task, &a3, NULL, NULL, PRIORITY, 0, 0);

int main(void)
{
	return 0;
}