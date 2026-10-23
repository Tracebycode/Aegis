#include "aegis/storage/sharded_hash_map.hpp"

#include <bit>

namespace aegis {

namespace {

// Glob pattern matching helper for keys
bool pattern_match(std::string_view pattern, std::string_view str) {
    size_t p = 0, s = 0;
    size_t p_star = std::string_view::npos, s_star = 0;

    while (s < str.size()) {
        if (p < pattern.size() && (pattern[p] == '?' || pattern[p] == str[s])) {
            ++p;
            ++s;
        } else if (p < pattern.size() && pattern[p] == '*') {
            p_star = p++;
            s_star = s;
        } else if (p_star != std::string_view::npos) {
            p = p_star + 1;
            s = ++s_star;
        } else {
            return false;
        }
    }

    while (p < pattern.size() && pattern[p] == '*') {
        ++p;
    }

    return p == pattern.size();
}

size_t next_power_of_two(size_t n) {
    if (n <= 1) return 1;
    return std::bit_ceil(n);
}

} // namespace

ShardedHashMap::ShardedHashMap(size_t num_shards)
    : num_shards_(next_power_of_two(num_shards)),
      shard_mask_(num_shards_ - 1) {
    shards_.reserve(num_shards_);
    for (size_t i = 0; i < num_shards_; ++i) {
        shards_.push_back(std::make_unique<Shard>());
    }
}

Result<std::string> ShardedHashMap::get(std::string_view key) {
    return get_shard(key).get(key, now_ms());
}

void ShardedHashMap::set(std::string key, std::string val, uint64_t expire_at_ms) {
    uint64_t now = now_ms();
    Value v(std::move(val), expire_at_ms, now);
    get_shard(key).set(std::move(key), std::move(v));
}

bool ShardedHashMap::del(std::string_view key) {
    return get_shard(key).del(key);
}

size_t ShardedHashMap::del(const std::vector<std::string>& keys) {
    size_t count = 0;
    for (const auto& key : keys) {
        if (del(key)) {
            ++count;
        }
    }
    return count;
}

bool ShardedHashMap::exists(std::string_view key) {
    return get_shard(key).exists(key, now_ms());
}

bool ShardedHashMap::expire(std::string_view key, uint64_t seconds) {
    uint64_t now = now_ms();
    uint64_t expire_at = now + (seconds * 1000);
    return get_shard(key).expire(key, expire_at, now);
}

bool ShardedHashMap::pexpire(std::string_view key, uint64_t milliseconds) {
    uint64_t now = now_ms();
    uint64_t expire_at = now + milliseconds;
    return get_shard(key).expire(key, expire_at, now);
}

int64_t ShardedHashMap::ttl(std::string_view key) {
    return get_shard(key).ttl_sec(key, now_ms());
}

int64_t ShardedHashMap::pttl(std::string_view key) {
    return get_shard(key).ttl_ms(key, now_ms());
}

bool ShardedHashMap::persist(std::string_view key) {
    return get_shard(key).persist(key, now_ms());
}

size_t ShardedHashMap::dbsize() {
    uint64_t now = now_ms();
    size_t total = 0;
    for (auto& shard : shards_) {
        total += shard->size(now);
    }
    return total;
}

void ShardedHashMap::flushall() {
    for (auto& shard : shards_) {
        shard->clear();
    }
}

std::vector<std::string> ShardedHashMap::keys(std::string_view pattern) {
    uint64_t now = now_ms();
    std::vector<std::string> all_keys;
    for (auto& shard : shards_) {
        shard->keys(all_keys, now);
    }

    if (pattern == "*") {
        return all_keys;
    }

    std::vector<std::string> filtered;
    for (const auto& k : all_keys) {
        if (pattern_match(pattern, k)) {
            filtered.push_back(k);
        }
    }
    return filtered;
}

size_t ShardedHashMap::purge_expired(size_t max_keys_per_shard) {
    uint64_t now = now_ms();
    size_t total_evicted = 0;
    for (auto& shard : shards_) {
        total_evicted += shard->purge_expired(now, max_keys_per_shard);
    }
    return total_evicted;
}

std::vector<std::pair<std::string, Value>> ShardedHashMap::get_snapshot() {
    uint64_t now = now_ms();
    std::vector<std::pair<std::string, Value>> snapshot;
    for (auto& shard : shards_) {
        shard->snapshot(snapshot, now);
    }
    return snapshot;
}

void ShardedHashMap::load_snapshot(const std::vector<std::pair<std::string, Value>>& entries) {
    for (const auto& [k, v] : entries) {
        get_shard(k).set(k, v);
    }
}

} // namespace aegis
