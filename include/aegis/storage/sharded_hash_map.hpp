#pragma once

#include "aegis/common/hash.hpp"
#include "aegis/common/time_utils.hpp"
#include "aegis/common/types.hpp"
#include "aegis/storage/shard.hpp"
#include "aegis/storage/value.hpp"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace aegis {

class ShardedHashMap {
public:
    static constexpr size_t DEFAULT_SHARDS = 64;

    explicit ShardedHashMap(size_t num_shards = DEFAULT_SHARDS);
    ~ShardedHashMap() = default;

    ShardedHashMap(const ShardedHashMap&) = delete;
    ShardedHashMap& operator=(const ShardedHashMap&) = delete;

    Result<std::string> get(std::string_view key);
    void set(std::string key, std::string val, uint64_t expire_at_ms = 0);
    bool del(std::string_view key);
    size_t del(const std::vector<std::string>& keys);
    bool exists(std::string_view key);
    bool expire(std::string_view key, uint64_t seconds);
    bool pexpire(std::string_view key, uint64_t milliseconds);
    int64_t ttl(std::string_view key);
    int64_t pttl(std::string_view key);
    bool persist(std::string_view key);

    size_t dbsize();
    void flushall();
    std::vector<std::string> keys(std::string_view pattern = "*");

    // TTL active purge
    size_t purge_expired(size_t max_keys_per_shard = 20);

    // Persistence snapshots
    std::vector<std::pair<std::string, Value>> get_snapshot();
    void load_snapshot(const std::vector<std::pair<std::string, Value>>& entries);

    [[nodiscard]] size_t shard_count() const noexcept { return num_shards_; }

private:
    [[nodiscard]] size_t shard_index(std::string_view key) const noexcept {
        return hash_key(key) & shard_mask_;
    }

    [[nodiscard]] Shard& get_shard(std::string_view key) noexcept {
        return *shards_[shard_index(key)];
    }

    size_t num_shards_;
    size_t shard_mask_;
    std::vector<std::unique_ptr<Shard>> shards_;
};

} // namespace aegis
