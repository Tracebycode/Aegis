#pragma once

#include "aegis/net/connection.hpp"
#include "aegis/net/epoll_reactor.hpp"
#include "aegis/net/socket.hpp"
#include "aegis/persistence/aof_logger.hpp"
#include "aegis/protocol/command.hpp"
#include "aegis/protocol/parser.hpp"
#include "aegis/protocol/serializer.hpp"
#include "aegis/storage/sharded_hash_map.hpp"
#include "aegis/ttl/ttl_manager.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace aegis {

struct ServerConfig {
    std::string host{"0.0.0.0"};
    uint16_t port{6379};
    size_t num_shards{64};
    size_t worker_threads{4};
    bool aof_enabled{false};
    std::string aof_filepath{"aegis.aof"};
    FsyncPolicy aof_fsync_policy{FsyncPolicy::EverySec};
    std::string snapshot_filepath{"aegis.rdb"};
    std::chrono::milliseconds ttl_interval{100};
};

class Server {
public:
    explicit Server(ServerConfig config = {});
    ~Server();

    Server(const Server&) = delete;
    Server& operator=(const Server&) = delete;

    bool start();
    void stop();
    void wait();

    [[nodiscard]] bool is_running() const noexcept { return is_running_.load(); }
    [[nodiscard]] ShardedHashMap& storage() noexcept { return storage_; }
    [[nodiscard]] const ServerConfig& config() const noexcept { return config_; }

    // Direct command execution (for testing and internal routing)
    std::string execute_command(const Command& cmd, bool& should_close);

    [[nodiscard]] uint64_t total_commands_processed() const noexcept {
        return total_commands_processed_.load();
    }
    [[nodiscard]] uint64_t total_connections_accepted() const noexcept {
        return total_connections_accepted_.load();
    }

private:
    void worker_loop(size_t worker_id, std::stop_token stop_token);
    void handle_connection_events(EpollReactor& reactor,
                                  std::unordered_map<int, std::unique_ptr<Connection>>& conns,
                                  int fd, uint32_t events);

    ServerConfig config_;
    ShardedHashMap storage_;
    std::unique_ptr<TtlManager> ttl_manager_;
    std::unique_ptr<AofLogger> aof_logger_;

    std::atomic<bool> is_running_{false};
    std::atomic<uint64_t> total_commands_processed_{0};
    std::atomic<uint64_t> total_connections_accepted_{0};
    std::atomic<uint64_t> active_connections_{0};
    uint64_t start_time_ms_{0};

    std::vector<std::jthread> worker_threads_;
};

} // namespace aegis
