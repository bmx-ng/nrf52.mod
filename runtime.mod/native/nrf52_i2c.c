#include <errno.h>
#include <stdint.h>

#include "blitzmax/embedded_i2c.h"
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/dt-bindings/pinctrl/nrf-pinctrl.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/sys/util.h>

#define BMX_I2C_PIN(psel) (((psel) >> NRF_PIN_POS) & NRF_PIN_MSK)
#define BMX_I2C_FUN(psel) (((psel) >> NRF_FUN_POS) & NRF_FUN_MSK)
#define BMX_I2C_GROUP(label) DT_CHILD(DT_NODELABEL(label), group1)
#define BMX_I2C_PSEL(label, index) \
    DT_PROP_BY_IDX(BMX_I2C_GROUP(label), psels, index)
#define I2C_SDA_PIN BMX_I2C_PIN(BMX_I2C_PSEL(i2c1_default, 0))
#define I2C_SCL_PIN BMX_I2C_PIN(BMX_I2C_PSEL(i2c1_default, 1))
#define BMX_I2C_VALID_PIN(pin) ((pin) < 48u && (pin) != 18u && \
    (pin) != 24u && (pin) != 25u)
#define BMX_I2C_OTHER_BUS_PIN(pin) ( \
    (pin) == BMX_I2C_PIN(BMX_I2C_PSEL(uart0_default, 0)) || \
    (pin) == BMX_I2C_PIN(BMX_I2C_PSEL(uart0_default, 1)) || \
    (pin) == BMX_I2C_PIN(BMX_I2C_PSEL(uart0_default, 2)) || \
    (pin) == BMX_I2C_PIN(BMX_I2C_PSEL(uart0_default, 3)) || \
    (pin) == BMX_I2C_PIN(BMX_I2C_PSEL(spi2_default, 0)) || \
    (pin) == BMX_I2C_PIN(BMX_I2C_PSEL(spi2_default, 1)) || \
    (pin) == BMX_I2C_PIN(BMX_I2C_PSEL(spi2_default, 2)) || \
    (pin) == BMX_I2C_PIN(BMX_I2C_PSEL(pwm0_default, 0)) || \
    (pin) == BMX_I2C_PIN(BMX_I2C_PSEL(bmx_pwm1_default, 0)))

BUILD_ASSERT(DT_PROP_LEN(BMX_I2C_GROUP(i2c1_default), psels) == 2,
    "I2C1 needs SDA and SCL pin assignments");
BUILD_ASSERT(DT_PROP_LEN(BMX_I2C_GROUP(i2c1_sleep), psels) == 2,
    "I2C1 sleep routing must match the default routing");
BUILD_ASSERT(BMX_I2C_FUN(BMX_I2C_PSEL(i2c1_default, 0)) == NRF_FUN_TWIM_SDA &&
    BMX_I2C_FUN(BMX_I2C_PSEL(i2c1_default, 1)) == NRF_FUN_TWIM_SCL,
    "I2C1 pin functions must be ordered SDA, SCL");
BUILD_ASSERT(BMX_I2C_FUN(BMX_I2C_PSEL(i2c1_sleep, 0)) == NRF_FUN_TWIM_SDA &&
    BMX_I2C_FUN(BMX_I2C_PSEL(i2c1_sleep, 1)) == NRF_FUN_TWIM_SCL,
    "I2C1 sleep pin functions must be ordered SDA, SCL");
BUILD_ASSERT(I2C_SDA_PIN == BMX_I2C_PIN(BMX_I2C_PSEL(i2c1_sleep, 0)) &&
    I2C_SCL_PIN == BMX_I2C_PIN(BMX_I2C_PSEL(i2c1_sleep, 1)),
    "I2C1 default and sleep pin routes must agree");
BUILD_ASSERT(BMX_I2C_VALID_PIN(I2C_SDA_PIN) &&
    BMX_I2C_VALID_PIN(I2C_SCL_PIN) && I2C_SDA_PIN != I2C_SCL_PIN,
    "I2C1 needs distinct non-reserved pins");
BUILD_ASSERT(!BMX_I2C_OTHER_BUS_PIN(I2C_SDA_PIN) &&
    !BMX_I2C_OTHER_BUS_PIN(I2C_SCL_PIN),
    "I2C1 pins conflict with UART, SPI or PWM routing");
#define I2C_ERROR (-1)
#define I2C_TIMEOUT (-2)
#define I2C_INVALID_ARGUMENT (-5)

int bmx_nrf52_i2c_transfer_timeout(const struct device *dev,
        struct i2c_msg *msgs, uint8_t num_msgs, uint16_t addr,
        uint32_t timeout_us);

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
    if (!valid_transfer(controller, address, data, length))
        return I2C_INVALID_ARGUMENT;
    struct i2c_msg msg = {
        .buf = data, .len = (uint32_t)length,
        .flags = I2C_MSG_WRITE | I2C_MSG_STOP,
    };
    return result(bmx_nrf52_i2c_transfer_timeout(i2c_device(), &msg, 1,
        (uint16_t)address, timeout_us), length);
}

int32_t bmx_embedded_i2c_read_timeout_us(int32_t controller,
        uint32_t address, void *data, int32_t length, uint32_t timeout_us) {
    if (!valid_transfer(controller, address, data, length))
        return I2C_INVALID_ARGUMENT;
    struct i2c_msg msg = {
        .buf = data, .len = (uint32_t)length,
        .flags = I2C_MSG_READ | I2C_MSG_STOP,
    };
    return result(bmx_nrf52_i2c_transfer_timeout(i2c_device(), &msg, 1,
        (uint16_t)address, timeout_us), length);
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
    if (!valid_transfer(controller, address, write_data, write_length) ||
            !valid_transfer(controller, address, read_data, read_length))
        return I2C_INVALID_ARGUMENT;
    struct i2c_msg msgs[2] = {
        {.buf = write_data, .len = (uint32_t)write_length,
            .flags = I2C_MSG_WRITE},
        {.buf = read_data, .len = (uint32_t)read_length,
            .flags = I2C_MSG_RESTART | I2C_MSG_READ | I2C_MSG_STOP},
    };
    return result(bmx_nrf52_i2c_transfer_timeout(i2c_device(), msgs, 2,
        (uint16_t)address, timeout_us), read_length);
}
