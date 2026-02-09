#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <stdio.h>

static const struct gpio_dt_spec button = GPIO_DT_SPEC_GET(DT_ALIAS(sw0), gpios);
static struct gpio_callback button_cb;

static struct k_sem btn_sem;

/* ISR: deferred handling -> only signal */
static void button_isr(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(cb);
	ARG_UNUSED(pins);

	k_sem_give(&btn_sem); // signal the button task
}

// wait for button press with debounce
static void wait_for_press(void)
{
	k_sem_take(&btn_sem, K_FOREVER); // wait for button press signal
	k_msleep(50);

    while(k_sem_take(&btn_sem, K_NO_WAIT) == 0) {
        // flush any additional presses during debounce period
    }
}

int main(void)
{
	k_sem_init(&btn_sem, 0, 1);

    // configure button with interrupt
	if (!gpio_is_ready_dt(&button)) return 0;
	gpio_pin_configure_dt(&button, GPIO_INPUT);
	gpio_pin_interrupt_configure_dt(&button, GPIO_INT_EDGE_TO_ACTIVE);
	gpio_init_callback(&button_cb, button_isr, BIT(button.pin));
	gpio_add_callback(button.port, &button_cb);

	while (1) {
		printf("\n--- Three second game ---\n");
		printf("Press the button to start the game\n");
		fflush(stdout);

		wait_for_press();
		int64_t t0 = k_uptime_get(); // record start time

		printf("Game Started! Press again after exactly 3.000 seconds...\n");
		fflush(stdout);

		wait_for_press();
		int64_t t1 = k_uptime_get(); // record end time

        // calculate and display results
		int64_t elapsed = t1 - t0;
		int64_t target  = 3000;
		int64_t error   = elapsed - target;
		int64_t abs_err = (error < 0) ? -error : error;
		printf("Your time: %lld ms\n", elapsed);
		printf("Error: %+lld ms (abs %lld ms)\n", error, abs_err);

		// result feedback
		if (abs_err <= 50) {
			printf("Well Done! Very close\n");
		} else if (abs_err <= 150) {
			printf("Close enough\n");
		} else {
			printf("Yikes! Not close\n");
		}
		fflush(stdout);
	}
}
