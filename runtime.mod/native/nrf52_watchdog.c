#include <stdint.h>

#include <hal/nrf_wdt.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/hwinfo.h>
#include <zephyr/drivers/watchdog.h>

extern uint32_t bmx_nrf52_reset_cause_flags(void);

static int watchdog_channel = -1;
static uint8_t watchdog_enabled;

static const struct device *watchdog_device(void) {
    const struct device *device = DEVICE_DT_GET(DT_ALIAS(watchdog0));
    return device_is_ready(device) ? device : NULL;
}

uint32_t bmx_embedded_watchdog_maximum_delay_ms(void) {
    return NRF_WDT_RR_VALUE_MS;
}

int32_t bmx_embedded_watchdog_enable(uint32_t delay_ms) {
    if (delay_ms == 0 || delay_ms > NRF_WDT_RR_VALUE_MS || watchdog_enabled)
        return 0;
    const struct device *device = watchdog_device();
    if (!device) return 0;
    struct wdt_timeout_cfg timeout = {
        .window = { .min = 0, .max = delay_ms },
        .flags = WDT_FLAG_RESET_SOC,
    };
    int channel = wdt_install_timeout(device, &timeout);
    if (channel < 0) return 0;
    if (wdt_setup(device, WDT_OPT_PAUSE_HALTED_BY_DBG) != 0) return 0;
    watchdog_channel = channel;
    watchdog_enabled = 1;
    return 1;
}

int32_t bmx_embedded_watchdog_disable(void) {
    if (!watchdog_enabled) return 1;
    const struct device *device = watchdog_device();
    if (!device || wdt_disable(device) != 0) return 0;
    watchdog_channel = -1;
    watchdog_enabled = 0;
    return 1;
}

int32_t bmx_embedded_watchdog_feed(void) {
    const struct device *device = watchdog_device();
    return watchdog_enabled && device && watchdog_channel >= 0 &&
        wdt_feed(device, watchdog_channel) == 0;
}

int32_t bmx_embedded_watchdog_is_enabled(void) {
    return watchdog_enabled;
}

int32_t bmx_embedded_watchdog_caused_reboot(void) {
    return (bmx_nrf52_reset_cause_flags() & RESET_WATCHDOG) != 0;
}
