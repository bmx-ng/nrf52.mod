#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>

/* Test only: jumper D19/P0.02 to the default I2C SCL pad D9/P1.06. */
#define HOLD_PIN 2

static const struct device *gpio0 = DEVICE_DT_GET(DT_NODELABEL(gpio0));

static void release_clock(struct k_work *work)
{
    (void)work;
    (void)gpio_pin_configure(gpio0, HOLD_PIN, GPIO_INPUT);
}

K_WORK_DELAYABLE_DEFINE(release_work, release_clock);

int bmx_nrf52_i2c_test_hold_clock(uint32_t milliseconds)
{
    if (!device_is_ready(gpio0) ||
            gpio_pin_configure(gpio0, HOLD_PIN, GPIO_OUTPUT_LOW) != 0)
        return 0;
    return k_work_reschedule(&release_work, K_MSEC(milliseconds)) >= 0;
}
