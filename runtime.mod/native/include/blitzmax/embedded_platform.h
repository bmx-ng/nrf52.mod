#ifndef BLITZMAX_EMBEDDED_PLATFORM_H
#define BLITZMAX_EMBEDDED_PLATFORM_H

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

void bmx_nrf52_managed_init(void);
int bmx_nrf52_context_valid(void);

#define BMX_EMBEDDED_PLATFORM_CONTEXT_VALID() bmx_nrf52_context_valid()
#define BMX_EMBEDDED_PLATFORM_PANIC(message) do { \
    printk("BlitzMax runtime error: %s\n", (message)); \
    k_panic(); \
} while (0)
#define BMX_EMBEDDED_ARENA_STORAGE(name) name

#endif
