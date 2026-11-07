#include <stddef.h>
#include <stdint.h>

#include "blitzmax/embedded_pwm.h"
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/dt-bindings/pinctrl/nrf-pinctrl.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/sys/util.h>

#define BMX_NRF52_PWM_LED_PIN 15u
#define BMX_NRF52_PWM_D4_PIN 22u
#define BMX_NRF52_PWM_CHANNEL 0u
#define BMX_PWM_PIN(psel) (((psel) >> NRF_PIN_POS) & NRF_PIN_MSK)
#define BMX_PWM_FUN(psel) (((psel) >> NRF_FUN_POS) & NRF_FUN_MSK)
#define BMX_PWM_PSEL(label) DT_PROP_BY_IDX(DT_CHILD(DT_NODELABEL(label), group1), psels, 0)
#define BMX_PWM_BUS_PIN(label, index) BMX_PWM_PIN(DT_PROP_BY_IDX( \
    DT_CHILD(DT_NODELABEL(label), group1), psels, index))
#define BMX_PWM_PIN_RESERVED(pin) ((pin) >= 48u || (pin) == 18u || \
    (pin) == 24u || (pin) == 25u)
#define BMX_PWM_PIN_IN_USE(pin) ((pin) == BMX_NRF52_PWM_LED_PIN || \
    (pin) == BMX_NRF52_PWM_D4_PIN || \
    (pin) == BMX_PWM_BUS_PIN(uart0_default, 0) || \
    (pin) == BMX_PWM_BUS_PIN(uart0_default, 1) || \
    (pin) == BMX_PWM_BUS_PIN(uart0_default, 2) || \
    (pin) == BMX_PWM_BUS_PIN(uart0_default, 3) || \
    (pin) == BMX_PWM_BUS_PIN(spi2_default, 0) || \
    (pin) == BMX_PWM_BUS_PIN(spi2_default, 1) || \
    (pin) == BMX_PWM_BUS_PIN(spi2_default, 2) || \
    (pin) == BMX_PWM_BUS_PIN(i2c1_default, 0) || \
    (pin) == BMX_PWM_BUS_PIN(i2c1_default, 1))

#if DT_NODE_HAS_STATUS(DT_NODELABEL(pwm2), okay)
#define BMX_PWM2_PIN BMX_PWM_PIN(BMX_PWM_PSEL(bmx_pwm2_default))
BUILD_ASSERT(DT_PROP_LEN(DT_CHILD(DT_NODELABEL(bmx_pwm2_default), group1), psels) == 1 &&
    DT_PROP_LEN(DT_CHILD(DT_NODELABEL(bmx_pwm2_sleep), group1), psels) == 1 &&
    BMX_PWM_FUN(BMX_PWM_PSEL(bmx_pwm2_default)) == NRF_FUN_PWM_OUT0 &&
    BMX_PWM2_PIN == BMX_PWM_PIN(BMX_PWM_PSEL(bmx_pwm2_sleep)) &&
    !BMX_PWM_PIN_RESERVED(BMX_PWM2_PIN) && !BMX_PWM_PIN_IN_USE(BMX_PWM2_PIN),
    "PWM2 needs one valid, unclaimed output pin");
#endif
#if DT_NODE_HAS_STATUS(DT_NODELABEL(pwm3), okay)
#define BMX_PWM3_PIN BMX_PWM_PIN(BMX_PWM_PSEL(bmx_pwm3_default))
BUILD_ASSERT(DT_PROP_LEN(DT_CHILD(DT_NODELABEL(bmx_pwm3_default), group1), psels) == 1 &&
    DT_PROP_LEN(DT_CHILD(DT_NODELABEL(bmx_pwm3_sleep), group1), psels) == 1 &&
    BMX_PWM_FUN(BMX_PWM_PSEL(bmx_pwm3_default)) == NRF_FUN_PWM_OUT0 &&
    BMX_PWM3_PIN == BMX_PWM_PIN(BMX_PWM_PSEL(bmx_pwm3_sleep)) &&
    !BMX_PWM_PIN_RESERVED(BMX_PWM3_PIN) && !BMX_PWM_PIN_IN_USE(BMX_PWM3_PIN),
    "PWM3 needs one valid, unclaimed output pin");
#endif
#if DT_NODE_HAS_STATUS(DT_NODELABEL(pwm2), okay) && \
    DT_NODE_HAS_STATUS(DT_NODELABEL(pwm3), okay)
BUILD_ASSERT(BMX_PWM2_PIN != BMX_PWM3_PIN,
    "PWM2 and PWM3 need distinct output pins");
#endif

typedef struct {
    uint8_t configured;
    uint8_t enabled;
    uint8_t inverted;
    uint32_t frequency;
    uint32_t duty;
} BMXNrf52PWMState;

static BMXNrf52PWMState pwm_states[4];

static int pwm_index(uint32_t pin) {
    if (pin == BMX_NRF52_PWM_LED_PIN) return 0;
    if (pin == BMX_NRF52_PWM_D4_PIN) return 1;
#if DT_NODE_HAS_STATUS(DT_NODELABEL(pwm2), okay)
    if (pin == BMX_PWM2_PIN) return 2;
#endif
#if DT_NODE_HAS_STATUS(DT_NODELABEL(pwm3), okay)
    if (pin == BMX_PWM3_PIN) return 3;
#endif
    return -1;
}

