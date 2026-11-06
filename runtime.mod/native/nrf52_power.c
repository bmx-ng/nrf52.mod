#include <stdint.h>

#include "blitzmax/embedded_gpio.h"
#include <hal/nrf_gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/poweroff.h>

#define POWER_CAPABILITY_IDLE 0x01u
#define POWER_CAPABILITY_SLEEP_TIMER 0x04u
#define POWER_SUCCESS 0
#define POWER_NOT_PERMITTED (-4)
#define POWER_INVALID_ARGUMENT (-5)
#define POWER_UNAVAILABLE (-17)
#define POWER_PRECONDITION_NOT_MET (-14)

uint32_t bmx_embedded_power_capabilities(void) {
    return POWER_CAPABILITY_IDLE | POWER_CAPABILITY_SLEEP_TIMER;
}

void bmx_embedded_power_idle(void) {
    if (k_can_yield()) k_msleep(1);
}

int32_t bmx_embedded_power_sleep_until_interrupt(void) {
    return POWER_UNAVAILABLE;
}

int32_t bmx_embedded_power_sleep_for_ms(uint32_t milliseconds, int32_t exclusive) {
    (void)exclusive;
    if (!milliseconds || milliseconds > INT32_MAX) return POWER_INVALID_ARGUMENT;
    if (!k_can_yield()) return POWER_NOT_PERMITTED;
    k_msleep((int32_t)milliseconds);
    return POWER_SUCCESS;
}

int32_t bmx_embedded_power_dormant_for_ms(uint32_t milliseconds) {
    (void)milliseconds;
    return POWER_UNAVAILABLE;
}

int32_t bmx_embedded_power_dormant_until_gpio(uint32_t pin, int32_t edge,
        int32_t high) {
    (void)pin;
    (void)edge;
    (void)high;
    return POWER_UNAVAILABLE;
}

int32_t bmx_embedded_power_set_unused_pins_low_leakage(uint64_t exclude_mask) {
    (void)exclude_mask;
    return POWER_UNAVAILABLE;
}

int32_t bmx_nrf52_system_off(void) {
    if (!k_can_yield()) return POWER_NOT_PERMITTED;
    sys_poweroff();
}

int32_t bmx_nrf52_system_off_until_gpio(uint32_t pin, int32_t active_high) {
    if (!k_can_yield()) return POWER_NOT_PERMITTED;
    if (!bmx_embedded_gpio_is_valid(pin)) return POWER_INVALID_ARGUMENT;

    NRF_GPIO_Type *port = pin < 32u ? NRF_P0 : NRF_P1;
    uint32_t port_pin = pin & 31u;
    uint32_t previous_config = port->PIN_CNF[port_pin];
    nrf_gpio_cfg_sense_input(pin,
        active_high ? NRF_GPIO_PIN_PULLDOWN : NRF_GPIO_PIN_PULLUP,
        active_high ? NRF_GPIO_PIN_SENSE_HIGH : NRF_GPIO_PIN_SENSE_LOW);
    k_busy_wait(100);
    if (nrf_gpio_pin_read(pin) == (active_high != 0)) {
        port->PIN_CNF[port_pin] = previous_config;
        return POWER_PRECONDITION_NOT_MET;
    }

    sys_poweroff();
}
