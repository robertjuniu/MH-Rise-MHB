// SPDX-License-Identifier: GPL-2.0-or-later
#include <stdint.h>
#include <stddef.h>
#include "capture_code.h"

struct MemoryInfo {
    uint64_t address, size;
    uint32_t type, attributes, permission, ipc_refs, device_refs, reserved;
};
struct CaptureState {
    uint64_t health_pair, vitality;
    uint32_t sequence, species;
    uint64_t physical;
};
static struct CaptureState state __attribute__((aligned(16)));
#ifndef MHR_RENDER
static unsigned char observer_stack[8192] __attribute__((aligned(4096)));
#endif
extern uint32_t mhr_query_memory(struct MemoryInfo *, uint32_t *, uintptr_t);
extern uint32_t mhr_get_info(uint64_t *, uint32_t, uint32_t, uint64_t);
extern uint32_t mhr_map_process(uintptr_t, uint32_t, uintptr_t, uint64_t);
extern uint32_t mhr_unmap_process(uintptr_t, uint32_t, uintptr_t, uint64_t);
extern uint32_t mhr_close_handle(uint32_t);
extern uint32_t mhr_create_thread(uint32_t *, void (*)(void *), void *, void *, int, int);
extern uint32_t mhr_start_thread(uint32_t);
extern void mhr_sleep(int64_t);
extern void mhr_cache_sync(uintptr_t, uintptr_t, size_t);
extern uint32_t mhr_debug_string(const char *, size_t);
extern void mhr_log_line(const char *);
extern void mhr_log_address(const char *, uintptr_t);

struct HpPacket { float current, maximum; uint64_t target; uint32_t sequence, species; };
int mhr_read_hp(struct HpPacket *packet) {
    const uint32_t sequence = __atomic_load_n(&state.sequence, __ATOMIC_ACQUIRE);
    if (!sequence || (sequence & 1)) return 0;
    union { uint64_t bits; float health[2]; } pair;
    pair.bits = __atomic_load_n(&state.health_pair, __ATOMIC_RELAXED);
    packet->current = pair.health[0];
    packet->maximum = pair.health[1];
    packet->target = __atomic_load_n(&state.vitality, __ATOMIC_RELAXED);
    packet->sequence = sequence;
    packet->species = __atomic_load_n(&state.species, __ATOMIC_RELAXED);
    __atomic_thread_fence(__ATOMIC_ACQUIRE);
    return sequence == __atomic_load_n(&state.sequence, __ATOMIC_RELAXED) &&
        packet->current >= 0 && packet->maximum > 0 && packet->current <= packet->maximum;
}

#ifndef MHR_RENDER
static size_t append_text(char *line, size_t index, const char *text) {
    while (*text) line[index++] = *text++;
    return index;
}
static size_t append_uint(char *line, size_t index, uint32_t value) {
    char reverse[10];
    size_t count = 0;
    do { reverse[count++] = (char)('0' + value % 10); value /= 10; } while (value);
    while (count) line[index++] = reverse[--count];
    return index;
}
static void observe(void *unused) {
    (void)unused;
    mhr_log_line("[MHR-Patch] observer thread entered");
    uint32_t previous = 0;
    unsigned polls = 0;
    for (;;) {
        uint32_t sequence = __atomic_load_n(&state.sequence, __ATOMIC_ACQUIRE);
        if (!(polls++ % 20)) {
            mhr_log_address("[MHR-Patch] observer heartbeat sequence=0x", sequence);
            mhr_log_address("[MHR-Patch] observer health bits=0x", __atomic_load_n(&state.health_pair, __ATOMIC_RELAXED));
        }
        if (sequence && !(sequence & 1) && sequence != previous) {
            union { uint64_t bits; float health[2]; } pair;
            pair.bits = __atomic_load_n(&state.health_pair, __ATOMIC_RELAXED);
            uint32_t species = __atomic_load_n(&state.species, __ATOMIC_RELAXED);
            __atomic_thread_fence(__ATOMIC_ACQUIRE);
            if (__atomic_load_n(&state.sequence, __ATOMIC_RELAXED) == sequence &&
                pair.health[0] >= 0 && pair.health[1] > 0 &&
                pair.health[0] <= pair.health[1] && pair.health[1] < 1000000000.0f) {
                char line[160];
                size_t size = append_text(line, 0, "[MHR-Patch] capture seq=");
                size = append_uint(line, size, sequence);
                size = append_text(line, size, " hp=");
                size = append_uint(line, size, (uint32_t)pair.health[0]);
                size = append_text(line, size, " max=");
                size = append_uint(line, size, (uint32_t)pair.health[1]);
                size = append_text(line, size, " species=");
                size = append_uint(line, size, species);
                mhr_debug_string(line, size);
                previous = sequence;
            }
        }
        mhr_sleep(1000000000);
    }
}

