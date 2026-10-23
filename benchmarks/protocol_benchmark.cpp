#include "aegis/protocol/parser.hpp"
#include "aegis/protocol/serializer.hpp"

#include <benchmark/benchmark.h>
#include <string>

static void BM_RespParser_SetCommand(benchmark::State& state) {
    std::string frame = "*3\r\n$3\r\nSET\r\n$12\r\nuser:session\r\n$32\r\n9f8a3a6702344f088bf26d2c9d1b5a88\r\n";
    aegis::RespParser parser;

    for (auto _ : state) {
        size_t consumed = 0;
        aegis::Command cmd;
        auto res = parser.parse(frame, consumed, cmd);
        benchmark::DoNotOptimize(res);
        benchmark::DoNotOptimize(consumed);
        benchmark::DoNotOptimize(cmd);
    }

    state.SetBytesProcessed(static_cast<int64_t>(state.iterations() * frame.size()));
}
BENCHMARK(BM_RespParser_SetCommand);

static void BM_RespSerializer_BulkString(benchmark::State& state) {
    std::string payload = "9f8a3a6702344f088bf26d2c9d1b5a88";

    for (auto _ : state) {
        std::string res = aegis::RespSerializer::bulk_string(payload);
        benchmark::DoNotOptimize(res);
    }
}
BENCHMARK(BM_RespSerializer_BulkString);

BENCHMARK_MAIN();
