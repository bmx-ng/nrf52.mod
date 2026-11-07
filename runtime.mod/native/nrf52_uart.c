#include <stddef.h>
#include <stdint.h>

#include "blitzmax/embedded_uart.h"
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/dt-bindings/pinctrl/nrf-pinctrl.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/irq.h>
#include <zephyr/kernel.h>
#include <zephyr/pm/device.h>
#include <zephyr/sys/util.h>

#define BMX_UART_PIN(psel) (((psel) >> NRF_PIN_POS) & NRF_PIN_MSK)
#define BMX_UART_FUN(psel) (((psel) >> NRF_FUN_POS) & NRF_FUN_MSK)
#define BMX_UART_GROUP(label) DT_CHILD(DT_NODELABEL(label), group1)
#define BMX_UART_PSEL(label, index) \
    DT_PROP_BY_IDX(BMX_UART_GROUP(label), psels, index)
#define UART_TX_PIN BMX_UART_PIN(BMX_UART_PSEL(uart0_default, 0))
#define UART_RX_PIN BMX_UART_PIN(BMX_UART_PSEL(uart0_default, 1))
#define UART_RTS_PIN BMX_UART_PIN(BMX_UART_PSEL(uart0_default, 2))
#define UART_CTS_PIN BMX_UART_PIN(BMX_UART_PSEL(uart0_default, 3))
#define BMX_UART_VALID_PIN(pin) ((pin) < 48u && (pin) != 18u && \
    (pin) != 24u && (pin) != 25u)
#define BMX_UART_OTHER_BUS_PIN(pin) ( \
    (pin) == BMX_UART_PIN(BMX_UART_PSEL(spi2_default, 0)) || \
    (pin) == BMX_UART_PIN(BMX_UART_PSEL(spi2_default, 1)) || \
    (pin) == BMX_UART_PIN(BMX_UART_PSEL(spi2_default, 2)) || \
    (pin) == BMX_UART_PIN(BMX_UART_PSEL(i2c1_default, 0)) || \
    (pin) == BMX_UART_PIN(BMX_UART_PSEL(i2c1_default, 1)) || \
    (pin) == BMX_UART_PIN(BMX_UART_PSEL(pwm0_default, 0)) || \
    (pin) == BMX_UART_PIN(BMX_UART_PSEL(bmx_pwm1_default, 0)))

BUILD_ASSERT(DT_PROP_LEN(BMX_UART_GROUP(uart0_default), psels) == 4,
    "UART0 needs TX, RX, RTS and CTS pin assignments");
BUILD_ASSERT(DT_PROP_LEN(BMX_UART_GROUP(uart0_sleep), psels) == 4,
    "UART0 sleep routing must match the default routing");
BUILD_ASSERT(BMX_UART_FUN(BMX_UART_PSEL(uart0_default, 0)) == NRF_FUN_UART_TX &&
    BMX_UART_FUN(BMX_UART_PSEL(uart0_default, 1)) == NRF_FUN_UART_RX &&
    BMX_UART_FUN(BMX_UART_PSEL(uart0_default, 2)) == NRF_FUN_UART_RTS &&
    BMX_UART_FUN(BMX_UART_PSEL(uart0_default, 3)) == NRF_FUN_UART_CTS,
    "UART0 pin functions must be ordered TX, RX, RTS, CTS");
BUILD_ASSERT(BMX_UART_FUN(BMX_UART_PSEL(uart0_sleep, 0)) == NRF_FUN_UART_TX &&
    BMX_UART_FUN(BMX_UART_PSEL(uart0_sleep, 1)) == NRF_FUN_UART_RX &&
    BMX_UART_FUN(BMX_UART_PSEL(uart0_sleep, 2)) == NRF_FUN_UART_RTS &&
    BMX_UART_FUN(BMX_UART_PSEL(uart0_sleep, 3)) == NRF_FUN_UART_CTS,
    "UART0 sleep pin functions must be ordered TX, RX, RTS, CTS");
BUILD_ASSERT(UART_TX_PIN == BMX_UART_PIN(BMX_UART_PSEL(uart0_sleep, 0)) &&
    UART_RX_PIN == BMX_UART_PIN(BMX_UART_PSEL(uart0_sleep, 1)) &&
    UART_RTS_PIN == BMX_UART_PIN(BMX_UART_PSEL(uart0_sleep, 2)) &&
    UART_CTS_PIN == BMX_UART_PIN(BMX_UART_PSEL(uart0_sleep, 3)),
    "UART0 default and sleep pin routes must agree");
