#pragma once

#include "aegis/common/types.hpp"
#include "aegis/storage/sharded_hash_map.hpp"

#include <string>

namespace aegis {

class SnapshotManager {
public:
    static bool save(const std::string& filepath, ShardedHashMap& storage);
    static bool load(const std::string& filepath, ShardedHashMap& storage);
};

} // namespace aegis
