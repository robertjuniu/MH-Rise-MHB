// SPDX-License-Identifier: GPL-2.0-or-later
#include "helpers/memoryHelper.h"
#include "helpers/InputHelper.h"
#include "logger/Logger.hpp"
#include "os/os_tick.hpp"
#include <cstdlib>
#include <cstdio>
#include <cstring>

extern "C" void mhr_log_line(const char *);
extern "C" unsigned mhr_debug_string(const char *, size_t);
extern "C" long long mhr_get_tick();
extern "C" void mhr_sleep(long long);
extern "C" void *aligned_alloc(size_t, size_t);

void Mem::Init() { mhr_log_line("[MHR-HUD] allocator uses exported game functions"); }
void *Mem::Allocate(size_t size) { return malloc(size); }
void *Mem::AllocateAlign(size_t alignment, size_t size) { return aligned_alloc(alignment, (size + alignment - 1) & ~(alignment - 1)); }
void Mem::Deallocate(void *ptr) { free(ptr); }
void *Mem::Reallocate(void *ptr, size_t size) { return realloc(ptr, size); }

Logger &Logger::instance() { static Logger logger; return logger; }
void Logger::forward(LoggerFunc callback) { forwarder = callback; }
void Logger::log(const char *format, ...) {
    va_list args;
    va_start(args, format);
    log(format, args);
    va_end(args);
}
void Logger::log(const char *format, va_list args) {
    char line[512];
    const int size = vsnprintf(line, sizeof(line), format, args);
    if (size > 0) mhr_debug_string(line, size < 512 ? size : 511);
}

ulong InputHelper::selectedPort = 0;
bool InputHelper::toggleInput = false;
bool InputHelper::isReadInput = true;
void InputHelper::initKBM() {}
void InputHelper::updatePadState() {}
bool InputHelper::isButtonPress(nn::hid::NpadButton) { return false; }
bool InputHelper::isButtonHold(nn::hid::NpadButton) { return false; }

namespace nn::os {
Tick GetSystemTick() { return Tick(mhr_get_tick()); }
s64 GetSystemTickFrequency() { return 19200000; }
TimeSpan ConvertToTimeSpan(Tick tick) {
    return TimeSpan(TimeSpanType::FromNanoSeconds((tick.GetInt64Value() / 19200000) * 1000000000 +
        (tick.GetInt64Value() % 19200000) * 1000000000 / 19200000));
}
}

namespace nn::diag::detail {
void AbortImpl(const char *file, const char *function, const char *message, int line) {
    Logger::log("[MHR-HUD] renderer stopped at %s:%d %s: %s", file, line, function, message);
    for (;;) mhr_sleep(1000000000);
}
}
extern "C" __attribute__((noreturn)) void __assert2(const char *file, int line, const char *function, const char *expression) {
    nn::diag::detail::AbortImpl(file, function, expression, line);
    __builtin_unreachable();
}
