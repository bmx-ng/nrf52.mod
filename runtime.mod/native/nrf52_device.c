#include <stdint.h>
#include <string.h>

#include "blitzmax/embedded_device.h"
#include <zephyr/drivers/hwinfo.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/reboot.h>

#define BMX_NRF52_DEVICE_ID_BYTES 8u

static uint32_t startup_reset_cause_flags;
static uint8_t startup_reset_cause_captured;

void bmx_nrf52_capture_reset_cause(void) {
    if (startup_reset_cause_captured) return;
    uint32_t flags = 0;
    if (hwinfo_get_reset_cause(&flags) == 0) {
        startup_reset_cause_flags = flags;
        (void)hwinfo_clear_reset_cause();
    }
    startup_reset_cause_captured = 1;
}

static int bmx_nrf52_read_device_id(uint8_t identifier[BMX_NRF52_DEVICE_ID_BYTES]) {
    return hwinfo_get_device_id(identifier, BMX_NRF52_DEVICE_ID_BYTES) ==
        BMX_NRF52_DEVICE_ID_BYTES;
}

const BMXEmbeddedString *bmx_embedded_unique_device_id(void) {
    uint8_t identifier[BMX_NRF52_DEVICE_ID_BYTES];
    if (!bmx_nrf52_read_device_id(identifier))
        return bmx_embedded_string_from_ascii("", 0);
    return bmx_embedded_string_from_bytes_as_hex(identifier,
        BMX_NRF52_DEVICE_ID_BYTES, 1);
}

BMXEmbeddedArray *bmx_embedded_unique_device_id_bytes(void) {
    uint8_t identifier[BMX_NRF52_DEVICE_ID_BYTES];
    if (!bmx_nrf52_read_device_id(identifier)) return &bmx_embedded_empty_array;
    BMXEmbeddedArray *result = bmx_embedded_array_new_1d(
        BMX_NRF52_DEVICE_ID_BYTES, sizeof(uint8_t),
        BMX_EMBEDDED_ARRAY_ELEMENT_VALUE, NULL, NULL);
    if (result != &bmx_embedded_empty_array)
        memcpy(bmx_embedded_array_data(result), identifier, sizeof(identifier));
    return result;
}

uint32_t bmx_nrf52_reset_cause_flags(void) {
    bmx_nrf52_capture_reset_cause();
    return startup_reset_cause_flags;
}

uint32_t bmx_nrf52_supported_reset_cause_flags(void) {
    uint32_t flags = 0;
    return hwinfo_get_supported_reset_cause(&flags) == 0 ? flags : 0;
}

int32_t bmx_embedded_device_reset_reason(void) {
    uint32_t flags = bmx_nrf52_reset_cause_flags();
    if (flags & RESET_WATCHDOG) return BMX_EMBEDDED_RESET_REASON_WATCHDOG;
    if (flags & RESET_CPU_LOCKUP) return BMX_EMBEDDED_RESET_REASON_CPU_LOCKUP;
    if (flags & RESET_PIN) return BMX_EMBEDDED_RESET_REASON_EXTERNAL;
    if (flags & RESET_SOFTWARE) return BMX_EMBEDDED_RESET_REASON_SOFTWARE;
    if (flags & RESET_BROWNOUT) return BMX_EMBEDDED_RESET_REASON_BROWNOUT;
    if (flags & RESET_LOW_POWER_WAKE) return BMX_EMBEDDED_RESET_REASON_DEEP_SLEEP;
    if (flags & RESET_POR) return BMX_EMBEDDED_RESET_REASON_POWER_ON;
    return BMX_EMBEDDED_RESET_REASON_UNKNOWN;
}

int32_t bmx_embedded_device_reboot(uint32_t delay_ms) {
    if (delay_ms > INT32_MAX) return 0;
    if (delay_ms) k_msleep((int32_t)delay_ms);
    sys_reboot(SYS_REBOOT_COLD);
    return 0;
}
