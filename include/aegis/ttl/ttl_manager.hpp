#pragma once

#include "aegis/storage/sharded_hash_map.hpp"

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <thread>

namespace aegis {

class TtlManager {
public:
    explicit TtlManager(ShardedHashMap& storage,
                        std::chrono::milliseconds interval = std::chrono::milliseconds(100),
                        size_t sample_size = 20);
    ~TtlManager();

    TtlManager(const TtlManager&) = delete;
    TtlManager& operator=(const TtlManager&) = delete;

    void start();
    void stop();

    [[nodiscard]] bool is_running() const noexcept { return is_running_.load(); }
    [[nodiscard]] uint64_t total_evicted() const noexcept { return total_evicted_.load(); }
    [[nodiscard]] uint64_t total_cycles() const noexcept { return total_cycles_.load(); }

    // Run a single synchronous active expiration cycle (useful for tests and manual triggers)
    size_t run_cycle();

private:
    void worker_loop(std::stop_token stop_token);

    ShardedHashMap& storage_;
    std::chrono::milliseconds interval_;
    size_t sample_size_;
    std::atomic<bool> is_running_{false};
    std::atomic<uint64_t> total_evicted_{0};
    std::atomic<uint64_t> total_cycles_{0};
    std::jthread worker_thread_;
};

} // namespace aegis
