#pragma once

#include <chrono>
#include <cstdint>

namespace aegis {

using Clock = std::chrono::steady_clock;
using TimePoint = Clock::time_point;
using SystemClock = std::chrono::system_clock;
using SystemTimePoint = SystemClock::time_point;

inline uint64_t now_ms() noexcept {
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            Clock::now().time_since_epoch()
        ).count()
    );
}

inline uint64_t now_us() noexcept {
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(
            Clock::now().time_since_epoch()
        ).count()
    );
}

inline uint64_t now_ns() noexcept {
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            Clock::now().time_since_epoch()
        ).count()
    );
}

inline uint64_t system_now_ms() noexcept {
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            SystemClock::now().time_since_epoch()
        ).count()
    );
}

} // namespace aegis
