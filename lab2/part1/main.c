#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/devicetree.h>
#include <zephyr/sys/printk.h>

/*
 * Part 1 requirement:
 * - 4 LEDs blink concurrently
 * - one thread per LED
 * - different periods: 100/200/300/500 ms
 * - LED pins come from devicetree aliases: led0..led3
 Note:
 * - This program toggles the LED every delay_ms.
 *   Full ON+OFF blink cycle is 2*delay_ms.
 *   (每 delay_ms 翻转一次，完整亮灭周期是 2*delay_ms)
 */
 */

/* Make build errors easier to understand if aliases are missing */
#if !DT_NODE_HAS_STATUS(DT_ALIAS(led0), okay)
#error "Devicetree alias 'led0' is missing or disabled. Check your overlay."
#endif
#if !DT_NODE_HAS_STATUS(DT_ALIAS(led1), okay)
#error "Devicetree alias 'led1' is missing or disabled. Check your overlay."
#endif
#if !DT_NODE_HAS_STATUS(DT_ALIAS(led2), okay)
#error "Devicetree alias 'led2' is missing or disabled. Check your overlay."
#endif
#if !DT_NODE_HAS_STATUS(DT_ALIAS(led3), okay)
#error "Devicetree alias 'led3' is missing or disabled. Check your overlay."
#endif

/* Read LED GPIOs from devicetree aliases led0..led3 */
static const struct gpio_dt_spec led0 = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);
static const struct gpio_dt_spec led1 = GPIO_DT_SPEC_GET(DT_ALIAS(led1), gpios);
static const struct gpio_dt_spec led2 = GPIO_DT_SPEC_GET(DT_ALIAS(led2), gpios);
static const struct gpio_dt_spec led3 = GPIO_DT_SPEC_GET(DT_ALIAS(led3), gpios);

/* Thread config */
#define STACK_SIZE 1024
#define PRIORITY   5

K_THREAD_STACK_DEFINE(stack0, STACK_SIZE);
K_THREAD_STACK_DEFINE(stack1, STACK_SIZE);
K_THREAD_STACK_DEFINE(stack2, STACK_SIZE);
K_THREAD_STACK_DEFINE(stack3, STACK_SIZE);

static struct k_thread t0, t1, t2, t3;

struct blink_cfg {
    const struct gpio_dt_spec *led; /*Which LED this thread controls. (该线程控制哪个 LED) */
	uint32_t delay_ms;              /*Toggle interval in ms. (翻转间隔，单位 ms) */
	const char *name;               /*For debug prints. (用于调试输出的名字) */
};

static void blinky_task(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	const struct blink_cfg *cfg = (const struct blink_cfg *)p1;

	if (cfg == NULL || cfg->led == NULL) {
		printk("blinky: invalid config pointer\n");
		return;
	}
	/*
	 *  gpio_is_ready_dt() checks whether the GPIO controller device is ready.
	 *  If this fails, devicetree or board config is likely wrong.
	 *
	 */
	if (!gpio_is_ready_dt(cfg->led)) {
		printk("%s: GPIO device not ready (check overlay/board)\n", cfg->name);
		return;
	}

	/* Configure LED pin as output, initial OFF */
	int ret = gpio_pin_configure_dt(cfg->led, GPIO_OUTPUT_INACTIVE);
	if (ret < 0) {
		printk("%s: gpio_pin_configure_dt() failed: %d\n", cfg->name, ret);
		return;
	}

	/* Toggle every delay_ms. That means full ON+OFF cycle is 2*delay_ms. */
	while (1) {
		ret = gpio_pin_toggle_dt(cfg->led);
		if (ret < 0) {
			printk("%s: gpio_pin_toggle_dt() failed: %d\n", cfg->name, ret);
			/* If toggle fails once, retry later instead of busy looping */
		}
		k_msleep(cfg->delay_ms);
	}
}

int main(void)
{
	static const struct blink_cfg cfg0 = { .led = &led0, .delay_ms = 100, .name = "led0" };
	static const struct blink_cfg cfg1 = { .led = &led1, .delay_ms = 200, .name = "led1" };
	static const struct blink_cfg cfg2 = { .led = &led2, .delay_ms = 300, .name = "led2" };
	static const struct blink_cfg cfg3 = { .led = &led3, .delay_ms = 500, .name = "led3" };

	k_tid_t id0 = k_thread_create(&t0, stack0, STACK_SIZE, blinky_task,
				      (void *)&cfg0, NULL, NULL,
				      PRIORITY, 0, K_NO_WAIT);//Start the thread immediately.
	k_thread_name_set(id0, "blink_led0");

	k_tid_t id1 = k_thread_create(&t1, stack1, STACK_SIZE, blinky_task,
				      (void *)&cfg1, NULL, NULL,
				      PRIORITY, 0, K_NO_WAIT);
	k_thread_name_set(id1, "blink_led1");

	k_tid_t id2 = k_thread_create(&t2, stack2, STACK_SIZE, blinky_task,
				      (void *)&cfg2, NULL, NULL,
				      PRIORITY, 0, K_NO_WAIT);
	k_thread_name_set(id2, "blink_led2");

	k_tid_t id3 = k_thread_create(&t3, stack3, STACK_SIZE, blinky_task,
				      (void *)&cfg3, NULL, NULL,
				      PRIORITY, 0, K_NO_WAIT);
	k_thread_name_set(id3, "blink_led3");

	/*
	 * Nothing else to do in main.
	 * The blinking is handled by worker threads.
	 */
	k_sleep(K_FOREVER);//让 main 线程挂起，把 CPU 完全交给其他线程
	return 0;
}
