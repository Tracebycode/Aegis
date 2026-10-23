#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace aegis {

// MurmurHash3 64-bit finalizer / implementation for string keys
uint64_t murmur_hash3_64(const void* key, size_t len, uint64_t seed = 0xc70f6907UL) noexcept;

inline uint64_t hash_key(std::string_view key, uint64_t seed = 0xc70f6907UL) noexcept {
    return murmur_hash3_64(key.data(), key.size(), seed);
}

} // namespace aegis