BUILD_ASSERT(BMX_UART_VALID_PIN(UART_TX_PIN) &&
    BMX_UART_VALID_PIN(UART_RX_PIN) && BMX_UART_VALID_PIN(UART_RTS_PIN) &&
    BMX_UART_VALID_PIN(UART_CTS_PIN),
    "UART0 cannot use reset, USB or out-of-range pins");
BUILD_ASSERT(UART_TX_PIN != UART_RX_PIN && UART_TX_PIN != UART_RTS_PIN &&
    UART_TX_PIN != UART_CTS_PIN && UART_RX_PIN != UART_RTS_PIN &&
    UART_RX_PIN != UART_CTS_PIN && UART_RTS_PIN != UART_CTS_PIN,
    "UART0 pins must be distinct");
BUILD_ASSERT(!BMX_UART_OTHER_BUS_PIN(UART_TX_PIN) &&
    !BMX_UART_OTHER_BUS_PIN(UART_RX_PIN) &&
    !BMX_UART_OTHER_BUS_PIN(UART_RTS_PIN) &&
    !BMX_UART_OTHER_BUS_PIN(UART_CTS_PIN),
    "UART0 pins conflict with SPI, I2C or PWM routing");
#define UART_INVALID_ARGUMENT (-5)
#define POLL_RX_DMA_SIZE 64u
#define POLL_RX_RING_SIZE 256u

static uint8_t pins_configured;
static uint8_t flow_pins_configured;
static uint8_t initialized;
static uint8_t break_active;
static uint8_t translate_crlf;
static uint8_t pending_valid;
static uint8_t pending_byte;
static volatile uint32_t errors;
static uint8_t poll_rx_dma[2][POLL_RX_DMA_SIZE];
static uint8_t poll_rx_dma_busy[2];
static uint8_t poll_rx_ring[POLL_RX_RING_SIZE];
static volatile uint32_t poll_rx_put;
static volatile uint32_t poll_rx_get;
static volatile uint8_t poll_rx_active;
static volatile uint8_t poll_rx_disabled;
static volatile uint8_t poll_rx_ready;

static const struct device *uart_device(void) {
    return DEVICE_DT_GET(DT_NODELABEL(uart0));
}

static const struct device *tx_gpio_device(void) {
    return UART_TX_PIN < 32u ? DEVICE_DT_GET(DT_NODELABEL(gpio0))
        : DEVICE_DT_GET(DT_NODELABEL(gpio1));
}

static int valid_controller(int32_t controller) {
    return controller == 0 && device_is_ready(uart_device());
}

static int enabled(int32_t controller) {
    return valid_controller(controller) && initialized;
}

static uint64_t now_us(void) {
    return k_ticks_to_us_floor64(k_uptime_ticks());
}

static void poll_callback(const struct device *device, struct uart_event *event,
        void *user_data) {
    (void)user_data;
    switch (event->type) {
    case UART_RX_RDY: {
        unsigned int key = irq_lock();
        for (size_t i = 0; i < event->data.rx.len; ++i) {
            if (poll_rx_put - poll_rx_get >= POLL_RX_RING_SIZE) {
                errors |= 8u;
            } else {
                poll_rx_ring[poll_rx_put++ & (POLL_RX_RING_SIZE - 1u)] =
                    event->data.rx.buf[event->data.rx.offset + i];
            }
        }
        irq_unlock(key);
        break;
    }
    case UART_RX_BUF_REQUEST:
        poll_rx_ready = 1;
        for (int i = 0; i < 2; ++i) {
            if (!poll_rx_dma_busy[i]) {
                poll_rx_dma_busy[i] = 1;
                if (uart_rx_buf_rsp(device, poll_rx_dma[i], POLL_RX_DMA_SIZE) != 0)
                    poll_rx_dma_busy[i] = 0;
                break;
            }
        }
        break;
    case UART_RX_BUF_RELEASED:
        for (int i = 0; i < 2; ++i)
            if (event->data.rx_buf.buf == poll_rx_dma[i])
                poll_rx_dma_busy[i] = 0;
        break;
    case UART_RX_DISABLED:
        poll_rx_disabled = 1;
        if (poll_rx_active) {
            poll_rx_ready = 0;
            for (int i = 0; i < 2; ++i) {
                if (!poll_rx_dma_busy[i]) {
                    poll_rx_dma_busy[i] = 1;
                    if (uart_rx_enable(device, poll_rx_dma[i],
                            POLL_RX_DMA_SIZE, 1000) == 0)
                        poll_rx_disabled = 0;
                    else
                        poll_rx_dma_busy[i] = 0;
                    break;
                }
            }
        }
        break;
    case UART_RX_STOPPED:
        if (event->data.rx_stop.reason & UART_ERROR_FRAMING) errors |= 1u;
        if (event->data.rx_stop.reason & UART_ERROR_PARITY) errors |= 2u;
        if (event->data.rx_stop.reason & UART_BREAK) errors |= 4u;
        if (event->data.rx_stop.reason & UART_ERROR_OVERRUN) errors |= 8u;
        break;
    default:
        break;
    }
}

