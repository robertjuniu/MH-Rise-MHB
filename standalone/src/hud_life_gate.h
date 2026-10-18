// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <stdint.h>
struct MhrHudLifeGate { uint64_t faint, revive; uint32_t phase, blocked_sequence; };
static inline int mhr_life_visible(struct MhrHudLifeGate *gate, uint64_t now,
                                   uint64_t faint, uint64_t revive, uint32_t sequence) {
    if (faint && faint!=gate->faint) { gate->faint=faint;gate->phase=1; }
    if (!gate->phase) return 1;
    if (gate->phase==1) {
        if (now<gate->faint || now-gate->faint<5ULL*19200000) return 1;
        gate->phase=2;gate->revive=revive;gate->blocked_sequence=sequence;
        return 0;
    }
    if (revive<gate->faint || revive!=gate->revive) {
        gate->revive=revive;gate->blocked_sequence=sequence;return 0;
    }
    if (!sequence || sequence==gate->blocked_sequence) return 0;
    gate->phase=0;
    return 1;
}
