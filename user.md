# Aegis: Prerequisites & Knowledge Guide

> **Project Overview**: Aegis is a Redis-inspired concurrent in-memory key-value server designed in C++20 for Linux, featuring an epoll-driven event loop, multithreaded request handling, custom wire protocol / RESP, TTL expiration mechanisms, pluggable persistence (AOF/snapshot), and benchmarking suites using GoogleTest and Google Benchmark.

This guide outlines everything you must understand before designing, implementing, and optimizing Aegis.

---

## Table of Contents
1. [Core Domains & Theoretical Foundations](#1-core-domains--theoretical-foundations)
2. [Modern C++20 Systems Programming](#2-modern-c20-systems-programming)
3. [Linux Networking & Socket Programming](#3-linux-networking--socket-programming)
4. [I/O Multiplexing with `epoll`](#4-io-multiplexing-with-epoll)
5. [Concurrency, Synchronization & Memory Models](#5-concurrency-synchronization--memory-models)
6. [Key-Value Store Internals & Data Structures](#6-key-value-store-internals--data-structures)
7. [Subsystems: TTL & Persistence](#7-subsystems-ttl--persistence)
8. [Debugging, Profiling & Tooling](#8-debugging-profiling--tooling)
9. [Curated Reading List & References](#9-curated-reading-list--references)

---

## 1. Core Domains & Theoretical Foundations

Before writing any code, ensure you have a mental model of how data moves from a network card to userspace memory and into a concurrent data structure:

```
[Client] ---> TCP/IP Packet ---> NIC Ring Buffer ---> Linux Kernel Socket Buffer (sk_buff)
                                                                 │
                                                    sys_epoll_wait wakes up
                                                                 │
                                                       read() / recv() into
                                                       Userspace Buffer (RingBuffer)
                                                                 │
                                                       Protocol Parser (Zero-Copy)
                                                                 │
                                                       Command Execution Engine
                                                                 │
                                            Concurrent Storage Engine (Sharded Hash Table)
                                                                 │
                                                      write() / send() back
```

### Essential Concepts to Grasp
- **Kernel-space vs. User-space context switching**: Costs of system calls (`syscall` instruction, CPU privilege level transition).
- **Zero-Copy philosophy**: Minimizing unnecessary `memcpy` operations by passing pointers/views (`std::string_view`, `std::span`) across network buffers.
- **Cache hierarchies & CPU microarchitecture**: L1/L2/L3 cache misses, CPU instruction pipeline stalls, and false sharing.

---

## 2. Modern C++20 Systems Programming

C++20 introduces language and library features that drastically improve safety, ergonomics, and performance for systems applications.

### Key C++20 Features Used in Aegis
- **Concepts & Constraints (`template <typename T> requires ...`)**:
  - Enforce compile-time constraints on custom allocators, hashers, and serialization handlers instead of opaque SFINAE.
- **`std::span` and `std::string_view`**:
  - Zero-copy slicing of network packets and buffer views without heap allocations.
- **`std::jthread` and `std::stop_token`**:
  - Cooperative thread cancellation and automatic joining on destruction for background worker threads (e.g., TTL sweeper, AOF flusher).
- **Atomics and Memory Fences (`std::atomic`, `std::atomic_ref`)**:
  - Non-blocking state management (e.g., server lifecycle state, connection counters, metric counters).
- **Bit manipulation (`<bit>`)**:
  - `std::bit_cast`, `std::countl_zero` for power-of-two hash table bucket math.
- **Designated Initializers**:
  - Clear and readable configuration structs for server options, connection options, and benchmark harnesses.

---

## 3. Linux Networking & Socket Programming

Aegis is built directly on the Linux POSIX socket API without heavy external frameworks (like Boost.Asio or Seastar) to maximize learning and raw control.

### Fundamental Socket Concepts
- **Socket Lifecycle**:
  - `socket()` $\rightarrow$ `setsockopt()` $\rightarrow$ `bind()` $\rightarrow$ `listen()` $\rightarrow$ `accept4(..., SOCK_NONBLOCK | SOCK_CLOEXEC)`.
- **Non-Blocking I/O (`O_NONBLOCK`)**:
  - In event-driven servers, sockets must **never** block. A `read()` or `write()` that cannot immediately proceed returns `-1` with `errno == EAGAIN` or `EWOULDBLOCK`.
- **Handling TCP Stream Semantics (Framing & Fragmentation)**:
  - TCP is a byte stream, **not** a packet protocol. A single request may arrive split across multiple `recv()` calls, or multiple requests may be batched into one `recv()`.
  - Your server must maintain a per-connection read buffer and parse framing boundaries incrementally.
- **Crucial Socket Options (`setsockopt`)**:
  - `SO_REUSEADDR`: Avoid `EADDRINUSE` errors when rapidly restarting the server during testing.
  - `SO_REUSEPORT`: Allows multiple threads/processes to bind to the exact same port, with the Linux kernel load-balancing incoming connections across listener sockets.
  - `TCP_NODELAY`: Disables Nagle’s algorithm. Crucial for key-value stores to minimize command-response round-trip latency.
  - `SO_RCVBUF` / `SO_SNDBUF`: Tune socket receive and send window buffers.

---

## 4. I/O Multiplexing with `epoll`

`epoll` is the Linux-specific kernel event notification mechanism capable of scaling to tens of thousands of concurrent file descriptors with $O(1)$ event readiness reporting.

### Core API
- `epoll_create1(EPOLL_CLOEXEC)`: Creates an epoll instance.
- `epoll_ctl(epfd, EPOLL_CTL_ADD / MOD / DEL, fd, &event)`: Registers or updates monitored events on a socket.
- `epoll_wait(epfd, events, maxevents, timeout)`: Blocks until one or more monitored file descriptors become ready.

### Level-Triggered (LT) vs. Edge-Triggered (ET)
| Feature | Level-Triggered (LT) | Edge-Triggered (ET) |
| :--- | :--- | :--- |
| **Trigger condition** | Triggers as long as buffer has unread data or writable space. | Triggers **only when state changes** (new data arrives). |
| **Complexity** | Easier; forgiving of unread data in buffer. | Higher; must loop `read()` until `EAGAIN`/`EWOULDBLOCK`. |
| **Syscall Overhead** | May cause spurious wakeups if data isn't consumed. | Minimal wakeups; requires strict draining loops. |
| **Aegis Design Choice** | Recommended for initial milestones. | Recommended for final high-throughput event loops. |

### Thundering Herd Problem
- When multiple worker threads block on the same `epoll_wait` instance, all might wake up when one connection arrives.
- **Solution in Aegis**: Either designate a dedicated acceptor thread or utilize `SO_REUSEPORT` with an epoll instance per worker thread.

---

## 5. Concurrency, Synchronization & Memory Models

Designing a multithreaded key-value store requires balancing throughput against lock contention.

### 1. Concurrency Architectures
- **Architecture A: Single-Threaded Reactor (Redis style)**:
  - Simple, no synchronization locks inside storage, but does not leverage multi-core CPUs without running multiple separate processes.
- **Architecture B: Thread-per-Core Reactor with `SO_REUSEPORT`**:
  - Each thread runs its own epoll loop. Threads share the storage engine or operate on sharded partitions.
- **Architecture C: Master Acceptor + Worker Thread Pool**:
  - Master thread handles `epoll_wait` for incoming connections and passes active sockets or parsed tasks to worker threads via lock-free ring buffers.

### 2. Synchronization Primitives
- **`std::shared_mutex` (Reader-Writer Locks)**:
  - Multiple concurrent `GET` operations; exclusive lock for `SET`/`DEL`.
- **Lock Striping / Sharding**:
  - Instead of one global mutex for the whole key-value store, partition keys into $N$ shards (e.g., 64 or 256 shards based on `hash(key) % N`), each with its own mutex. This dramatically cuts lock contention.
- **Spinlocks vs. Mutexes**:
  - For microsecond-level in-memory lookups, a lightweight user-space spinlock or `pthread_adaptive_mutex` often outperforms a sleeping `std::mutex`.

### 3. False Sharing & Memory Alignment
- When two variables used by different threads reside on the same 64-byte CPU cache line, updating one invalidates the other's cache line.
- **Solution**: Use `alignas(std::hardware_destructive_interference_size)` (or 64 bytes) on per-thread or per-shard data structures.

---

## 6. Key-Value Store Internals & Data Structures

### Primary In-Memory Index
- **Option 1: Sharded Hash Table (`std::unordered_map` with Striping)**:
  - Simplest starting point: an array of $K$ buckets/shards, each holding a `std::unordered_map<std::string, Value>` and a reader-writer lock.
- **Option 2: Custom Cache-Friendly Hash Table**:
  - Open addressing with Robin Hood hashing or Hopscotch hashing for flat memory layout and lower cache miss penalties.

### Protocol Specification
Aegis can support:
1. **Simple Text Protocol**: `GET <key>\r\n`, `SET <key> <val> [EX <seconds>]\r\n`, `DEL <key>\r\n`.
2. **RESP (Redis Serialization Protocol v2)**: Compatible with standard Redis clients (`redis-cli`, Python `redis-py`, etc.).
   - Simple strings (`+OK\r\n`)
   - Errors (`-ERR unknown command\r\n`)
   - Integers (`:1000\r\n`)
   - Bulk strings (`$4\r\ntest\r\n`)
   - Arrays (`*2\r\n$3\r\nGET\r\n$4\r\nkey1\r\n`)

---

## 7. Subsystems: TTL & Persistence

### TTL (Time-To-Live) Expiration Subsystem
1. **Passive / Lazy Expiration**:
   - Every time a key is accessed (`GET`), check its expiration timestamp. If expired, delete it and return `NIL`.
2. **Active / Background Expiration**:
   - A background thread wakes up periodically (e.g., every 100ms) to clean expired keys.
   - **Data Structure**: A Min-Heap (or hierarchical Timer Wheel / `std::multimap<Timestamp, Key>`) ordered by expiration time. Allows $O(1)$ inspection of next expiring keys.

### Persistence Subsystems
1. **AOF (Append-Only File)**:
   - Log all mutating commands (`SET`, `DEL`) sequentially to disk.
   - **Fsync policies**:
     - `always`: Call `fsync()` on every write (slowest, zero data loss).
     - `everysec`: Background thread calls `fdatasync()` once per second (standard trade-off).
     - `no`: Rely on OS dirty page flushes (fastest, risk of loss on crash).
2. **Snapshotting (RDB style)**:
   - Periodic dump of the entire in-memory state to a compact binary file.
   - On Linux, can leverage `fork()` to let copy-on-write (COW) handle consistent point-in-time snapshots without locking the main thread.

---

## 8. Debugging, Profiling & Tooling

To measure and defend your systems-level performance claims, you must master:

| Tool | Purpose | Commands / Usage |
| :--- | :--- | :--- |
| **GoogleTest** | Unit and functional testing | Thread-safety tests, command parsing tests |
| **Google Benchmark** | Microbenchmarking | Measuring lock contention, hash lookup ns/op |
| **ThreadSanitizer (TSan)** | Data race detection | Compile with `-fsanitize=thread -g` |
| **AddressSanitizer (ASan)** | Memory safety, buffer overruns | Compile with `-fsanitize=address -g` |
| **Valgrind / Massif** | Heap profiling & memory leak checks | `valgrind --tool=massif ./aegis_server` |
| **`perf`** | Linux CPU performance profiling | `perf record -F 99 -g -- ./aegis_server`<br>`perf report` / FlameGraphs |
| **`strace`** | Inspect syscall frequency | `strace -c ./aegis_server` |
| **`redis-benchmark` / `memtier_benchmark`** | Load testing throughput & latency | `memtier_benchmark -s 127.0.0.1 -p 6379 -c 50 -t 4 --ratio 1:1` |

---

## 9. Curated Reading List & References

### Foundational Books
1. **The Linux Programming Interface (TLPI)** — *Michael Kerrisk*
   - Read Chapter 56-61 (Sockets) and Chapter 63 (Alternative I/O Models: epoll).
2. **C++ Concurrency in Action (2nd Edition)** — *Anthony Williams*
   - Read Chapters 3-5 (Sharing data between threads, Synchronizing concurrent operations, C++ memory model and atomic operations).
3. **Systems Performance: Enterprise and the Cloud (2nd Edition)** — *Brendan Gregg*
   - Profiling CPU, memory, context switches, and cache hit rates.

### Seminal Articles & Documentation
- **Epoll Scalability**:
  - [The Method to epoll's Madness](https://copyconstruct.medium.com/the-method-to-epolls-madness-d9d2d6378d1f) by Cindy Sridharan.
  - [Epoll is fundamentally broken (1/2)](https://idea.popcount.org/2017-02-20-epoll-is-fundamentally-broken-12/) by Marek Majkowski (Cloudflare Blog).
- **Redis Architecture & Networking**:
  - [Redis Protocol Specification (RESP)](https://redis.io/docs/latest/develop/reference/protocol-spec/)
  - [Why Redis is single-threaded (and how it evolved with I/O threads)](https://redis.com/blog/diving-into-redis-6-io-threads/)
- **Lock-Free & Concurrency**:
  - [Lock-Striping Pattern in Concurrent Data Structures](https://en.wikipedia.org/wiki/Lock_striping)
  - [Herb Sutter: atomic<> Weapons 1 & 2](https://herbsutter.com/2013/02/11/atomic-weapons-the-c-memory-model-and-hardware-architecture/)

---
*Created for the Aegis Project. Proceed to [`roadmap.md`](roadmap.md) to inspect the high-level architecture and milestone delivery plan.*
