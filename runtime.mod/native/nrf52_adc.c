#include <stdint.h>

#include "blitzmax/embedded_adc.h"
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/dt-bindings/adc/nrf-saadc.h>

static uint8_t active_channels[8];

static int channel_for_pin(uint32_t pin) {
    switch (pin) {
        case 2u: return 0;
        case 3u: return 1;
        case 4u: return 2;
        case 5u: return 3;
        case 28u: return 4;
        case 29u: return 5;
        case 30u: return 6;
        case 31u: return 7;
        default: return -1;
    }
}

static const struct device *adc_device(void) {
    return DEVICE_DT_GET(DT_NODELABEL(adc));
}

int32_t bmx_embedded_adc_is_valid_pin(uint32_t pin) {
    return channel_for_pin(pin) >= 0 && device_is_ready(adc_device());
}

int32_t bmx_embedded_adc_init_pin(uint32_t pin) {
    int channel = channel_for_pin(pin);
    if (channel < 0 || !device_is_ready(adc_device())) return 0;
    struct adc_channel_cfg config = {
        .gain = ADC_GAIN_1_6,
        .reference = ADC_REF_INTERNAL,
        .acquisition_time = ADC_ACQ_TIME_DEFAULT,
        .channel_id = (uint8_t)channel,
        .differential = 0,
#ifdef CONFIG_ADC_CONFIGURABLE_INPUTS
        .input_positive = (uint8_t)(NRF_SAADC_AIN0 + channel),
#endif
    };
    if (adc_channel_setup(adc_device(), &config) != 0) return 0;
    active_channels[channel] = 1;
    return 1;
}

int32_t bmx_embedded_adc_deinit_pin(uint32_t pin) {
    int channel = channel_for_pin(pin);
    if (channel < 0 || !active_channels[channel]) return 0;
    active_channels[channel] = 0;
    return 1;
}

int32_t bmx_embedded_adc_read_raw(uint32_t pin, uint32_t *value) {
    int channel = channel_for_pin(pin);
    if (!value || channel < 0 || !active_channels[channel]) return 0;
    int16_t sample = 0;
    struct adc_sequence sequence = {
        .channels = 1u << channel,
        .buffer = &sample,
        .buffer_size = sizeof(sample),
        .resolution = 12,
    };
    if (adc_read(adc_device(), &sequence) != 0) return 0;
    *value = sample < 0 ? 0u : (uint32_t)sample;
    return 1;
}

uint32_t bmx_embedded_adc_resolution_bits(uint32_t pin) {
    return bmx_embedded_adc_is_valid_pin(pin) ? 12u : 0u;
}

uint32_t bmx_embedded_adc_maximum_value(uint32_t pin) {
    return bmx_embedded_adc_is_valid_pin(pin) ? 4095u : 0u;
}
