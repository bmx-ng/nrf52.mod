#include <stdint.h>
#include <string.h>

#include "blitzmax/embedded_events.h"
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/irq.h>
#include <zephyr/kernel.h>

#define RING_MAX 512u
#define DMA_SIZE 64u
#define UART_INVALID_ARGUMENT (-5)

static struct {
    uint8_t rx[RING_MAX];
    uint8_t tx[RING_MAX];
    uint8_t rx_dma[2][DMA_SIZE];
    uint8_t tx_dma[DMA_SIZE];
    uint32_t rx_size, tx_size;
    uint32_t rx_put, rx_get, tx_put, tx_get, dropped;
    uint32_t rx_token, tx_token, error_token;
    uint8_t open, tx_active, rx_dma_busy[2];
    volatile uint8_t rx_disabled;
    uint32_t tx_dma_len;
} state;

static const struct device *uart_device(void) {
    return DEVICE_DT_GET(DT_NODELABEL(uart0));
}

/* Claim a DMA chunk before leaving the critical section. The callback and
   managed thread may both reach this path after a queue transition. */
static void start_tx(void) {
    unsigned int key = irq_lock();
    if (!state.open || state.tx_active || state.tx_get == state.tx_put) {
        irq_unlock(key);
        return;
    }
    uint32_t count = state.tx_put - state.tx_get;
    if (count > DMA_SIZE) count = DMA_SIZE;
    for (uint32_t i = 0; i < count; ++i)
        state.tx_dma[i] = state.tx[(state.tx_get + i) & (state.tx_size - 1u)];
    state.tx_dma_len = count;
    state.tx_active = 1;
    irq_unlock(key);
    if (uart_tx(uart_device(), state.tx_dma, count, SYS_FOREVER_US) != 0) {
        key = irq_lock();
        state.tx_active = 0;
        state.tx_dma_len = 0;
        irq_unlock(key);
        (void)bmx_embedded_event_post(state.error_token, 8u, 0u);
    }
}

static void uart_callback(const struct device *device, struct uart_event *event,
        void *user_data) {
    (void)user_data;
    uint32_t token = 0;
    uint32_t data = 0;
    uint8_t start_next = 0;
    switch (event->type) {
    case UART_RX_RDY: {
        unsigned int key = irq_lock();
        uint8_t was_empty = state.rx_put == state.rx_get;
        for (size_t i = 0; i < event->data.rx.len; ++i) {
            if (state.rx_put - state.rx_get >= state.rx_size) {
                ++state.dropped;
            } else {
                state.rx[state.rx_put++ & (state.rx_size - 1u)] =
                    event->data.rx.buf[event->data.rx.offset + i];
            }
        }
        if (was_empty && state.rx_put != state.rx_get) token = state.rx_token;
        irq_unlock(key);
        break;
    }
    case UART_RX_BUF_REQUEST:
        for (int i = 0; i < 2; ++i) {
            if (!state.rx_dma_busy[i]) {
                state.rx_dma_busy[i] = 1;
                if (uart_rx_buf_rsp(device, state.rx_dma[i], DMA_SIZE) != 0)
                    state.rx_dma_busy[i] = 0;
                break;
            }
        }
        break;
    case UART_RX_BUF_RELEASED:
        for (int i = 0; i < 2; ++i)
            if (event->data.rx_buf.buf == state.rx_dma[i])
                state.rx_dma_busy[i] = 0;
        break;
    case UART_RX_DISABLED:
        state.rx_disabled = 1;
        if (state.open) {
            for (int i = 0; i < 2; ++i) {
                if (!state.rx_dma_busy[i]) {
                    state.rx_dma_busy[i] = 1;
                    if (uart_rx_enable(device, state.rx_dma[i], DMA_SIZE,
                            1000) != 0) state.rx_dma_busy[i] = 0;
                    else state.rx_disabled = 0;
                    break;
                }
            }
        }
        break;
    case UART_RX_STOPPED:
        token = state.error_token;
        if (event->data.rx_stop.reason & UART_ERROR_FRAMING) data |= 1u;
        if (event->data.rx_stop.reason & UART_ERROR_PARITY) data |= 2u;
        if (event->data.rx_stop.reason & UART_BREAK) data |= 4u;
        if (event->data.rx_stop.reason & UART_ERROR_OVERRUN) data |= 8u;
        break;
    case UART_TX_DONE:
    case UART_TX_ABORTED: {
        unsigned int key = irq_lock();
        uint32_t completed = event->data.tx.len;
        if (completed > state.tx_dma_len) completed = state.tx_dma_len;
        state.tx_get += completed;
        state.tx_dma_len = 0;
        state.tx_active = 0;
        if (state.tx_get == state.tx_put) token = state.tx_token;
        else start_next = 1;
        if (event->type == UART_TX_ABORTED) {
            token = state.error_token;
            data = 8u;
        }
        irq_unlock(key);
        break;
    }
    default:
        break;
    }
    if (token) (void)bmx_embedded_event_post_from_isr(token, data, 0u);
    if (start_next) start_tx();
}

