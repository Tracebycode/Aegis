#include "aegis/storage/sharded_hash_map.hpp"

#include <gtest/gtest.h>
#include <thread>
#include <vector>

TEST(StorageTest, BasicOperations) {
    aegis::ShardedHashMap storage(16);

    // Initial state
    EXPECT_EQ(storage.dbsize(), 0);
    EXPECT_FALSE(storage.exists("foo"));

    // Set & Get
    storage.set("foo", "bar");
    EXPECT_TRUE(storage.exists("foo"));
    EXPECT_EQ(storage.dbsize(), 1);

    auto res = storage.get("foo");
    ASSERT_TRUE(res.is_ok());
    EXPECT_EQ(res.value(), "bar");

    // Overwrite
    storage.set("foo", "baz");
    res = storage.get("foo");
    ASSERT_TRUE(res.is_ok());
    EXPECT_EQ(res.value(), "baz");

    // Del
    EXPECT_TRUE(storage.del("foo"));
    EXPECT_FALSE(storage.exists("foo"));
    EXPECT_FALSE(storage.get("foo").is_ok());
    EXPECT_EQ(storage.dbsize(), 0);
}

TEST(StorageTest, TtlAndLazyEviction) {
    aegis::ShardedHashMap storage(16);

    storage.set("temp", "val");
    EXPECT_EQ(storage.ttl("temp"), -1); // Persist

    // Expire in 1 second
    EXPECT_TRUE(storage.expire("temp", 1));
    EXPECT_GE(storage.ttl("temp"), 0);
    EXPECT_LE(storage.ttl("temp"), 1);

    // Persist again
    EXPECT_TRUE(storage.persist("temp"));
    EXPECT_EQ(storage.ttl("temp"), -1);

    // Expire with millisecond precision
    storage.pexpire("temp", 50);
    EXPECT_TRUE(storage.exists("temp"));

    // Wait for expiration
    std::this_thread::sleep_for(std::chrono::milliseconds(70));

    // Lazy expiration on get / exists
    EXPECT_FALSE(storage.exists("temp"));
    auto res = storage.get("temp");
    EXPECT_FALSE(res.is_ok());
    EXPECT_EQ(storage.dbsize(), 0);
}

TEST(StorageTest, KeysPatternMatching) {
    aegis::ShardedHashMap storage(8);

    storage.set("user:1", "alice");
    storage.set("user:2", "bob");
    storage.set("order:1", "apple");

    auto user_keys = storage.keys("user:*");
    EXPECT_EQ(user_keys.size(), 2);

    auto all_keys = storage.keys("*");
    EXPECT_EQ(all_keys.size(), 3);

    auto order_keys = storage.keys("order:?");
    EXPECT_EQ(order_keys.size(), 1);
}

TEST(StorageTest, ConcurrentStressTest) {
    aegis::ShardedHashMap storage(64);
    constexpr int NUM_THREADS = 8;
    constexpr int OPS_PER_THREAD = 1000;

    std::vector<std::jthread> threads;
    threads.reserve(NUM_THREADS);

    for (int t = 0; t < NUM_THREADS; ++t) {
        threads.emplace_back([&storage, t]() {
            for (int i = 0; i < OPS_PER_THREAD; ++i) {
                std::string key = "key_" + std::to_string(t) + "_" + std::to_string(i);
                std::string val = "val_" + std::to_string(i);
                storage.set(key, val);

                auto res = storage.get(key);
                EXPECT_TRUE(res.is_ok());
                EXPECT_EQ(res.value(), val);
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(storage.dbsize(), static_cast<size_t>(NUM_THREADS * OPS_PER_THREAD));
}
