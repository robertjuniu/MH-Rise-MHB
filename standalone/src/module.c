// SPDX-License-Identifier: GPL-2.0-or-later
#include <stdint.h>
#include <stddef.h>

struct MemoryInfo {
    uint64_t address, size;
    uint32_t type, attributes, permission, ipc_refs, device_refs, reserved;
};

__attribute__((aligned(16))) unsigned char mhr_module_runtime[0x100];
static volatile uintptr_t main_address;
extern uint32_t mhr_debug_string(const char *, size_t);
extern uint32_t mhr_query_memory(struct MemoryInfo *, uint32_t *, uintptr_t);

void mhr_log_line(const char *message) {
    size_t length = 0;
    while (message[length]) ++length;
    mhr_debug_string(message, length);
}

void mhr_log_address(const char *prefix, uintptr_t address) {
    char line[96];
    size_t length = 0;
    while (prefix[length] && length < 64) {
        line[length] = prefix[length];
        ++length;
    }
    for (int shift = 60; shift >= 0; shift -= 4)
        line[length++] = "0123456789abcdef"[(address >> shift) & 15];
    mhr_debug_string(line, length);
}

__attribute__((visibility("default"))) void mhr_module_init(void) {
#ifdef MHR_RENDER
    extern void (*__init_array_start[])(void), (*__init_array_end[])(void);
    for (void (**function)(void) = __init_array_start; function != __init_array_end; ++function) (*function)();
#endif
    mhr_log_line("[MHR-Patch] standalone ExeFS module initialized");
    uintptr_t cursor = 0;
    for (unsigned regions = 0; regions < 4096; ++regions) {
        struct MemoryInfo memory = {0};
        uint32_t page = 0;
        if (mhr_query_memory(&memory, &page, cursor)) break;
        if (memory.permission == 5 && memory.size >= 0x07d47000) {
            const volatile uint32_t *hook = (void *)(memory.address + 0x0501adb0);
            if (hook[0] == 0xbd001340 && hook[1] == 0x340007db) {
                main_address = memory.address;
                mhr_log_address("[MHR-Patch] original Rise damage signature at main=0x", main_address);
#ifdef MHR_CAPTURE
                extern int mhr_install_capture(uintptr_t);
                if (!mhr_install_capture(main_address)) mhr_log_line("[MHR-Patch] capture not installed");
#ifdef MHR_RENDER
                extern int mhr_install_player_life(uintptr_t);
                if (!mhr_install_player_life(main_address)) mhr_log_line("[MHR-HUD] faint hooks unavailable; regular HP HUD retained");
                extern int mhr_install_renderer(uintptr_t);
                if (!mhr_install_renderer(main_address)) mhr_log_line("[MHR-HUD] renderer not installed");
#endif
#else
                mhr_log_line("[MHR-Patch] no game bytes changed; no Android/JNI calls");
#endif
                return;
            }
        }
        uintptr_t next = memory.address + memory.size;
        if (next <= cursor) break;
        cursor = next;
    }
    mhr_log_line("[MHR-Patch] original signature unavailable; no game bytes changed");
}
