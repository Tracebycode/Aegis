#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace aegis {

class Value {
public:
    Value() : data_(), expire_at_ms_(0), created_at_ms_(0) {}

    explicit Value(std::string data, uint64_t expire_at_ms = 0, uint64_t created_at_ms = 0)
        : data_(std::move(data)),
          expire_at_ms_(expire_at_ms),
          created_at_ms_(created_at_ms) {}

    [[nodiscard]] const std::string& data() const noexcept { return data_; }
    [[nodiscard]] std::string_view view() const noexcept { return data_; }
    [[nodiscard]] uint64_t expire_at_ms() const noexcept { return expire_at_ms_; }
    [[nodiscard]] uint64_t created_at_ms() const noexcept { return created_at_ms_; }

    void set_data(std::string data) { data_ = std::move(data); }
    void set_expire_at_ms(uint64_t expire_at_ms) noexcept { expire_at_ms_ = expire_at_ms; }

    [[nodiscard]] bool has_expiry() const noexcept {
        return expire_at_ms_ > 0;
    }

    [[nodiscard]] bool is_expired(uint64_t now_ms) const noexcept {
        return has_expiry() && (now_ms >= expire_at_ms_);
    }

    // Returns TTL in milliseconds: -1 if no expiry, -2 if expired
    [[nodiscard]] int64_t ttl_ms(uint64_t now_ms) const noexcept {
        if (!has_expiry()) {
            return -1;
        }
        if (now_ms >= expire_at_ms_) {
            return -2;
        }
        return static_cast<int64_t>(expire_at_ms_ - now_ms);
    }

    // Returns TTL in seconds: -1 if no expiry, -2 if expired
    [[nodiscard]] int64_t ttl_sec(uint64_t now_ms) const noexcept {
        int64_t ms = ttl_ms(now_ms);
        if (ms < 0) return ms;
        return (ms + 999) / 1000;
    }

private:
    std::string data_;
    uint64_t expire_at_ms_{0}; // 0 indicates no expiration
    uint64_t created_at_ms_{0};
};

} // namespace aegis
