#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/sys/printk.h>

#include "bme680_reg.h"

#define I2C_NODE DT_NODELABEL(i2c0)
#define BME680_ADDR 0x77

/* Temperature calibration registers (BME680) */
#define DIG_T1_LSB 0xE9
#define DIG_T2_LSB 0x8A
#define DIG_T3     0x8C

/* BME680 forced-mode, temp oversampling x1:
   ctrl_meas: osrs_t[7:5]=001, mode[1:0]=01 -> (1<<5) | 0x01
*/
#define CTRL_MEAS_TEMP_X1_FORCED ((1u << 5) | 0x01)

static inline int rd8(const struct device *i2c, uint8_t reg, uint8_t *val)
{
    return i2c_reg_read_byte(i2c, BME680_ADDR, reg, val);
}

static inline int rdN(const struct device *i2c, uint8_t start_reg, uint8_t *buf, size_t len)
{
    return i2c_burst_read(i2c, BME680_ADDR, start_reg, buf, len);
}

static inline int wr8(const struct device *i2c, uint8_t reg, uint8_t val)
{
    return i2c_reg_write_byte(i2c, BME680_ADDR, reg, val);
}

/* Returns temperature in 0.01 C (Bosch compensation) */
static int32_t temp_01C(int32_t adc_T, uint16_t T1, int16_t T2, int8_t T3)
{
    int32_t v1 = ((((adc_T >> 3) - ((int32_t)T1 << 1))) * (int32_t)T2) >> 11;
    int32_t v2 = (((((adc_T >> 4) - (int32_t)T1) * ((adc_T >> 4) - (int32_t)T1)) >> 12) * (int32_t)T3) >> 14;
    int32_t tf = v1 + v2;
    return (tf * 5 + 128) >> 8;
}

int main(void)
{
    const struct device *i2c = DEVICE_DT_GET(I2C_NODE);
    if (!device_is_ready(i2c)) {
        printk("i2c0 not ready\n");
        return -1;
    }

    /* Read temperature calibration parameters */
    uint8_t b[2];
    uint16_t T1; int16_t T2; int8_t T3;

    if (rdN(i2c, DIG_T1_LSB, b, 2) != 0) return -1;
    T1 = (uint16_t)(b[0] | (b[1] << 8));

    if (rdN(i2c, DIG_T2_LSB, b, 2) != 0) return -1;
    T2 = (int16_t)(b[0] | (b[1] << 8));

    if (rd8(i2c, DIG_T3, (uint8_t *)&T3) != 0) return -1;

    /* Minimal config: humidity oversampling = 0 */
    (void)wr8(i2c, BME680_CTRL_HUM, 0x00);

    while (1) {
        /* Trigger one measurement (forced mode) */
        (void)wr8(i2c, BME680_CTRL_MEAS, CTRL_MEAS_TEMP_X1_FORCED);
        k_msleep(200);

        /* Read raw temperature (20-bit) */
        uint8_t t[3];
        if (rdN(i2c, BME680_TEMP_MSB, t, 3) != 0) {
            k_sleep(K_SECONDS(3));
            continue;
        }
        /* t[0] : T[19:12]
           t[1] : T[11:4]
           t[2] : T[3:0] 
        */
        int32_t adc_T = ((int32_t)t[0] << 12) | ((int32_t)t[1] << 4) | ((int32_t)t[2] >> 4);
        int32_t t01 = temp_01C(adc_T, T1, T2, T3);

        printk("Temperature: %d.%02d C\n", (int)(t01 / 100), (int)(t01 % 100));
        k_sleep(K_SECONDS(3));
    }
}
