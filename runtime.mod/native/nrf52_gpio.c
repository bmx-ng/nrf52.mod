#include <stdint.h>

#include "blitzmax/embedded_events.h"
#include "blitzmax/embedded_gpio.h"
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/dt-bindings/gpio/nordic-nrf-gpio.h>
#include <zephyr/irq.h>
#include <zephyr/kernel.h>

#define BMX_NRF52_GPIO_COUNT 48u

typedef struct {
    uint8_t initialized;
    uint8_t output;
    uint8_t value;
    uint8_t pull_up;
    uint8_t pull_down;
    uint8_t drive_strength;
} BMXNrf52GPIOState;

static BMXNrf52GPIOState pins[BMX_NRF52_GPIO_COUNT];
static struct gpio_callback callbacks[BMX_NRF52_GPIO_COUNT];
static uint8_t callback_added[BMX_NRF52_GPIO_COUNT];
static volatile uint32_t irq_events[BMX_NRF52_GPIO_COUNT];
static volatile uint32_t irq_masks[BMX_NRF52_GPIO_COUNT];
static volatile uint32_t event_tokens[BMX_NRF52_GPIO_COUNT];

#define IRQ_LEVEL_LOW 1u
#define IRQ_LEVEL_HIGH 2u
#define IRQ_EDGE_FALL 4u
#define IRQ_EDGE_RISE 8u

static const struct device *gpio_device(uint32_t pin) {
    if (pin < 32u) return DEVICE_DT_GET(DT_NODELABEL(gpio0));
    if (pin < BMX_NRF52_GPIO_COUNT) return DEVICE_DT_GET(DT_NODELABEL(gpio1));
    return NULL;
}

static gpio_pin_t port_pin(uint32_t pin) {
    return (gpio_pin_t)(pin & 31u);
}

int32_t bmx_embedded_gpio_is_valid(uint32_t pin) {
    /* nRESET and USB D-/D+ must never be claimed as application GPIO. */
    if (pin >= BMX_NRF52_GPIO_COUNT || pin == 18u || pin == 24u || pin == 25u)
        return 0;
    const struct device *port = gpio_device(pin);
    return device_is_ready(port);
}

int32_t bmx_embedded_gpio_is_output_capable(uint32_t pin) {
    return bmx_embedded_gpio_is_valid(pin);
}

int32_t bmx_embedded_gpio_is_pull_capable(uint32_t pin) {
    return bmx_embedded_gpio_is_valid(pin);
}

int32_t bmx_embedded_gpio_is_pull_up_capable(uint32_t pin) {
    return bmx_embedded_gpio_is_valid(pin);
}

int32_t bmx_embedded_gpio_is_pull_down_capable(uint32_t pin) {
    return bmx_embedded_gpio_is_valid(pin);
}

int32_t bmx_embedded_gpio_is_drive_strength_configurable(uint32_t pin) {
    return bmx_embedded_gpio_is_output_capable(pin);
}

int32_t bmx_embedded_gpio_is_irq_capable(uint32_t pin) {
    return bmx_embedded_gpio_is_valid(pin);
}

static int32_t configure_pin(uint32_t pin) {
    gpio_flags_t flags = pins[pin].output
        ? (GPIO_INPUT | (pins[pin].value ? GPIO_OUTPUT_HIGH : GPIO_OUTPUT_LOW))
        : GPIO_INPUT;
    if (pins[pin].pull_up) flags |= GPIO_PULL_UP;
    if (pins[pin].pull_down) flags |= GPIO_PULL_DOWN;
    if (pins[pin].drive_strength == 3u) flags |= NRF_GPIO_DRIVE_H0H1;
    return gpio_pin_configure(gpio_device(pin), port_pin(pin), flags) == 0;
}

int32_t bmx_embedded_gpio_init(uint32_t pin) {
    if (!bmx_embedded_gpio_is_valid(pin)) return 0;
    (void)gpio_pin_interrupt_configure(gpio_device(pin), port_pin(pin),
        GPIO_INT_DISABLE);
    if (callback_added[pin]) {
        (void)gpio_remove_callback(gpio_device(pin), &callbacks[pin]);
        callback_added[pin] = 0;
    }
    unsigned int key = irq_lock();
    irq_events[pin] = 0;
    irq_masks[pin] = 0;
    event_tokens[pin] = 0;
    irq_unlock(key);
    pins[pin] = (BMXNrf52GPIOState){0};
    if (!configure_pin(pin)) return 0;
    pins[pin].initialized = 1;
    return 1;
}

