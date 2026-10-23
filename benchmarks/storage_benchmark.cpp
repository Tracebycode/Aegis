#include "aegis/storage/sharded_hash_map.hpp"

#include <benchmark/benchmark.h>
#include <string>

static void BM_Storage_SetGet(benchmark::State& state) {
    size_t num_shards = static_cast<size_t>(state.range(0));
    aegis::ShardedHashMap storage(num_shards);

    uint64_t i = 0;
    for (auto _ : state) {
        std::string key = "bench_key_" + std::to_string(i++);
        storage.set(key, "bench_value");
        benchmark::DoNotOptimize(storage.get(key));
    }
}
BENCHMARK(BM_Storage_SetGet)->RangeMultiplier(4)->Range(1, 256)->Threads(1)->Threads(4)->Threads(8);

static void BM_Storage_ConcurrentReads(benchmark::State& state) {
    static aegis::ShardedHashMap storage(64);
    if (state.thread_index() == 0) {
        for (int i = 0; i < 10000; ++i) {
            storage.set("key_" + std::to_string(i), "val_" + std::to_string(i));
        }
    }

    uint64_t i = 0;
    for (auto _ : state) {
        std::string key = "key_" + std::to_string(i++ % 10000);
        auto res = storage.get(key);
        benchmark::DoNotOptimize(res);
    }
}
BENCHMARK(BM_Storage_ConcurrentReads)->Threads(1)->Threads(2)->Threads(4)->Threads(8);

BENCHMARK_MAIN();