int32_t bmx_nrf52_uart_poll_suspend(void) {
    if (!poll_rx_active) return 1;
    poll_rx_active = 0;
    poll_rx_disabled = 0;
    if (uart_rx_disable(uart_device()) != 0) {
        poll_rx_active = 1;
        return 0;
    }
    uint64_t deadline = now_us() + 100000u;
    while (!poll_rx_disabled && now_us() < deadline) k_usleep(100);
    return poll_rx_disabled;
}

int32_t bmx_nrf52_uart_poll_resume(void) {
    if (poll_rx_active) return 1;
    poll_rx_put = 0;
    poll_rx_get = 0;
    poll_rx_dma_busy[0] = 1;
    poll_rx_dma_busy[1] = 0;
    poll_rx_disabled = 0;
    poll_rx_ready = 0;
    if (uart_callback_set(uart_device(), poll_callback, NULL) != 0) return 0;
    poll_rx_active = 1;
    if (uart_rx_enable(uart_device(), poll_rx_dma[0], POLL_RX_DMA_SIZE,
            1000) != 0) {
        poll_rx_active = 0;
        poll_rx_dma_busy[0] = 0;
        return 0;
    }
    uint64_t deadline = now_us() + 10000u;
    while (!poll_rx_ready && now_us() < deadline) k_usleep(50);
    if (poll_rx_ready) return 1;
    (void)bmx_nrf52_uart_poll_suspend();
    return 0;
}

static int poll_pop(uint8_t *byte) {
    unsigned int key = irq_lock();
    if (poll_rx_get == poll_rx_put) {
        irq_unlock(key);
        return 0;
    }
    *byte = poll_rx_ring[poll_rx_get++ & (POLL_RX_RING_SIZE - 1u)];
    irq_unlock(key);
    return 1;
}

static int try_read(uint8_t *byte) {
    if (pending_valid) {
        *byte = pending_byte;
        pending_valid = 0;
        return 1;
    }
    return poll_pop(byte);
}

static int32_t read_timeout(int32_t controller, void *destination,
        int32_t length, uint32_t timeout_us) {
    if (!enabled(controller) || length < 0 || (length && !destination))
        return UART_INVALID_ARGUMENT;
    uint8_t *bytes = destination;
    uint64_t deadline = now_us() + timeout_us;
    int32_t count = 0;
    while (count < length) {
        if (try_read(&bytes[count])) {
            ++count;
        } else if (now_us() >= deadline) {
            break;
        } else {
            k_usleep(50);
        }
    }
    return count;
}

int32_t bmx_embedded_uart_controller_count(void) { return 1; }
int32_t bmx_embedded_uart_default_controller(void) { return -1; }
uint32_t bmx_embedded_uart_default_baudrate(void) { return 0; }

int32_t bmx_embedded_uart_configure_pins(int32_t controller, uint32_t tx_pin,
        uint32_t rx_pin) {
    if (!valid_controller(controller) || initialized ||
            tx_pin != UART_TX_PIN || rx_pin != UART_RX_PIN) return 0;
    pins_configured = 1;
    return 1;
}

