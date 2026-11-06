#include <stdint.h>
#include <stdio.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/kernel.h>

static k_tid_t managed_thread;

extern void bmx_nrf52_capture_reset_cause(void);

void bmx_nrf52_managed_init(void) {
    managed_thread = k_current_get();
    bmx_nrf52_capture_reset_cause();
}

int bmx_nrf52_context_valid(void) {
    return !k_is_in_isr() && managed_thread == k_current_get();
}

int32_t bmx_nrf52_stdio_init_all(void) {
    return 1;
}

int64_t bmx_nrf52_stdio_read(void *buffer, int64_t count) {
    if (!buffer || count <= 0) return 0;
    const struct device *console = DEVICE_DT_GET(DT_CHOSEN(zephyr_console));
    if (!device_is_ready(console)) return 0;
    uint8_t *bytes = buffer;
    unsigned char value;
    while (uart_poll_in(console, &value) != 0) k_msleep(1);
    bytes[0] = value;
    int64_t received = 1;
    while (received < count && uart_poll_in(console, &value) == 0)
        bytes[received++] = value;
    return received;
}

int64_t bmx_nrf52_stdio_write(void *buffer, int64_t count) {
    if (!buffer || count <= 0) return 0;
    size_t written = fwrite(buffer, 1, (size_t)count, stdout);
    fflush(stdout);
    return (int64_t)written;
}

void bmx_nrf52_stdio_flush(void) {
    fflush(stdout);
}

int32_t bmx_embedded_millisecs(void) {
    return (int32_t)(uint32_t)k_uptime_get();
}

void bmx_embedded_delay(int32_t milliseconds) {
    if (milliseconds > 0) k_msleep(milliseconds);
}

void bmx_embedded_udelay(int32_t microseconds) {
    if (microseconds > 0) k_usleep(microseconds);
}

uint64_t bmx_embedded_time_microseconds(void) {
    return k_ticks_to_us_floor64(k_uptime_ticks());
}

uint64_t bmx_embedded_time_milliseconds(void) {
    return (uint64_t)k_uptime_get();
}

void bmx_embedded_sleep_milliseconds(uint32_t milliseconds) {
    k_msleep(milliseconds);
}

void bmx_embedded_sleep_microseconds(uint64_t microseconds) {
    while (microseconds > 0) {
        int32_t chunk = microseconds > 1000000u ? 1000000 : (int32_t)microseconds;
        k_usleep(chunk);
        microseconds -= (uint32_t)chunk;
    }
}
