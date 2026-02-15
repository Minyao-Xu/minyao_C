#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/sys/printk.h>

#define BME_NODE DT_NODELABEL(bme680)

int main(void)
{
    const struct device *dev = DEVICE_DT_GET(BME_NODE);

    if (!device_is_ready(dev)) {
        printk("BME680 device not ready\n");
        return -1;
    }

    while (1) {
        /* 1) Ask driver to fetch a new sample (driver performs I2C ops + compensation internally) */
        if (sensor_sample_fetch(dev) < 0) {
            printk("sensor_sample_fetch failed\n");
            k_sleep(K_SECONDS(3));
            continue;
        }

        /* 2) Get the temperature channel */
        struct sensor_value temp;
        if (sensor_channel_get(dev, SENSOR_CHAN_AMBIENT_TEMP, &temp) < 0) {
            printk("sensor_channel_get failed\n");
            k_sleep(K_SECONDS(3));
            continue;
        }

        /* sensor_value: val1 is integer part, val2 is fractional part in 1e-6 */
        printk("Temperature: %d.%06d C\n", temp.val1, temp.val2);

        k_sleep(K_SECONDS(3));
    }
}