int32_t bmx_embedded_uart_async_open(int32_t controller,
        uint32_t rx_capacity, uint32_t tx_capacity, uint32_t rx_token,
        uint32_t tx_token, uint32_t error_token) {
    extern int32_t bmx_embedded_uart_is_enabled(int32_t);
    if (controller != 0 || !bmx_embedded_uart_is_enabled(controller) ||
            state.open || rx_capacity < 2u || tx_capacity < 2u ||
            rx_capacity > RING_MAX || tx_capacity > RING_MAX ||
            (rx_capacity & (rx_capacity - 1u)) ||
            (tx_capacity & (tx_capacity - 1u)) ||
            !rx_token || !tx_token || !error_token) return 0;
    extern int32_t bmx_nrf52_uart_poll_suspend(void);
    extern int32_t bmx_nrf52_uart_poll_resume(void);
    if (!bmx_nrf52_uart_poll_suspend()) return 0;
    memset(&state, 0, sizeof(state));
    state.rx_size = rx_capacity;
    state.tx_size = tx_capacity;
    state.rx_token = rx_token;
    state.tx_token = tx_token;
    state.error_token = error_token;
    if (uart_callback_set(uart_device(), uart_callback, NULL) != 0) {
        (void)bmx_nrf52_uart_poll_resume();
        return 0;
    }
    state.open = 1;
    state.rx_dma_busy[0] = 1;
    if (uart_rx_enable(uart_device(), state.rx_dma[0], DMA_SIZE, 1000) != 0) {
        state.open = 0;
        state.rx_dma_busy[0] = 0;
        (void)bmx_nrf52_uart_poll_resume();
        return 0;
    }
    return 1;
}

int32_t bmx_embedded_uart_async_close(int32_t controller) {
    if (controller != 0) return 0;
    if (!state.open) return 1;
    state.open = 0;
    state.rx_disabled = 0;
    if (uart_rx_disable(uart_device()) != 0) return 0;
    int64_t deadline = k_uptime_get() + 100;
    while (!state.rx_disabled && k_uptime_get() < deadline) k_usleep(100);
    if (!state.rx_disabled) return 0;
    extern int32_t bmx_nrf52_uart_poll_resume(void);
    return bmx_nrf52_uart_poll_resume();
}

int32_t bmx_embedded_uart_async_is_open(int32_t controller) {
    return controller == 0 && state.open;
}

int32_t bmx_embedded_uart_async_read(int32_t controller, void *destination,
        int32_t capacity) {
    if (controller != 0 || !state.open || capacity < 0 ||
            (capacity && !destination)) return UART_INVALID_ARGUMENT;
    uint8_t *bytes = destination;
    unsigned int key = irq_lock();
    int32_t count = 0;
    while (count < capacity && state.rx_get != state.rx_put)
        bytes[count++] = state.rx[state.rx_get++ & (state.rx_size - 1u)];
    irq_unlock(key);
    return count;
}

int32_t bmx_embedded_uart_async_write(int32_t controller, const void *source,
        int32_t length) {
    if (controller != 0 || !state.open || length < 0 ||
            (length && !source)) return UART_INVALID_ARGUMENT;
    const uint8_t *bytes = source;
    unsigned int key = irq_lock();
    int32_t count = 0;
    while (count < length && state.tx_put - state.tx_get < state.tx_size)
        state.tx[state.tx_put++ & (state.tx_size - 1u)] = bytes[count++];
    irq_unlock(key);
    if (count) start_tx();
    return count;
}

uint32_t bmx_embedded_uart_async_read_available(int32_t controller) {
    if (controller != 0 || !state.open) return 0;
    unsigned int key = irq_lock();
    uint32_t count = state.rx_put - state.rx_get;
    irq_unlock(key);
    return count;
}

uint32_t bmx_embedded_uart_async_write_available(int32_t controller) {
    if (controller != 0 || !state.open) return 0;
    unsigned int key = irq_lock();
    uint32_t count = state.tx_size - (state.tx_put - state.tx_get);
    irq_unlock(key);
    return count;
}

uint32_t bmx_embedded_uart_async_rx_dropped(int32_t controller) {
    if (controller != 0) return 0;
    unsigned int key = irq_lock();
    uint32_t count = state.dropped;
    irq_unlock(key);
    return count;
}

int32_t bmx_embedded_uart_async_tx_idle(int32_t controller) {
    if (controller != 0 || !state.open) return 1;
    unsigned int key = irq_lock();
    int32_t idle = !state.tx_active && state.tx_put == state.tx_get;
    irq_unlock(key);
    return idle;
}
