#include "aegis/storage/shard.hpp"

namespace aegis {

Result<std::string> Shard::get(std::string_view key, uint64_t now_ms) {
    {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        auto it = table_.find(std::string(key));
        if (it == table_.end()) {
            return Status::not_found();
        }
        if (!it->second.is_expired(now_ms)) {
            return it->second.data();
        }
    }

    // Lazy eviction
    std::unique_lock<std::shared_mutex> lock(mutex_);
    auto it = table_.find(std::string(key));
    if (it != table_.end() && it->second.is_expired(now_ms)) {
        table_.erase(it);
    }
    return Status::not_found();
}

void Shard::set(std::string key, Value value) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    table_[std::move(key)] = std::move(value);
}

bool Shard::del(std::string_view key) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    return table_.erase(std::string(key)) > 0;
}

bool Shard::exists(std::string_view key, uint64_t now_ms) {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    auto it = table_.find(std::string(key));
    if (it == table_.end()) {
        return false;
    }
    return !it->second.is_expired(now_ms);
}

bool Shard::expire(std::string_view key, uint64_t expire_at_ms, uint64_t now_ms) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    auto it = table_.find(std::string(key));
    if (it == table_.end()) {
        return false;
    }
    if (it->second.is_expired(now_ms)) {
        table_.erase(it);
        return false;
    }
    it->second.set_expire_at_ms(expire_at_ms);
    return true;
}

int64_t Shard::ttl_ms(std::string_view key, uint64_t now_ms) {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    auto it = table_.find(std::string(key));
    if (it == table_.end()) {
        return -2;
    }
    return it->second.ttl_ms(now_ms);
}

int64_t Shard::ttl_sec(std::string_view key, uint64_t now_ms) {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    auto it = table_.find(std::string(key));
    if (it == table_.end()) {
        return -2;
    }
    return it->second.ttl_sec(now_ms);
}

bool Shard::persist(std::string_view key, uint64_t now_ms) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    auto it = table_.find(std::string(key));
    if (it == table_.end() || it->second.is_expired(now_ms)) {
        return false;
    }
    if (it->second.has_expiry()) {
        it->second.set_expire_at_ms(0);
        return true;
    }
    return false;
}

size_t Shard::size(uint64_t now_ms) {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    size_t count = 0;
    for (const auto& [_, val] : table_) {
        if (!val.is_expired(now_ms)) {
            ++count;
        }
    }
    return count;
}

void Shard::clear() {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    table_.clear();
}

void Shard::keys(std::vector<std::string>& out, uint64_t now_ms) {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    for (const auto& [k, val] : table_) {
        if (!val.is_expired(now_ms)) {
            out.push_back(k);
        }
    }
}

size_t Shard::purge_expired(uint64_t now_ms, size_t max_sample) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    size_t evicted = 0;
    size_t sampled = 0;

    for (auto it = table_.begin(); it != table_.end() && sampled < max_sample; ) {
        ++sampled;
        if (it->second.is_expired(now_ms)) {
            it = table_.erase(it);
            ++evicted;
        } else {
            ++it;
        }
    }
    return evicted;
}

void Shard::snapshot(std::vector<std::pair<std::string, Value>>& out, uint64_t now_ms) {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    for (const auto& [k, v] : table_) {
        if (!v.is_expired(now_ms)) {
            out.emplace_back(k, v);
        }
    }
}

} // namespace aegis