int32_t bmx_embedded_gpio_set_direction(uint32_t pin, int32_t direction) {
    if (!bmx_embedded_gpio_is_valid(pin) || !pins[pin].initialized ||
        (direction != 0 && direction != 1)) return 0;
    uint8_t prior = pins[pin].output;
    pins[pin].output = direction != 0;
    if (configure_pin(pin)) return 1;
    pins[pin].output = prior;
    return 0;
}

int32_t bmx_embedded_gpio_get_direction(uint32_t pin) {
    return bmx_embedded_gpio_is_valid(pin) && pins[pin].initialized && pins[pin].output;
}

int32_t bmx_embedded_gpio_set_input(uint32_t pin) {
    return bmx_embedded_gpio_set_direction(pin, 0);
}

int32_t bmx_embedded_gpio_set_output(uint32_t pin) {
    return bmx_embedded_gpio_set_direction(pin, 1);
}

int32_t bmx_embedded_gpio_get(uint32_t pin) {
    if (!bmx_embedded_gpio_is_valid(pin) || !pins[pin].initialized) return 0;
    int value = gpio_pin_get_raw(gpio_device(pin), port_pin(pin));
    return value > 0;
}

int32_t bmx_embedded_gpio_put(uint32_t pin, int32_t value) {
    if (!bmx_embedded_gpio_is_valid(pin) || !pins[pin].initialized ||
        !pins[pin].output) return 0;
    if (gpio_pin_set_raw(gpio_device(pin), port_pin(pin), value != 0) != 0) return 0;
    pins[pin].value = value != 0;
    return 1;
}

int32_t bmx_embedded_gpio_get_output(uint32_t pin) {
    return bmx_embedded_gpio_is_valid(pin) && pins[pin].initialized && pins[pin].value;
}

int32_t bmx_embedded_gpio_set_pulls(uint32_t pin, int32_t pull_up, int32_t pull_down) {
    if (!bmx_embedded_gpio_is_valid(pin) || !pins[pin].initialized ||
        (pull_up && pull_down)) return 0;
    uint8_t prior_up = pins[pin].pull_up;
    uint8_t prior_down = pins[pin].pull_down;
    pins[pin].pull_up = pull_up != 0;
    pins[pin].pull_down = pull_down != 0;
    if (configure_pin(pin)) return 1;
    pins[pin].pull_up = prior_up;
    pins[pin].pull_down = prior_down;
    return 0;
}

int32_t bmx_embedded_gpio_pull_up(uint32_t pin) {
    return bmx_embedded_gpio_set_pulls(pin, 1, 0);
}

int32_t bmx_embedded_gpio_pull_down(uint32_t pin) {
    return bmx_embedded_gpio_set_pulls(pin, 0, 1);
}

int32_t bmx_embedded_gpio_disable_pulls(uint32_t pin) {
    return bmx_embedded_gpio_set_pulls(pin, 0, 0);
}

int32_t bmx_embedded_gpio_is_pulled_up(uint32_t pin) {
    return bmx_embedded_gpio_is_valid(pin) && pins[pin].initialized && pins[pin].pull_up;
}

int32_t bmx_embedded_gpio_is_pulled_down(uint32_t pin) {
    return bmx_embedded_gpio_is_valid(pin) && pins[pin].initialized && pins[pin].pull_down;
}

int32_t bmx_embedded_gpio_set_drive_strength(uint32_t pin, int32_t strength) {
    if (!bmx_embedded_gpio_is_valid(pin) || !pins[pin].initialized ||
            !pins[pin].output ||
            (strength != 0 && strength != 3)) return 0;
    uint8_t prior = pins[pin].drive_strength;
    pins[pin].drive_strength = (uint8_t)strength;
    if (configure_pin(pin)) return 1;
    pins[pin].drive_strength = prior;
    return 0;
}

