#include "aegis/persistence/snapshot.hpp"

#include <cstdio>
#include <fstream>
#include <vector>

namespace aegis {

namespace {

constexpr const char* MAGIC_HEADER = "AEGIS_RDB_V1\n";
constexpr const char* EOF_MARKER = "AEGIS_EOF\n";

} // namespace

bool SnapshotManager::save(const std::string& filepath, ShardedHashMap& storage) {
    std::string tmp_filepath = filepath + ".tmp";
    std::ofstream out(tmp_filepath, std::ios::binary | std::ios::trunc);
    if (!out.is_open()) {
        return false;
    }

    out.write(MAGIC_HEADER, std::string_view(MAGIC_HEADER).size());

    auto entries = storage.get_snapshot();
    uint64_t count = entries.size();
    out.write(reinterpret_cast<const char*>(&count), sizeof(count));

    for (const auto& [key, value] : entries) {
        uint64_t key_len = key.size();
        out.write(reinterpret_cast<const char*>(&key_len), sizeof(key_len));
        out.write(key.data(), static_cast<std::streamsize>(key_len));

        uint64_t val_len = value.data().size();
        out.write(reinterpret_cast<const char*>(&val_len), sizeof(val_len));
        out.write(value.data().data(), static_cast<std::streamsize>(val_len));

        uint64_t expire_at_ms = value.expire_at_ms();
        out.write(reinterpret_cast<const char*>(&expire_at_ms), sizeof(expire_at_ms));
    }

    out.write(EOF_MARKER, std::string_view(EOF_MARKER).size());
    out.flush();
    out.close();

    // Atomic rename
    return (std::rename(tmp_filepath.c_str(), filepath.c_str()) == 0);
}

bool SnapshotManager::load(const std::string& filepath, ShardedHashMap& storage) {
    std::ifstream in(filepath, std::ios::binary);
    if (!in.is_open()) {
        return false;
    }

    std::string header(std::string_view(MAGIC_HEADER).size(), '\0');
    in.read(header.data(), static_cast<std::streamsize>(header.size()));
    if (header != MAGIC_HEADER) {
        return false;
    }

    uint64_t count = 0;
    in.read(reinterpret_cast<char*>(&count), sizeof(count));
    if (!in.good()) {
        return false;
    }

    std::vector<std::pair<std::string, Value>> entries;
    entries.reserve(count);

    uint64_t now = now_ms();

    for (uint64_t i = 0; i < count; ++i) {
        uint64_t key_len = 0;
        in.read(reinterpret_cast<char*>(&key_len), sizeof(key_len));
        std::string key(key_len, '\0');
        in.read(key.data(), static_cast<std::streamsize>(key_len));

        uint64_t val_len = 0;
        in.read(reinterpret_cast<char*>(&val_len), sizeof(val_len));
        std::string val(val_len, '\0');
        in.read(val.data(), static_cast<std::streamsize>(val_len));

        uint64_t expire_at_ms = 0;
        in.read(reinterpret_cast<char*>(&expire_at_ms), sizeof(expire_at_ms));

        if (!in.good()) {
            return false;
        }

        Value v(std::move(val), expire_at_ms, now);
        if (!v.is_expired(now)) {
            entries.emplace_back(std::move(key), std::move(v));
        }
    }

    std::string footer(std::string_view(EOF_MARKER).size(), '\0');
    in.read(footer.data(), static_cast<std::streamsize>(footer.size()));
    if (footer != EOF_MARKER) {
        return false;
    }

    storage.load_snapshot(entries);
    return true;
}

} // namespace aegis
