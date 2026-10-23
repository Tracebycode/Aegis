#include "aegis/common/hash.hpp"
#include <cstring>

namespace aegis {

uint64_t murmur_hash3_64(const void* key, size_t len, uint64_t seed) noexcept {
    const uint64_t m = 0xc6a4a7935bd1e995ULL;
    const int r = 47;

    uint64_t h = seed ^ (static_cast<uint64_t>(len) * m);

    const auto* data = static_cast<const uint8_t*>(key);
    const size_t nblocks = len / 8;

    for (size_t i = 0; i < nblocks; ++i) {
        uint64_t k = 0;
        std::memcpy(&k, data + (i * 8), sizeof(uint64_t));

        k *= m;
        k ^= k >> r;
        k *= m;

        h ^= k;
        h *= m;
    }

    const auto* tail = data + (nblocks * 8);
    switch (len & 7) {
    case 7:
        h ^= static_cast<uint64_t>(tail[6]) << 48;
        [[fallthrough]];
    case 6:
        h ^= static_cast<uint64_t>(tail[5]) << 40;
        [[fallthrough]];
    case 5:
        h ^= static_cast<uint64_t>(tail[4]) << 32;
        [[fallthrough]];
    case 4:
        h ^= static_cast<uint64_t>(tail[3]) << 24;
        [[fallthrough]];
    case 3:
        h ^= static_cast<uint64_t>(tail[2]) << 16;
        [[fallthrough]];
    case 2:
        h ^= static_cast<uint64_t>(tail[1]) << 8;
        [[fallthrough]];
    case 1:
        h ^= static_cast<uint64_t>(tail[0]);
        h *= m;
        break;
    default:
        break;
    }

    h ^= h >> r;
    h *= m;
    h ^= h >> r;

    return h;
}

} // namespace aegis
