#include "aegis/persistence/aof_logger.hpp"
#include "aegis/persistence/snapshot.hpp"
#include "aegis/storage/sharded_hash_map.hpp"

#include <filesystem>
#include <gtest/gtest.h>

TEST(PersistenceTest, AofLogAndReplay) {
    std::string test_aof = "test_aegis.aof";
    if (std::filesystem::exists(test_aof)) {
        std::filesystem::remove(test_aof);
    }

    {
        aegis::ShardedHashMap storage(8);
        aegis::AofLogger logger(test_aof, aegis::FsyncPolicy::Always);
        ASSERT_TRUE(logger.open());

        // Perform mutations and log them
        aegis::Command cmd1({"SET", "k1", "v1"});
        storage.set("k1", "v1");
        logger.append(cmd1);

        aegis::Command cmd2({"SET", "k2", "v2"});
        storage.set("k2", "v2");
        logger.append(cmd2);

        aegis::Command cmd3({"DEL", "k1"});
        storage.del("k1");
        logger.append(cmd3);

        logger.close();
    }

    // Now restore into a fresh storage engine
    {
        aegis::ShardedHashMap recovered_storage(8);
        EXPECT_EQ(recovered_storage.dbsize(), 0);

        aegis::AofLogger logger(test_aof);
        EXPECT_TRUE(logger.replay(recovered_storage));

        EXPECT_FALSE(recovered_storage.exists("k1"));
        EXPECT_TRUE(recovered_storage.exists("k2"));

        auto res = recovered_storage.get("k2");
        ASSERT_TRUE(res.is_ok());
        EXPECT_EQ(res.value(), "v2");
    }

    std::filesystem::remove(test_aof);
}

TEST(PersistenceTest, SnapshotSaveAndLoad) {
    std::string test_rdb = "test_snapshot.rdb";
    if (std::filesystem::exists(test_rdb)) {
        std::filesystem::remove(test_rdb);
    }

    {
        aegis::ShardedHashMap storage(8);
        storage.set("alpha", "111");
        storage.set("beta", "222");
        storage.set("gamma", "333");

        EXPECT_TRUE(aegis::SnapshotManager::save(test_rdb, storage));
    }

    {
        aegis::ShardedHashMap new_storage(8);
        EXPECT_TRUE(aegis::SnapshotManager::load(test_rdb, new_storage));

        EXPECT_EQ(new_storage.dbsize(), 3);
        EXPECT_EQ(new_storage.get("alpha").value(), "111");
        EXPECT_EQ(new_storage.get("beta").value(), "222");
        EXPECT_EQ(new_storage.get("gamma").value(), "333");
    }

    std::filesystem::remove(test_rdb);
}
