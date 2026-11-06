#include <setjmp.h>
#include <stdint.h>

#include "blitzmax/embedded_runtime.h"

enum { ROOT_CAPACITY = 56u };
static const BMXEmbeddedTypeDescriptor small_type = {
    .name = "Nrf52SmallBlock", .instance_size = 1024u
};
static const BMXEmbeddedTypeDescriptor too_large_type = {
    .name = "Nrf52TooLargeBlock", .instance_size = 65536u
};

static int catches_oom(void) {
    BMXEmbeddedExceptionFrame frame;
    bmx_embedded_exception_enter(&frame);
    if (setjmp(frame.buffer) == 0) {
        (void)bmx_embedded_object_allocate(&too_large_type);
        bmx_embedded_exception_leave();
        return 0;
    }
    return bmx_embedded_exception_catch().kind == BMX_EMBEDDED_EXCEPTION_STRING;
}

int32_t bmx_nrf52_memory_stress_test(void) {
    if (bmx_embedded_arena_capacity() != 65536u) return 1;
    BMXEmbeddedObject *blocks[ROOT_CAPACITY] = {0};
    BMXEmbeddedRootSlot slots[ROOT_CAPACITY];
    for (uint32_t i = 0; i < ROOT_CAPACITY; ++i)
        slots[i] = (BMXEmbeddedRootSlot){&blocks[i], BMX_EMBEDDED_ROOT_OBJECT, NULL};
    BMXEmbeddedRootFrame frame;
    bmx_embedded_root_frame_enter(&frame, slots, ROOT_CAPACITY);

    uint32_t count = 0;
    while (count < ROOT_CAPACITY && bmx_embedded_arena_remaining() >= 1536u) {
        blocks[count] = bmx_embedded_object_allocate(&small_type);
        if (!blocks[count]) return 2;
        ++count;
    }
    if (count < 20u || !bmx_embedded_heap_integrity_valid()) return 3;
    BMXEmbeddedObject *freed[ROOT_CAPACITY / 2u];
    for (uint32_t i = 0; i < count; i += 2u) freed[i / 2u] = blocks[i];
    for (uint32_t i = 0; i < count; i += 2u) blocks[i] = NULL;
    (void)bmx_embedded_collect_objects();
    if (!bmx_embedded_heap_integrity_valid() ||
        bmx_embedded_heap_reusable_bytes() < count * 400u) return 4;
    uint32_t used_before_reuse = bmx_embedded_arena_used();
    blocks[0] = bmx_embedded_object_allocate(&small_type);
    int reused_gap = 0;
    for (uint32_t i = 0; i < (count + 1u) / 2u; ++i)
        if (blocks[0] == freed[i]) reused_gap = 1;
    if (!reused_gap || bmx_embedded_arena_used() != used_before_reuse ||
        !bmx_embedded_heap_integrity_valid()) return 8;
    if (!catches_oom()) return 5;
    for (uint32_t i = 0; i < count; ++i) blocks[i] = NULL;
    bmx_embedded_root_frame_leave(&frame);
    (void)bmx_embedded_collect_objects();
    if (!bmx_embedded_heap_integrity_valid()) return 6;

    BMXEmbeddedObject *recovery = NULL;
    BMXEmbeddedRootSlot recovery_slot = {
        &recovery, BMX_EMBEDDED_ROOT_OBJECT, NULL
    };
    BMXEmbeddedRootFrame recovery_frame;
    bmx_embedded_root_frame_enter(&recovery_frame, &recovery_slot, 1u);
    recovery = bmx_embedded_object_allocate(&small_type);
    int32_t passed = recovery != NULL && bmx_embedded_heap_integrity_valid();
    recovery = NULL;
    bmx_embedded_root_frame_leave(&recovery_frame);
    return passed ? 0 : 7;
}
