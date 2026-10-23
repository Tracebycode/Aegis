#pragma once

#include "aegis/common/types.hpp"
#include "aegis/storage/value.hpp"

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace aegis {

class alignas(64) Shard {
public:
    Shard() = default;
    ~Shard() = default;

    // Disallow copy/move due to mutex
    Shard(const Shard&) = delete;
    Shard& operator=(const Shard&) = delete;
    Shard(Shard&&) = delete;
    Shard& operator=(Shard&&) = delete;

    Result<std::string> get(std::string_view key, uint64_t now_ms);
    void set(std::string key, Value value);
    bool del(std::string_view key);
    bool exists(std::string_view key, uint64_t now_ms);
    bool expire(std::string_view key, uint64_t expire_at_ms, uint64_t now_ms);
    int64_t ttl_ms(std::string_view key, uint64_t now_ms);
    int64_t ttl_sec(std::string_view key, uint64_t now_ms);
    bool persist(std::string_view key, uint64_t now_ms);

    size_t size(uint64_t now_ms);
    void clear();
    void keys(std::vector<std::string>& out, uint64_t now_ms);
    size_t purge_expired(uint64_t now_ms, size_t max_sample);
    void snapshot(std::vector<std::pair<std::string, Value>>& out, uint64_t now_ms);

private:
    mutable std::shared_mutex mutex_;
    std::unordered_map<std::string, Value> table_;
};

} // namespace aegis
