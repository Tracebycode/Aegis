# Aegis: High-Performance Concurrent In-Memory Key-Value Store

Aegis is a high-performance, multithreaded, in-memory key-value store engineered in **C++20** for Linux. It features an **`epoll` event-driven Multi-Reactor**, **RESP2 wire protocol compatibility** (plug-and-play with `redis-cli` and standard Redis client libraries), a **lock-striped sharded storage engine** designed to eliminate false sharing, a **two-tier TTL expiration subsystem**, and **pluggable persistence** (Append-Only File and binary Point-in-Time Snapshots).

---

## Architecture Overview

```
                      +-----------------------------+
                      |   Standard Redis Clients    |
                      | (redis-cli, py-redis, etc.) |
                      +--------------+--------------+
                                     |  TCP
                                     v
                      +-----------------------------+
                      |  Multi-Reactor Network I/O  |
                      |  (SO_REUSEPORT, epoll_wait) |
                      +--------------+--------------+
                                     |
                                     v
                      +-----------------------------+
                      |   RESP2 Protocol Parser     |
                      |   (Zero-copy byte stream)   |
                      +--------------+--------------+
                                     |
                                     v
                      +-----------------------------+
                      |  Sharded Concurrent Storage |
                      |    (alignas(64), Striped    |
                      |    std::shared_mutex Map)   |
                      +-------+-------------+-------+
                              |             |
           +------------------+             +-------------------+
           v                                                    v
+-----------------------+                            +--------------------+
|  TTL Expiration Engine|                            | Persistence Engine |
|  Lazy + Background    |                            | Async AOF + RDB    |
|  std::jthread Cleaner |                            | Snapshot Recovery  |
+-----------------------+                            +--------------------+
```

### Core Architectural Features

1. **Lock-Striped Sharded Hash Table (`src/storage/`)**:
   - Keyspace partitioned across $2^N$ cache-line aligned shards (`alignas(64)`), eliminating CPU false sharing.
   - Fast MurmurHash3 64-bit striping with `shard_index = hash & (num_shards - 1)`.
   - Reader-writer locks (`std::shared_mutex`) allow massive concurrent parallel reads while isolating writes to individual shards.
2. **RESP2 Protocol Engine (`src/protocol/`)**:
   - Zero-copy incremental parser handling Bulk Strings, Integers, Simple Strings, Arrays, Errors, and inline text commands.
   - Resilient against TCP stream fragmentation and supports command pipelining.
3. **Multi-Reactor Network Subsystem (`src/net/`)**:
   - Native Linux non-blocking sockets with `SO_REUSEPORT`, allowing the Linux kernel to distribute incoming connections across dedicated epoll reactor worker threads without acceptor lock contention.
   - Edge/level-triggered `epoll` loops with `ByteBuffer` buffering.
4. **Two-Tier TTL & Eviction Engine (`src/ttl/`)**:
   - **Passive / Lazy**: Keys accessed after expiration are evicted immediately upon lookup (`GET`, `EXISTS`).
   - **Active / Background**: A dedicated `std::jthread` with cooperative `std::stop_token` wakes periodically to sample shards and purge expired keys.
5. **Pluggable Durability Subsystem (`src/persistence/`)**:
   - **Append-Only File (AOF)**: High-throughput double-buffering logger with configurable fsync policies (`always`, `everysec`, `no`) and startup replay.
   - **Point-in-Time Snapshotting**: Atomic disk serialization and state restoration.

---

## Directory Structure

```
Aegis/
├── CMakeLists.txt              # Build configuration (C++20, GoogleTest, Google Benchmark)
├── .clang-format               # Code formatting rules
├── include/
│   └── aegis/
│       ├── common/             # Types, Result<T>, TimeUtils, ByteBuffer, MurmurHash3
│       ├── net/                # Socket, Connection, EpollReactor, Server
│       ├── protocol/           # RespParser, Command, RespSerializer, RespType
│       ├── storage/            # ShardedHashMap, Shard, Value
│       ├── ttl/                # TtlManager
│       └── persistence/        # AofLogger, SnapshotManager
├── src/
│   ├── main.cpp                # CLI server binary entrypoint
│   ├── common/
│   ├── net/
│   ├── protocol/
│   ├── storage/
│   ├── ttl/
│   └── persistence/
├── tests/
│   ├── unit/                   # Storage, Protocol, TTL, Net, Persistence tests
│   └── integration/            # Full-loop TCP client/server tests
├── benchmarks/
│   ├── storage_benchmark.cpp   # Multi-thread contention benchmarks
│   └── protocol_benchmark.cpp  # RESP2 parsing & serialization benchmarks
├── user.md                     # Systems programming and concept guide
└── roadmap.md                  # Architectural specification
```

---

## Building and Running

### Prerequisites
- Linux / WSL (Ubuntu 22.04+)
- C++20 compiler (`g++-13` or `clang++-16` or newer)
- CMake 3.20+
- Ninja or Make

### Build

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

### Running the Server

```bash
# Start server on default port 6379 with 4 worker threads and 64 shards
./build/aegis_server

# Custom port, threads, and AOF persistence
./build/aegis_server -p 6379 -w 4 -s 64 --aof aegis.aof --snapshot aegis.rdb
```

### Testing with `redis-cli`

```bash
# Connect with standard redis-cli
redis-cli -p 6379

127.0.0.1:6379> PING
PONG
127.0.0.1:6379> SET user:100 "Alice"
OK
127.0.0.1:6379> GET user:100
"Alice"
127.0.0.1:6379> EXPIRE user:100 60
(integer) 1
127.0.0.1:6379> TTL user:100
(integer) 60
127.0.0.1:6379> DBSIZE
(integer) 1
127.0.0.1:6379> INFO
# Server
redis_version:7.0.0-aegis
aegis_version:0.1.0
...
```

---

## Running Test Suite

All unit tests and end-to-end integration tests run via CTest:

```bash
cd build
ctest --output-on-failure
```

Or execute test binaries directly:

```bash
./build/aegis_unit_tests
./build/aegis_integration_tests
```

---

## Running Benchmarks

### In-Memory Storage Contention Benchmark

```bash
./build/storage_benchmark
```
*Results sample (8-core CPU)*:
- Concurrent 8-thread lookups: **28.0 ns** per operation (~35.7 million ops/sec).
- Concurrent 8-thread striped Set/Get (256 shards): **115 ns** per operation.

### RESP2 Protocol Benchmark

```bash
./build/protocol_benchmark
```
*Results sample*:
- RESP Command Parser: **117 ns** (~580 MiB/s streaming throughput).
- Bulk String Serializer: **28.3 ns**.

### Live Network Throughput (`redis-benchmark`)

```bash
redis-benchmark -p 6379 -t get,set -n 100000 -q
```
Achieves **37,000+ requests per second** over loopback TCP with sub-millisecond median latency.
