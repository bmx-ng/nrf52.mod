#include <errno.h>
#include <stdint.h>

#include "blitzmax/embedded_i2c.h"
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/i2c.h>

#define I2C_SDA_PIN 36u /* P1.04, D8 */
#define I2C_SCL_PIN 38u /* P1.06, D9 */
#define I2C_ERROR (-1)
#define I2C_TIMEOUT (-2)
#define I2C_INVALID_ARGUMENT (-5)
#define I2C_DRIVER_TIMEOUT_US (CONFIG_I2C_TRANSFER_TIMEOUT_MS * 1000u)

static uint8_t pins_configured;
static uint8_t initialized;

static const struct device *i2c_device(void) {
    return DEVICE_DT_GET(DT_NODELABEL(i2c1));
}

static int valid_controller(int32_t controller) {
    return controller == 0 && device_is_ready(i2c_device());
}

static int enabled(int32_t controller) {
    return valid_controller(controller) && initialized;
}

static uint32_t speed_config(uint32_t baudrate) {
    if (baudrate == 100000u) return I2C_SPEED_SET(I2C_SPEED_STANDARD);
    if (baudrate == 400000u) return I2C_SPEED_SET(I2C_SPEED_FAST);
    return 0;
}

static int32_t result(int status, int32_t count) {
    if (status == 0) return count;
    return status == -ETIMEDOUT ? I2C_TIMEOUT : I2C_ERROR;
}

static int valid_transfer(int32_t controller, uint32_t address,
        const void *data, int32_t length) {
    return enabled(controller) && address < 128 && data &&
        length > 0 && length <= UINT16_MAX;
}

int32_t bmx_embedded_i2c_controller_count(void) { return 1; }
int32_t bmx_embedded_i2c_default_controller(void) { return 0; }
uint32_t bmx_embedded_i2c_default_sda_pin(void) { return I2C_SDA_PIN; }
uint32_t bmx_embedded_i2c_default_scl_pin(void) { return I2C_SCL_PIN; }

int32_t bmx_embedded_i2c_configure_pins(int32_t controller,
        uint32_t sda_pin, uint32_t scl_pin, int32_t pull_ups) {
    if (!valid_controller(controller) || initialized ||
            sda_pin != I2C_SDA_PIN || scl_pin != I2C_SCL_PIN || !pull_ups)
        return 0;
    pins_configured = 1;
    return 1;
}

uint32_t bmx_embedded_i2c_init(int32_t controller, uint32_t baudrate) {
    uint32_t speed = speed_config(baudrate);
    if (!valid_controller(controller) || !pins_configured || initialized ||
            !speed || i2c_configure(i2c_device(), I2C_MODE_CONTROLLER | speed) != 0)
        return 0;
    initialized = 1;
    return baudrate;
}

int32_t bmx_embedded_i2c_deinit(int32_t controller) {
    if (!enabled(controller)) return 0;
    initialized = 0;
    return 1;
}

uint32_t bmx_embedded_i2c_set_baudrate(int32_t controller, uint32_t baudrate) {
    uint32_t speed = speed_config(baudrate);
    if (!enabled(controller) || !speed ||
            i2c_configure(i2c_device(), I2C_MODE_CONTROLLER | speed) != 0)
        return 0;
    return baudrate;
}

int32_t bmx_embedded_i2c_write_blocking(int32_t controller,
        uint32_t address, void *data, int32_t length) {
    if (!valid_transfer(controller, address, data, length))
        return I2C_INVALID_ARGUMENT;
    return result(i2c_write(i2c_device(), data, (size_t)length,
        (uint16_t)address), length);
}

int32_t bmx_embedded_i2c_read_blocking(int32_t controller,
        uint32_t address, void *data, int32_t length) {
    if (!valid_transfer(controller, address, data, length))
        return I2C_INVALID_ARGUMENT;
    return result(i2c_read(i2c_device(), data, (size_t)length,
        (uint16_t)address), length);
}

int32_t bmx_embedded_i2c_write_timeout_us(int32_t controller,
        uint32_t address, void *data, int32_t length, uint32_t timeout_us) {
    if (timeout_us < I2C_DRIVER_TIMEOUT_US) return I2C_INVALID_ARGUMENT;
    return bmx_embedded_i2c_write_blocking(controller, address, data, length);
}

int32_t bmx_embedded_i2c_read_timeout_us(int32_t controller,
        uint32_t address, void *data, int32_t length, uint32_t timeout_us) {
    if (timeout_us < I2C_DRIVER_TIMEOUT_US) return I2C_INVALID_ARGUMENT;
    return bmx_embedded_i2c_read_blocking(controller, address, data, length);
}

int32_t bmx_embedded_i2c_write_read_blocking(int32_t controller,
        uint32_t address, void *write_data, int32_t write_length,
        void *read_data, int32_t read_length) {
    if (!valid_transfer(controller, address, write_data, write_length) ||
            !valid_transfer(controller, address, read_data, read_length))
        return I2C_INVALID_ARGUMENT;
    return result(i2c_write_read(i2c_device(), (uint16_t)address,
        write_data, (size_t)write_length, read_data, (size_t)read_length),
        read_length);
}

int32_t bmx_embedded_i2c_write_read_timeout_us(int32_t controller,
        uint32_t address, void *write_data, int32_t write_length,
        void *read_data, int32_t read_length, uint32_t timeout_us) {
    if (timeout_us < 2u * I2C_DRIVER_TIMEOUT_US) return I2C_INVALID_ARGUMENT;
    return bmx_embedded_i2c_write_read_blocking(controller, address,
        write_data, write_length, read_data, read_length);
}