int32_t bmx_embedded_uart_configure_flow_pins(int32_t controller,
        uint32_t cts_pin, uint32_t rts_pin) {
    if (!valid_controller(controller) || initialized ||
            cts_pin != UART_CTS_PIN || rts_pin != UART_RTS_PIN) return 0;
    flow_pins_configured = 1;
    return 1;
}

uint32_t bmx_embedded_uart_init(int32_t controller, uint32_t baudrate) {
    if (!valid_controller(controller) || !pins_configured || initialized || !baudrate)
        return 0;
    struct uart_config config = {
        .baudrate = baudrate,
        .parity = UART_CFG_PARITY_NONE,
        .stop_bits = UART_CFG_STOP_BITS_1,
        .data_bits = UART_CFG_DATA_BITS_8,
        .flow_ctrl = UART_CFG_FLOW_CTRL_NONE,
    };
    if (uart_configure(uart_device(), &config) != 0) return 0;
    if (!bmx_nrf52_uart_poll_resume()) return 0;
    initialized = 1;
    break_active = 0;
    translate_crlf = 0;
    pending_valid = 0;
    errors = 0;
    return baudrate;
}

int32_t bmx_embedded_uart_deinit(int32_t controller) {
    if (!enabled(controller)) return 0;
    if (break_active && !bmx_embedded_uart_set_break(controller, 0)) return 0;
    if (!bmx_nrf52_uart_poll_suspend()) return 0;
    initialized = 0;
    pending_valid = 0;
    return 1;
}

uint32_t bmx_embedded_uart_set_baudrate(int32_t controller, uint32_t baudrate) {
    if (!enabled(controller) || break_active || !baudrate) return 0;
    struct uart_config config;
    if (uart_config_get(uart_device(), &config) != 0) return 0;
    config.baudrate = baudrate;
    if (uart_configure(uart_device(), &config) != 0) return 0;
    return baudrate;
}

int32_t bmx_embedded_uart_set_format(int32_t controller, uint32_t data_bits,
        uint32_t stop_bits, uint32_t parity) {
    if (!enabled(controller) || break_active || data_bits != 8 || stop_bits != 1 || parity > 1)
        return 0;
    struct uart_config config;
    if (uart_config_get(uart_device(), &config) != 0) return 0;
    config.data_bits = UART_CFG_DATA_BITS_8;
    config.stop_bits = UART_CFG_STOP_BITS_1;
    config.parity = parity == 1 ? UART_CFG_PARITY_EVEN : UART_CFG_PARITY_NONE;
    return uart_configure(uart_device(), &config) == 0;
}

int32_t bmx_embedded_uart_set_flow_control(int32_t controller,
        int32_t cts, int32_t rts) {
    if (!enabled(controller) || break_active || (!!cts != !!rts) ||
            (cts && !flow_pins_configured)) return 0;
    struct uart_config config;
    if (uart_config_get(uart_device(), &config) != 0) return 0;
    config.flow_ctrl = cts ? UART_CFG_FLOW_CTRL_RTS_CTS : UART_CFG_FLOW_CTRL_NONE;
    if (!bmx_nrf52_uart_poll_suspend()) return 0;
    int configured = uart_configure(uart_device(), &config) == 0;
    int resumed = bmx_nrf52_uart_poll_resume();
    return configured && resumed;
}

int32_t bmx_embedded_uart_is_enabled(int32_t controller) {
    return enabled(controller);
}

int32_t bmx_embedded_uart_is_writable(int32_t controller) {
    return enabled(controller);
}

int32_t bmx_embedded_uart_is_readable(int32_t controller) {
    if (!enabled(controller)) return 0;
    if (pending_valid) return 1;
    uint8_t value;
    if (!poll_pop(&value)) return 0;
    pending_byte = value;
    pending_valid = 1;
    return 1;
}

int32_t bmx_embedded_uart_is_readable_within_us(int32_t controller,
        uint32_t timeout_us) {
    if (!enabled(controller)) return 0;
    uint64_t deadline = now_us() + timeout_us;
    do {
        if (bmx_embedded_uart_is_readable(controller)) return 1;
        if (now_us() >= deadline) return 0;
        k_usleep(50);
    } while (1);
}

