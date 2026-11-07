/* Copyright (c) 2026 Bruce A Henderson and contributors
 * SPDX-License-Identifier: Zlib */
#include <openthread.h>

int bmx_thread_platform_initialize(void) {
    return openthread_get_default_instance() ? 0 : -19;
}

otInstance *bmx_thread_platform_instance(void) {
    return openthread_get_default_instance();
}

void bmx_thread_platform_lock(void) {
    openthread_mutex_lock();
}

void bmx_thread_platform_unlock(void) {
    openthread_mutex_unlock();
}
