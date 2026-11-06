#include <stddef.h>
#include <stdint.h>

#include "blitzmax/embedded_random.h"
#include <zephyr/kernel.h>
#include <zephyr/random/random.h>

uint32_t bmx_embedded_random_uint32(void) {
    uint32_t value = 0;
    if (sys_csrand_get(&value, sizeof(value)) != 0) k_panic();
    return value;
}

uint64_t bmx_embedded_random_uint64(void) {
    uint64_t value = 0;
    if (sys_csrand_get(&value, sizeof(value)) != 0) k_panic();
    return value;
}

int32_t bmx_embedded_random_fill(void *buffer, int32_t length) {
    if (length < 0 || (length > 0 && !buffer)) return 0;
    if (length == 0) return 1;
    return sys_csrand_get(buffer, (size_t)length) == 0;
}