int32_t bmx_embedded_uart_write_blocking(int32_t controller,
        const void *source, int32_t length) {
    if (!enabled(controller) || break_active || length < 0 || (length && !source))
        return UART_INVALID_ARGUMENT;
    const uint8_t *bytes = source;
    for (int32_t i = 0; i < length; ++i) {
        if (translate_crlf && bytes[i] == '\n') uart_poll_out(uart_device(), '\r');
        uart_poll_out(uart_device(), bytes[i]);
    }
    return length;
}

int32_t bmx_embedded_uart_read_blocking(int32_t controller,
        void *destination, int32_t length) {
    if (!enabled(controller) || length < 0 || (length && !destination))
        return UART_INVALID_ARGUMENT;
    uint8_t *bytes = destination;
    for (int32_t i = 0; i < length;) {
        if (try_read(&bytes[i])) ++i;
        else k_usleep(50);
    }
    return length;
}

int32_t bmx_embedded_uart_read_timeout_us(int32_t controller,
        void *destination, int32_t length, uint32_t timeout_us) {
    return read_timeout(controller, destination, length, timeout_us);
}

int32_t bmx_embedded_uart_read_available(int32_t controller,
        void *destination, int32_t capacity) {
    if (!enabled(controller) || capacity < 0 || (capacity && !destination))
        return UART_INVALID_ARGUMENT;
    uint8_t *bytes = destination;
    int32_t count = 0;
    while (count < capacity && try_read(&bytes[count])) ++count;
    return count;
}

int32_t bmx_embedded_uart_put_byte(int32_t controller, uint32_t value) {
    if (!enabled(controller) || value > 255) return UART_INVALID_ARGUMENT;
    uint8_t byte = value;
    return bmx_embedded_uart_write_blocking(controller, &byte, 1);
}

int32_t bmx_embedded_uart_tx_wait_blocking(int32_t controller) {
    return enabled(controller); /* Polling output completes before returning. */
}

int32_t bmx_embedded_uart_set_break(int32_t controller, int32_t enabled_break) {
    extern int32_t bmx_embedded_uart_async_is_open(int32_t);
    if (!enabled(controller) || bmx_embedded_uart_async_is_open(controller)) return 0;
    if (!!enabled_break == !!break_active) return 1;
    const struct device *port = tx_gpio_device();
    if (!device_is_ready(port)) return 0;
    uint32_t pin = UART_TX_PIN % 32u;
    if (enabled_break) {
        if (pending_valid || poll_rx_get != poll_rx_put) return 0;
        if (!bmx_nrf52_uart_poll_suspend()) return 0;
        if (pending_valid || poll_rx_get != poll_rx_put) {
            (void)bmx_nrf52_uart_poll_resume();
            return 0;
        }
        if (pm_device_action_run(uart_device(), PM_DEVICE_ACTION_SUSPEND) != 0) {
            (void)bmx_nrf52_uart_poll_resume();
            return 0;
        }
        if (gpio_pin_configure(port, pin, GPIO_OUTPUT_LOW) != 0) {
            (void)pm_device_action_run(uart_device(), PM_DEVICE_ACTION_RESUME);
            (void)bmx_nrf52_uart_poll_resume();
            return 0;
        }
        break_active = 1;
        return 1;
    }
    if (gpio_pin_configure(port, pin, GPIO_OUTPUT_HIGH) != 0) return 0;
    if (pm_device_action_run(uart_device(), PM_DEVICE_ACTION_RESUME) != 0) return 0;
    break_active = 0;
    return bmx_nrf52_uart_poll_resume();
}

int32_t bmx_embedded_uart_set_translate_crlf(int32_t controller, int32_t value) {
    if (!enabled(controller)) return 0;
    translate_crlf = value != 0;
    return 1;
}

uint32_t bmx_embedded_uart_get_errors(int32_t controller) {
    if (!enabled(controller)) return 0;
    int error = uart_err_check(uart_device());
    if (error > 0) {
        if (error & UART_ERROR_FRAMING) errors |= 1u;
        if (error & UART_ERROR_PARITY) errors |= 2u;
        if (error & UART_BREAK) errors |= 4u;
        if (error & UART_ERROR_OVERRUN) errors |= 8u;
    }
    return errors;
}

int32_t bmx_embedded_uart_clear_errors(int32_t controller) {
    if (!enabled(controller)) return 0;
    errors = 0;
    (void)uart_err_check(uart_device());
    return 1;
}
