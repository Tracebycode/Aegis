#include "aegis/ttl/ttl_manager.hpp"

namespace aegis {

TtlManager::TtlManager(ShardedHashMap& storage,
                       std::chrono::milliseconds interval,
                       size_t sample_size)
    : storage_(storage),
      interval_(interval),
      sample_size_(sample_size) {}

TtlManager::~TtlManager() {
    stop();
}

void TtlManager::start() {
    if (is_running_.exchange(true)) {
        return;
    }

    worker_thread_ = std::jthread([this](std::stop_token stop_token) {
        worker_loop(stop_token);
    });
}

void TtlManager::stop() {
    if (!is_running_.exchange(false)) {
        return;
    }

    if (worker_thread_.joinable()) {
        worker_thread_.request_stop();
        worker_thread_.join();
    }
}

size_t TtlManager::run_cycle() {
    size_t evicted = storage_.purge_expired(sample_size_);
    total_evicted_.fetch_add(evicted, std::memory_order_relaxed);
    total_cycles_.fetch_add(1, std::memory_order_relaxed);
    return evicted;
}

void TtlManager::worker_loop(std::stop_token stop_token) {
    while (!stop_token.stop_requested()) {
        run_cycle();

        // Sleep for interval in small increments to respond quickly to stop requests
        auto sleep_remaining = interval_;
        auto step = std::chrono::milliseconds(20);
        while (sleep_remaining > std::chrono::milliseconds(0) && !stop_token.stop_requested()) {
            auto current_sleep = std::min(sleep_remaining, step);
            std::this_thread::sleep_for(current_sleep);
            sleep_remaining -= current_sleep;
        }
    }
}

} // namespace aegis
