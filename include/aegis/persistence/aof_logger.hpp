#pragma once

#include "aegis/protocol/command.hpp"
#include "aegis/storage/sharded_hash_map.hpp"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <fstream>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace aegis {

enum class FsyncPolicy {
    Always,
    EverySec,
    No
};

class AofLogger {
public:
    explicit AofLogger(std::string filepath, FsyncPolicy policy = FsyncPolicy::EverySec);
    ~AofLogger();

    AofLogger(const AofLogger&) = delete;
    AofLogger& operator=(const AofLogger&) = delete;

    bool open();
    void close();

    // Appends a mutating command to the AOF log
    void append(const Command& cmd);
    void append_raw(std::string_view raw_resp);

    // Forces an immediate fsync to disk
    void sync();

    // Replays commands from the AOF file into storage during startup
    bool replay(ShardedHashMap& storage);

    [[nodiscard]] bool is_open() const noexcept { return is_open_.load(); }
    [[nodiscard]] const std::string& filepath() const noexcept { return filepath_; }
    [[nodiscard]] FsyncPolicy policy() const noexcept { return policy_; }

private:
    void flush_worker(std::stop_token stop_token);
    void write_buffer_to_disk(const std::string& buffer);

    std::string filepath_;
    FsyncPolicy policy_;
    int fd_{-1};
    std::atomic<bool> is_open_{false};

    // Double buffering for high-throughput async writes
    std::mutex mutex_;
    std::condition_variable_any cv_;
    std::string active_buffer_;
    std::string flush_buffer_;

    std::jthread flush_thread_;
};

} // namespace aegis
