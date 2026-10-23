#include "aegis/storage/sharded_hash_map.hpp"
#include "aegis/ttl/ttl_manager.hpp"

#include <gtest/gtest.h>
#include <thread>

TEST(TtlTest, ActiveBackgroundPurge) {
    aegis::ShardedHashMap storage(8);
    aegis::TtlManager ttl_mgr(storage, std::chrono::milliseconds(20), 50);

    // Add keys with short TTLs
    for (int i = 0; i < 20; ++i) {
        storage.set("temp_" + std::to_string(i), "val", aegis::now_ms() + 50);
    }
    // Add persistent keys
    storage.set("perm_1", "keep");
    storage.set("perm_2", "keep");

    EXPECT_EQ(storage.dbsize(), 22);

    ttl_mgr.start();

    // Sleep long enough for keys to expire and background worker to purge
    std::this_thread::sleep_for(std::chrono::milliseconds(150));

    ttl_mgr.stop();

    EXPECT_EQ(storage.dbsize(), 2);
    EXPECT_TRUE(storage.exists("perm_1"));
    EXPECT_TRUE(storage.exists("perm_2"));
    EXPECT_GE(ttl_mgr.total_evicted(), 20);
}