int32_t bmx_embedded_gpio_get_drive_strength(uint32_t pin) {
    return bmx_embedded_gpio_is_valid(pin) && pins[pin].initialized
        ? pins[pin].drive_strength : 0;
}

static gpio_flags_t interrupt_flags(uint32_t mask) {
    switch (mask) {
        case IRQ_LEVEL_LOW: return GPIO_INT_LEVEL_LOW;
        case IRQ_LEVEL_HIGH: return GPIO_INT_LEVEL_HIGH;
        case IRQ_EDGE_FALL: return GPIO_INT_EDGE_FALLING;
        case IRQ_EDGE_RISE: return GPIO_INT_EDGE_RISING;
        case IRQ_EDGE_FALL | IRQ_EDGE_RISE: return GPIO_INT_EDGE_BOTH;
        default: return 0;
    }
}

static void gpio_interrupt(const struct device *port,
        struct gpio_callback *callback, gpio_port_pins_t triggered) {
    uint32_t pin = (uint32_t)(callback - callbacks);
    if (pin >= BMX_NRF52_GPIO_COUNT ||
        !(triggered & BIT(port_pin(pin))) || port != gpio_device(pin)) return;
    uint32_t mask = irq_masks[pin];
    int level = gpio_pin_get_raw(port, port_pin(pin));
    uint32_t event = 0;
    if (mask == (IRQ_EDGE_FALL | IRQ_EDGE_RISE))
        event = level > 0 ? IRQ_EDGE_RISE : IRQ_EDGE_FALL;
    else if (mask == IRQ_LEVEL_LOW || mask == IRQ_LEVEL_HIGH ||
             mask == IRQ_EDGE_FALL || mask == IRQ_EDGE_RISE)
        event = mask;
    if (!event) return;
    unsigned int key = irq_lock();
    irq_events[pin] |= event;
    uint32_t token = event_tokens[pin];
    irq_unlock(key);
    uint64_t timestamp = k_ticks_to_us_floor64(k_uptime_ticks());
    if (token) (void)bmx_embedded_event_post_from_isr_ex(token, event, pin,
        (uint32_t)timestamp, (uint32_t)(timestamp >> 32u));
}

int32_t bmx_embedded_gpio_set_irq_enabled(uint32_t pin, uint32_t mask, int32_t enabled) {
    if (!bmx_embedded_gpio_is_valid(pin) || !pins[pin].initialized) return 0;
    gpio_flags_t flags = interrupt_flags(mask);
    if (!flags) return 0;
    if (!enabled) {
        if (gpio_pin_interrupt_configure(gpio_device(pin), port_pin(pin),
                GPIO_INT_DISABLE) != 0) return 0;
        irq_masks[pin] = 0;
        return 1;
    }
    if (!callback_added[pin]) {
        gpio_init_callback(&callbacks[pin], gpio_interrupt, BIT(port_pin(pin)));
        if (gpio_add_callback(gpio_device(pin), &callbacks[pin]) != 0) return 0;
        callback_added[pin] = 1;
    }
    irq_masks[pin] = mask;
    if (gpio_pin_interrupt_configure(gpio_device(pin), port_pin(pin), flags) == 0)
        return 1;
    irq_masks[pin] = 0;
    return 0;
}

int32_t bmx_embedded_gpio_set_event_token(uint32_t pin, uint32_t token) {
    if (!bmx_embedded_gpio_is_valid(pin) || !pins[pin].initialized) return 0;
    unsigned int key = irq_lock();
    if (token && event_tokens[pin] && event_tokens[pin] != token) {
        irq_unlock(key);
        return 0;
    }
    event_tokens[pin] = token;
    irq_unlock(key);
    return 1;
}

uint32_t bmx_embedded_gpio_pending_irq_events(uint32_t pin) {
    if (!bmx_embedded_gpio_is_valid(pin)) return 0;
    unsigned int key = irq_lock();
    uint32_t events = irq_events[pin];
    irq_unlock(key);
    return events;
}

uint32_t bmx_embedded_gpio_take_irq_events(uint32_t pin) {
    if (!bmx_embedded_gpio_is_valid(pin)) return 0;
    unsigned int key = irq_lock();
    uint32_t events = irq_events[pin];
    irq_events[pin] = 0;
    irq_unlock(key);
    return events;
}
