#include <stdint.h>

#include "blitzmax/embedded_events.h"
#include "blitzmax/embedded_system.h"
#include <zephyr/irq.h>
#include <zephyr/kernel.h>

#define BMX_NRF52_EVENT_CAPACITY 32u
#define BMX_NRF52_EVENT_MASK (BMX_NRF52_EVENT_CAPACITY - 1u)

typedef struct {
    uint32_t token;
    uint32_t data;
    uint32_t mods;
    uint32_t x;
    uint32_t y;
} BMXNrf52Event;

static BMXNrf52Event events[BMX_NRF52_EVENT_CAPACITY];
static uint32_t event_put;
static uint32_t event_get;
static uint32_t event_dropped;
K_SEM_DEFINE(event_signal, 0, 1);

static int32_t post_event(uint32_t token, uint32_t data, uint32_t mods,
        uint32_t x, uint32_t y) {
    if (!token) return 0;
    unsigned int key = irq_lock();
    if (event_put - event_get >= BMX_NRF52_EVENT_CAPACITY) {
        if (event_dropped != UINT32_MAX) ++event_dropped;
        irq_unlock(key);
        return 0;
    }
    events[event_put & BMX_NRF52_EVENT_MASK] =
        (BMXNrf52Event){token, data, mods, x, y};
    ++event_put;
    irq_unlock(key);
    k_sem_give(&event_signal);
    return 1;
}

int32_t bmx_embedded_event_post(uint32_t token, uint32_t data, uint32_t detail) {
    return post_event(token, data, 0, detail, 0);
}

int32_t bmx_embedded_event_post_ex(uint32_t token, uint32_t data,
        uint32_t mods, uint32_t x, uint32_t y) {
    return post_event(token, data, mods, x, y);
}

int32_t bmx_embedded_event_post_from_isr(uint32_t token, uint32_t data,
        uint32_t detail) {
    return post_event(token, data, 0, detail, 0);
}

int32_t bmx_embedded_event_post_from_isr_ex(uint32_t token, uint32_t data,
        uint32_t mods, uint32_t x, uint32_t y) {
    return post_event(token, data, mods, x, y);
}

int32_t bmx_embedded_event_take(uint32_t *token, uint32_t *data,
        uint32_t *mods, uint32_t *x, uint32_t *y) {
    if (!token || !data || !mods || !x || !y) return 0;
    unsigned int key = irq_lock();
    if (event_get == event_put) {
        irq_unlock(key);
        return 0;
    }
    BMXNrf52Event event = events[event_get & BMX_NRF52_EVENT_MASK];
    ++event_get;
    irq_unlock(key);
    *token = event.token;
    *data = event.data;
    *mods = event.mods;
    *x = event.x;
    *y = event.y;
    return 1;
}

uint32_t bmx_embedded_event_pending(void) {
    unsigned int key = irq_lock();
    uint32_t count = event_put - event_get;
    irq_unlock(key);
    return count;
}

uint32_t bmx_embedded_event_dropped(void) {
    unsigned int key = irq_lock();
    uint32_t count = event_dropped;
    irq_unlock(key);
    return count;
}

void bmx_embedded_system_wait(void) {
    if (bmx_embedded_event_pending()) return;
    (void)k_sem_take(&event_signal, K_FOREVER);
}

void bmx_embedded_system_wake(void) {
    k_sem_give(&event_signal);
}