#endif

int mhr_install_capture(uintptr_t main_address) {
    const uintptr_t hook = main_address + 0x0501adb0;
    const uintptr_t cave = main_address + 0x07d46d00;
    if (*(const uint32_t *)hook != 0xbd001340 ||
        *(const uint32_t *)(hook + 4) != 0x340007db) return 0;
    for (size_t i = 0; i < sizeof(mhr_capture_code); ++i)
        if (((const unsigned char *)cave)[i]) return 0;
    uint64_t process = 0, aslr = 0, aslr_size = 0;
    if (mhr_get_info(&process, 65001, 0, 0)) {
        mhr_log_line("[MHR-Patch] process handle unavailable; game left unchanged");
        return 0;
    }
    if (mhr_get_info(&aslr, 12, 0xffff8001, 0) ||
        mhr_get_info(&aslr_size, 13, 0xffff8001, 0)) {
        mhr_close_handle((uint32_t)process);
        return 0;
    }
    uintptr_t alias = 0, cursor = aslr;
    for (unsigned i = 0; i < 4096 && cursor < aslr + aslr_size; ++i) {
        struct MemoryInfo memory = {0};
        uint32_t page;
        if (mhr_query_memory(&memory, &page, cursor)) break;
        uintptr_t candidate = (cursor + 0xfff) & ~(uintptr_t)0xfff;
        if ((memory.type & 0xff) == 0 && candidate + 0x2000 <= memory.address + memory.size &&
            candidate + 0x2000 <= aslr + aslr_size &&
            !mhr_map_process(candidate, (uint32_t)process, hook & ~(uintptr_t)0xfff, 0x1000)) {
            if (!mhr_map_process(candidate + 0x1000, (uint32_t)process, cave & ~(uintptr_t)0xfff, 0x1000)) {
                alias = candidate;
                break;
            }
            mhr_unmap_process(candidate, (uint32_t)process, hook & ~(uintptr_t)0xfff, 0x1000);
        }
        uintptr_t next = memory.address + memory.size;
        if (next <= cursor) break;
        cursor = next;
    }
    if (!alias) {
        mhr_log_line("[MHR-Patch] writable code alias unavailable; game left unchanged");
        mhr_close_handle((uint32_t)process);
        return 0;
    }
    unsigned char *cave_alias = (void *)(alias + 0x1000 + (cave & 0xfff));
    for (size_t i = 0; i < sizeof(mhr_capture_code); ++i) cave_alias[i] = mhr_capture_code[i];
    *(uint64_t *)(cave_alias + MHR_STATE_LITERAL_OFFSET) = (uintptr_t)&state;
    mhr_cache_sync(cave, (uintptr_t)cave_alias, sizeof(mhr_capture_code));
    *(uint32_t *)(alias + (hook & 0xfff)) = 0x14b4afd4;
    mhr_cache_sync(hook, alias + (hook & 0xfff), 4);
    uint32_t unmap_cave = mhr_unmap_process(alias + 0x1000, (uint32_t)process, cave & ~(uintptr_t)0xfff, 0x1000);
    uint32_t unmap_hook = mhr_unmap_process(alias, (uint32_t)process, hook & ~(uintptr_t)0xfff, 0x1000);
    mhr_close_handle((uint32_t)process);
    mhr_log_address("[MHR-Patch] live hook instruction=0x", *(const uint32_t *)hook);
    mhr_log_address("[MHR-Patch] live state literal=0x", *(const uint64_t *)(cave + MHR_STATE_LITERAL_OFFSET));
    if (*(const uint32_t *)hook != 0x14b4afd4 ||
        *(const uint64_t *)(cave + MHR_STATE_LITERAL_OFFSET) != (uintptr_t)&state) {
        mhr_log_line("[MHR-Patch] code alias writes not visible through original mapping");
        return 0;
    }
    mhr_log_address("[MHR-Patch] independent HP hook installed; state=0x", (uintptr_t)&state);
    if (unmap_cave || unmap_hook) mhr_log_line("[MHR-Patch] code alias cleanup returned an error");
#ifdef MHR_RENDER
    mhr_log_line("[MHR-Patch] independent capture active; diagnostic polling thread disabled");
    return 1;
#else
    uint32_t thread = 0;
    uint32_t result = mhr_create_thread(&thread, observe, 0, observer_stack + sizeof(observer_stack), 0x20, 2);
    if (!result) {
        result = mhr_start_thread(thread);
        mhr_close_handle(thread);
    }
    if (result) {
        mhr_log_line("[MHR-Patch] capture installed; observer thread could not start");
        return 1;
    }
    mhr_log_line("[MHR-Patch] independent capture observer running");
    return 1;
#endif
}