static const struct device *pwm_device(int index) {
    switch (index) {
    case 0: return DEVICE_DT_GET(DT_NODELABEL(pwm0));
    case 1: return DEVICE_DT_GET(DT_NODELABEL(pwm1));
#if DT_NODE_HAS_STATUS(DT_NODELABEL(pwm2), okay)
    case 2: return DEVICE_DT_GET(DT_NODELABEL(pwm2));
#endif
#if DT_NODE_HAS_STATUS(DT_NODELABEL(pwm3), okay)
    case 3: return DEVICE_DT_GET(DT_NODELABEL(pwm3));
#endif
    default: return NULL;
    }
}

int32_t bmx_embedded_pwm_is_valid_pin(uint32_t pin) {
    int index = pwm_index(pin);
    return index >= 0 && device_is_ready(pwm_device(index));
}

static int32_t apply_pwm(int index, const BMXNrf52PWMState *state) {
    if (!state->frequency) return 0;
    uint64_t period = (UINT64_C(1000000000) + state->frequency / 2u) /
        state->frequency;
    if (period == 0 || period > UINT32_MAX) return 0;
    uint64_t pulse = state->enabled
        ? (period * state->duty + BMX_EMBEDDED_PWM_DUTY_MAXIMUM / 2u) /
            BMX_EMBEDDED_PWM_DUTY_MAXIMUM
        : 0;
    pwm_flags_t flags = state->inverted ? PWM_POLARITY_INVERTED : 0;
    return pwm_set(pwm_device(index), BMX_NRF52_PWM_CHANNEL,
        (uint32_t)period, (uint32_t)pulse, flags) == 0;
}

uint32_t bmx_embedded_pwm_init_pin(uint32_t pin, uint32_t frequency,
        uint32_t duty, int32_t inverted) {
    int index = pwm_index(pin);
    if (!bmx_embedded_pwm_is_valid_pin(pin) || !frequency ||
        duty > BMX_EMBEDDED_PWM_DUTY_MAXIMUM) return 0;
    BMXNrf52PWMState next = {
        .configured = 1, .enabled = 1, .inverted = inverted != 0,
        .frequency = frequency, .duty = duty,
    };
    if (!apply_pwm(index, &next)) return 0;
    pwm_states[index] = next;
    return frequency;
}

int32_t bmx_embedded_pwm_deinit_pin(uint32_t pin) {
    int index = pwm_index(pin);
    if (index < 0 || !pwm_states[index].configured) return 0;
    BMXNrf52PWMState next = pwm_states[index];
    next.enabled = 0;
    if (!apply_pwm(index, &next)) return 0;
    pwm_states[index] = (BMXNrf52PWMState){0};
    return 1;
}

uint32_t bmx_embedded_pwm_set_pin_frequency(uint32_t pin, uint32_t frequency) {
    int index = pwm_index(pin);
    if (index < 0 || !pwm_states[index].configured || !frequency) return 0;
    BMXNrf52PWMState next = pwm_states[index];
    next.frequency = frequency;
    if (!apply_pwm(index, &next)) return 0;
    pwm_states[index] = next;
    return frequency;
}

uint32_t bmx_embedded_pwm_get_pin_frequency(uint32_t pin) {
    int index = pwm_index(pin);
    return index >= 0 && pwm_states[index].configured
        ? pwm_states[index].frequency : 0;
}

int32_t bmx_embedded_pwm_set_pin_duty(uint32_t pin, uint32_t duty) {
    int index = pwm_index(pin);
    if (index < 0 || !pwm_states[index].configured ||
        duty > BMX_EMBEDDED_PWM_DUTY_MAXIMUM) return 0;
    BMXNrf52PWMState next = pwm_states[index];
    next.duty = duty;
    if (!apply_pwm(index, &next)) return 0;
    pwm_states[index] = next;
    return 1;
}

uint32_t bmx_embedded_pwm_get_pin_duty(uint32_t pin) {
    int index = pwm_index(pin);
    return index >= 0 && pwm_states[index].configured
        ? pwm_states[index].duty : 0;
}

int32_t bmx_embedded_pwm_set_pin_polarity(uint32_t pin, int32_t inverted) {
    int index = pwm_index(pin);
    if (index < 0 || !pwm_states[index].configured) return 0;
    BMXNrf52PWMState next = pwm_states[index];
    next.inverted = inverted != 0;
    if (!apply_pwm(index, &next)) return 0;
    pwm_states[index] = next;
    return 1;
}

int32_t bmx_embedded_pwm_get_pin_polarity(uint32_t pin) {
    int index = pwm_index(pin);
    return index >= 0 && pwm_states[index].configured &&
        pwm_states[index].inverted;
}

int32_t bmx_embedded_pwm_set_pin_enabled(uint32_t pin, int32_t enabled) {
    int index = pwm_index(pin);
    if (index < 0 || !pwm_states[index].configured) return 0;
    BMXNrf52PWMState next = pwm_states[index];
    next.enabled = enabled != 0;
    if (!apply_pwm(index, &next)) return 0;
    pwm_states[index] = next;
    return 1;
}

int32_t bmx_embedded_pwm_get_pin_enabled(uint32_t pin) {
    int index = pwm_index(pin);
    return index >= 0 && pwm_states[index].configured &&
        pwm_states[index].enabled;
}
