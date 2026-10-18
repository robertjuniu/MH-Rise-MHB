// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <stdint.h>

// NVN wait results: 0/1 complete, 2 timeout, 3 failure.
// Never allow CPU writes into memory still consumed by the GPU.
typedef int (*MhrWaitForFrame)(void *, uint64_t);
static inline int mhr_acquire_frame(unsigned *pending, void *context,
                                   MhrWaitForFrame wait, unsigned *blocked) {
    *blocked = 0;
    if (!*pending) return 1;
    int result = wait(context, 0);
    if (result == 2) {
        *blocked = 1;
        result = wait(context, 1000000000ULL);
    }
    if (result != 0 && result != 1) return 0;
    *pending = 0;
    return 1;
}
