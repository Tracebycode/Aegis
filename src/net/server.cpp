#include "aegis/net/server.hpp"
#include "aegis/persistence/snapshot.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <filesystem>
#include <sstream>

namespace aegis {

namespace {

std::string format_info(const ServerConfig& config,
                        uint64_t uptime_sec,
                        uint64_t total_commands,
                        uint64_t total_connections,
                        uint64_t active_clients,
                        size_t total_keys) {
    std::ostringstream oss;
    oss << "# Server\r\n"
        << "redis_version:7.0.0-aegis\r\n"
        << "aegis_version:0.1.0\r\n"
        << "os:Linux\r\n"
        << "arch_bits:64\r\n"
        << "uptime_in_seconds:" << uptime_sec << "\r\n"
        << "tcp_port:" << config.port << "\r\n"
        << "# Clients\r\n"
        << "connected_clients:" << active_clients << "\r\n"
        << "# Stats\r\n"
        << "total_connections_received:" << total_connections << "\r\n"
        << "total_commands_processed:" << total_commands << "\r\n"
        << "# Keyspace\r\n"
        << "db0:keys=" << total_keys << ",expires=0,avg_ttl=0\r\n";
    return oss.str();
}

} // namespace

Server::Server(ServerConfig config)
    : config_(std::move(config)),
      storage_(config_.num_shards) {
    ttl_manager_ = std::make_unique<TtlManager>(storage_, config_.ttl_interval);
    if (config_.aof_enabled) {
        aof_logger_ = std::make_unique<AofLogger>(config_.aof_filepath, config_.aof_fsync_policy);
    }
}

Server::~Server() {
    stop();
}

bool Server::start() {
    if (is_running_.exchange(true)) {
        return true;
    }

    start_time_ms_ = now_ms();

    // Replay AOF or load snapshot
    if (config_.aof_enabled && aof_logger_) {
        aof_logger_->replay(storage_);
        aof_logger_->open();
    } else if (std::filesystem::exists(config_.snapshot_filepath)) {
        SnapshotManager::load(config_.snapshot_filepath, storage_);
    }

    // Start background TTL cleaner
    ttl_manager_->start();

    // Spawn Multi-Reactor worker threads using SO_REUSEPORT
    size_t num_workers = std::max(size_t{1}, config_.worker_threads);
    worker_threads_.reserve(num_workers);

    for (size_t i = 0; i < num_workers; ++i) {
        worker_threads_.emplace_back([this, i](std::stop_token stop_token) {
            worker_loop(i, stop_token);
        });
    }

    return true;
}

void Server::stop() {
    if (!is_running_.exchange(false)) {
        return;
    }

    if (ttl_manager_) {
        ttl_manager_->stop();
    }

    if (aof_logger_) {
        aof_logger_->close();
    }

    for (auto& t : worker_threads_) {
        if (t.joinable()) {
            t.request_stop();
        }
    }

    for (auto& t : worker_threads_) {
        if (t.joinable()) {
            t.join();
        }
    }
    worker_threads_.clear();
}

void Server::wait() {
    for (auto& t : worker_threads_) {
        if (t.joinable()) {
            t.join();
        }
    }
}

void Server::worker_loop(size_t /*worker_id*/, std::stop_token stop_token) {
    Socket listener = Socket::create_tcp();
    if (!listener.is_valid()) {
        return;
    }

    listener.set_reuseaddr(true);
    listener.set_reuseport(true);
    listener.set_nonblocking(true);

    if (!listener.bind(config_.host, config_.port) || !listener.listen(4096)) {
        return;
    }

    EpollReactor reactor(1024);
    if (!reactor.is_valid()) {
        return;
    }

    reactor.add_fd(listener.fd(), EPOLLIN);

    std::unordered_map<int, std::unique_ptr<Connection>> conns;
    std::vector<epoll_event> active_events;

    while (!stop_token.stop_requested() && is_running_.load()) {
        int nfds = reactor.wait(active_events, 50);
        if (nfds <= 0) {
            continue;
        }

        for (const auto& ev : active_events) {
            int fd = ev.data.fd;

            if (fd == listener.fd()) {
                // Accept incoming connections
                while (true) {
                    std::string client_ip;
                    uint16_t client_port = 0;
                    int client_fd = listener.accept(client_ip, client_port);
                    if (client_fd < 0) {
                        break;
                    }

                    Socket client_sock(client_fd);
                    auto conn = std::make_unique<Connection>(std::move(client_sock),
                                                             std::move(client_ip), client_port);

                    uint32_t conn_events = EPOLLIN | EPOLLRDHUP | EPOLLERR;
                    reactor.add_fd(client_fd, conn_events);
                    conns[client_fd] = std::move(conn);

                    total_connections_accepted_.fetch_add(1, std::memory_order_relaxed);
                    active_connections_.fetch_add(1, std::memory_order_relaxed);
                }
            } else {
                handle_connection_events(reactor, conns, fd, ev.events);
            }
        }
    }

    // Clean up connections on loop termination
    for (auto& [fd, conn] : conns) {
        reactor.remove_fd(fd);
        conn->close();
    }
    conns.clear();
}

void Server::handle_connection_events(EpollReactor& reactor,
                                      std::unordered_map<int, std::unique_ptr<Connection>>& conns,
                                      int fd, uint32_t events) {
    auto it = conns.find(fd);
    if (it == conns.end()) {
        return;
    }

    auto& conn = it->second;

    if (events & (EPOLLERR | EPOLLHUP | EPOLLRDHUP)) {
        reactor.remove_fd(fd);
        conn->close();
        active_connections_.fetch_sub(1, std::memory_order_relaxed);
        conns.erase(it);
        return;
    }

    if (events & EPOLLIN) {
        ssize_t n = conn->read_from_socket();
        if (n == 0) {
            reactor.remove_fd(fd);
            conn->close();
            active_connections_.fetch_sub(1, std::memory_order_relaxed);
            conns.erase(it);
            return;
        }

        RespParser parser;
        bool should_close = false;

        while (!conn->read_buffer().empty()) {
            std::string_view read_view = conn->read_buffer().read_view();
            size_t consumed = 0;
            Command cmd;

            ParseResult pres = parser.parse(read_view, consumed, cmd);
            if (pres == ParseResult::Success && consumed > 0) {
                conn->read_buffer().advance_read(consumed);

                if (!cmd.empty()) {
                    std::string resp = execute_command(cmd, should_close);
                    conn->queue_response(resp);
                }

                if (should_close) {
                    break;
                }
            } else if (pres == ParseResult::NeedMoreData) {
                break;
            } else {
                // Syntax or protocol error
                conn->queue_response(RespSerializer::error("ERR Protocol syntax error"));
                conn->read_buffer().reset();
                break;
            }
        }

        if (conn->has_pending_write()) {
            conn->write_to_socket();
            if (conn->has_pending_write()) {
                reactor.modify_fd(fd, EPOLLIN | EPOLLOUT | EPOLLRDHUP | EPOLLERR);
            }
        }

        if (should_close) {
            reactor.remove_fd(fd);
            conn->close();
            active_connections_.fetch_sub(1, std::memory_order_relaxed);
            conns.erase(it);
            return;
        }
    }

    if (events & EPOLLOUT) {
        conn->write_to_socket();
        if (!conn->has_pending_write()) {
            reactor.modify_fd(fd, EPOLLIN | EPOLLRDHUP | EPOLLERR);
        }
    }

    if (conn->is_closed()) {
        reactor.remove_fd(fd);
        active_connections_.fetch_sub(1, std::memory_order_relaxed);
        conns.erase(it);
    }
}

std::string Server::execute_command(const Command& cmd, bool& should_close) {
    total_commands_processed_.fetch_add(1, std::memory_order_relaxed);
    should_close = false;

    switch (cmd.type()) {
    case CommandType::Ping: {
        if (cmd.arg_count() >= 1) {
            return RespSerializer::bulk_string(cmd.arg(0));
        }
        return RespSerializer::pong();
    }
    case CommandType::Echo: {
        if (cmd.arg_count() < 1) {
            return RespSerializer::error("wrong number of arguments for 'echo' command");
        }
        return RespSerializer::bulk_string(cmd.arg(0));
    }
    case CommandType::Set: {
        if (cmd.arg_count() < 2) {
            return RespSerializer::error("wrong number of arguments for 'set' command");
        }
        const std::string& key = cmd.arg(0);
        const std::string& val = cmd.arg(1);
        uint64_t expire_at_ms = 0;

        if (cmd.arg_count() >= 4) {
            std::string opt = cmd.arg(2);
            for (char& c : opt) c = static_cast<char>(std::toupper(c));
            try {
                if (opt == "EX") {
                    uint64_t sec = std::stoull(cmd.arg(3));
                    expire_at_ms = now_ms() + (sec * 1000);
                } else if (opt == "PX") {
                    uint64_t ms = std::stoull(cmd.arg(3));
                    expire_at_ms = now_ms() + ms;
                } else {
                    return RespSerializer::error("syntax error in SET command");
                }
            } catch (...) {
                return RespSerializer::error("value is not an integer or out of range");
            }
        }

        storage_.set(key, val, expire_at_ms);

        if (aof_logger_) {
            aof_logger_->append(cmd);
        }

        return RespSerializer::ok();
    }
    case CommandType::Get: {
        if (cmd.arg_count() < 1) {
            return RespSerializer::error("wrong number of arguments for 'get' command");
        }
        auto res = storage_.get(cmd.arg(0));
        if (res.is_ok()) {
            return RespSerializer::bulk_string(res.value());
        }
        return RespSerializer::null_bulk();
    }
    case CommandType::Del: {
        if (cmd.arg_count() < 1) {
            return RespSerializer::error("wrong number of arguments for 'del' command");
        }
        std::vector<std::string> keys_to_del;
        keys_to_del.reserve(cmd.arg_count());
        for (size_t i = 0; i < cmd.arg_count(); ++i) {
            keys_to_del.push_back(cmd.arg(i));
        }

        size_t count = storage_.del(keys_to_del);

        if (aof_logger_ && count > 0) {
            aof_logger_->append(cmd);
        }

        return RespSerializer::integer(static_cast<int64_t>(count));
    }
    case CommandType::Exists: {
        if (cmd.arg_count() < 1) {
            return RespSerializer::error("wrong number of arguments for 'exists' command");
        }
        int64_t count = 0;
        for (size_t i = 0; i < cmd.arg_count(); ++i) {
            if (storage_.exists(cmd.arg(i))) {
                ++count;
            }
        }
        return RespSerializer::integer(count);
    }
    case CommandType::Expire: {
        if (cmd.arg_count() < 2) {
            return RespSerializer::error("wrong number of arguments for 'expire' command");
        }
        try {
            uint64_t sec = std::stoull(cmd.arg(1));
            bool ok = storage_.expire(cmd.arg(0), sec);
            if (ok && aof_logger_) {
                aof_logger_->append(cmd);
            }
            return RespSerializer::integer(ok ? 1 : 0);
        } catch (...) {
            return RespSerializer::error("value is not an integer or out of range");
        }
    }
    case CommandType::Pexpire: {
        if (cmd.arg_count() < 2) {
            return RespSerializer::error("wrong number of arguments for 'pexpire' command");
        }
        try {
            uint64_t ms = std::stoull(cmd.arg(1));
            bool ok = storage_.pexpire(cmd.arg(0), ms);
            if (ok && aof_logger_) {
                aof_logger_->append(cmd);
            }
            return RespSerializer::integer(ok ? 1 : 0);
        } catch (...) {
            return RespSerializer::error("value is not an integer or out of range");
        }
    }
    case CommandType::Ttl: {
        if (cmd.arg_count() < 1) {
            return RespSerializer::error("wrong number of arguments for 'ttl' command");
        }
        return RespSerializer::integer(storage_.ttl(cmd.arg(0)));
    }
    case CommandType::Pttl: {
        if (cmd.arg_count() < 1) {
            return RespSerializer::error("wrong number of arguments for 'pttl' command");
        }
        return RespSerializer::integer(storage_.pttl(cmd.arg(0)));
    }
    case CommandType::Persist: {
        if (cmd.arg_count() < 1) {
            return RespSerializer::error("wrong number of arguments for 'persist' command");
        }
        return RespSerializer::integer(storage_.persist(cmd.arg(0)) ? 1 : 0);
    }
    case CommandType::DbSize: {
        return RespSerializer::integer(static_cast<int64_t>(storage_.dbsize()));
    }
    case CommandType::FlushAll: {
        storage_.flushall();
        if (aof_logger_) {
            aof_logger_->append(cmd);
        }
        return RespSerializer::ok();
    }
    case CommandType::Keys: {
        std::string pattern = "*";
        if (cmd.arg_count() >= 1) {
            pattern = cmd.arg(0);
        }
        auto matched = storage_.keys(pattern);
        return RespSerializer::array(matched);
    }
    case CommandType::Save: {
        bool ok = SnapshotManager::save(config_.snapshot_filepath, storage_);
        if (ok) {
            return RespSerializer::ok();
        }
        return RespSerializer::error("ERR failed to save snapshot");
    }
    case CommandType::BgSave: {
        std::thread([this]() {
            SnapshotManager::save(config_.snapshot_filepath, storage_);
        }).detach();
        return RespSerializer::simple_string("Background saving started");
    }
    case CommandType::Info: {
        uint64_t uptime_sec = (now_ms() - start_time_ms_) / 1000;
        std::string info_str = format_info(config_,
                                           uptime_sec,
                                           total_commands_processed_.load(),
                                           total_connections_accepted_.load(),
                                           active_connections_.load(),
                                           storage_.dbsize());
        return RespSerializer::bulk_string(info_str);
    }
    case CommandType::Command: {
        // Modern redis-cli sends COMMAND or COMMAND DOCS upon connection
        return RespSerializer::empty_array();
    }
    case CommandType::Quit: {
        should_close = true;
        return RespSerializer::ok();
    }
    case CommandType::Shutdown: {
        should_close = true;
        std::thread([this]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            stop();
        }).detach();
        return RespSerializer::ok();
    }
    default:
        return RespSerializer::error("unknown command '" + cmd.name() + "'");
    }
}

} // namespace aegis
