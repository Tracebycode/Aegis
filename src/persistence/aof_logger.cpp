#include "aegis/persistence/aof_logger.hpp"
#include "aegis/protocol/parser.hpp"
#include "aegis/protocol/serializer.hpp"

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <iostream>

namespace aegis {

AofLogger::AofLogger(std::string filepath, FsyncPolicy policy)
    : filepath_(std::move(filepath)), policy_(policy) {}

AofLogger::~AofLogger() {
    close();
}

bool AofLogger::open() {
    if (is_open_.load()) {
        return true;
    }

    fd_ = ::open(filepath_.c_str(), O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd_ < 0) {
        return false;
    }

    is_open_.store(true);

    if (policy_ != FsyncPolicy::Always) {
        flush_thread_ = std::jthread([this](std::stop_token stop_token) {
            flush_worker(stop_token);
        });
    }

    return true;
}

void AofLogger::close() {
    if (!is_open_.exchange(false)) {
        return;
    }

    if (flush_thread_.joinable()) {
        flush_thread_.request_stop();
        cv_.notify_all();
        flush_thread_.join();
    }

    // Flush any leftover data
    std::string remaining;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        remaining = std::move(active_buffer_);
    }

    if (!remaining.empty() && fd_ >= 0) {
        write_buffer_to_disk(remaining);
    }

    if (fd_ >= 0) {
        ::fdatasync(fd_);
        ::close(fd_);
        fd_ = -1;
    }
}

void AofLogger::append(const Command& cmd) {
    if (!is_open_.load() || cmd.empty()) {
        return;
    }

    // Format command as standard RESP array
    std::string resp_cmd = RespSerializer::array(cmd.args());
    append_raw(resp_cmd);
}

void AofLogger::append_raw(std::string_view raw_resp) {
    if (!is_open_.load() || raw_resp.empty()) {
        return;
    }

    if (policy_ == FsyncPolicy::Always) {
        std::lock_guard<std::mutex> lock(mutex_);
        write_buffer_to_disk(std::string(raw_resp));
        if (fd_ >= 0) {
            ::fdatasync(fd_);
        }
        return;
    }

    {
        std::lock_guard<std::mutex> lock(mutex_);
        active_buffer_.append(raw_resp);
    }
    cv_.notify_one();
}

void AofLogger::sync() {
    std::string to_write;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        to_write = std::move(active_buffer_);
    }

    if (!to_write.empty() && fd_ >= 0) {
        write_buffer_to_disk(to_write);
    }

    if (fd_ >= 0) {
        ::fdatasync(fd_);
    }
}

void AofLogger::write_buffer_to_disk(const std::string& buffer) {
    size_t written = 0;
    while (written < buffer.size() && fd_ >= 0) {
        ssize_t res = ::write(fd_, buffer.data() + written, buffer.size() - written);
        if (res <= 0) {
            break;
        }
        written += static_cast<size_t>(res);
    }
}

void AofLogger::flush_worker(std::stop_token stop_token) {
    while (!stop_token.stop_requested()) {
        {
            std::unique_lock<std::mutex> lock(mutex_);
            if (policy_ == FsyncPolicy::EverySec) {
                cv_.wait_for(lock, stop_token, std::chrono::seconds(1), [this] {
                    return !active_buffer_.empty();
                });
            } else {
                cv_.wait(lock, stop_token, [this] {
                    return !active_buffer_.empty();
                });
            }

            flush_buffer_ = std::move(active_buffer_);
            active_buffer_.clear();
        }

        if (!flush_buffer_.empty()) {
            write_buffer_to_disk(flush_buffer_);
            flush_buffer_.clear();

            if (policy_ == FsyncPolicy::EverySec && fd_ >= 0) {
                ::fdatasync(fd_);
            }
        }
    }
}

bool AofLogger::replay(ShardedHashMap& storage) {
    int read_fd = ::open(filepath_.c_str(), O_RDONLY);
    if (read_fd < 0) {
        return false; // File doesn't exist yet, clean start
    }

    std::string file_content;
    char chunk[4096];
    ssize_t bytes_read = 0;
    while ((bytes_read = ::read(read_fd, chunk, sizeof(chunk))) > 0) {
        file_content.append(chunk, static_cast<size_t>(bytes_read));
    }
    ::close(read_fd);

    RespParser parser;
    std::string_view sv = file_content;
    size_t consumed = 0;
    Command cmd;

    while (!sv.empty()) {
        ParseResult res = parser.parse(sv, consumed, cmd);
        if (res != ParseResult::Success || consumed == 0) {
            break;
        }

        sv.remove_prefix(consumed);

        // Apply mutating command
        switch (cmd.type()) {
        case CommandType::Set: {
            if (cmd.arg_count() >= 2) {
                uint64_t expire_at_ms = 0;
                if (cmd.arg_count() >= 4) {
                    std::string opt = cmd.arg(2);
                    for (char& c : opt) c = static_cast<char>(std::toupper(c));
                    try {
                        if (opt == "EX") {
                            expire_at_ms = now_ms() + (std::stoull(cmd.arg(3)) * 1000);
                        } else if (opt == "PX") {
                            expire_at_ms = now_ms() + std::stoull(cmd.arg(3));
                        }
                    } catch (...) {}
                }
                storage.set(cmd.arg(0), cmd.arg(1), expire_at_ms);
            }
            break;
        }
        case CommandType::Del: {
            for (size_t i = 0; i < cmd.arg_count(); ++i) {
                storage.del(cmd.arg(i));
            }
            break;
        }
        case CommandType::Expire: {
            if (cmd.arg_count() >= 2) {
                try {
                    storage.expire(cmd.arg(0), std::stoull(cmd.arg(1)));
                } catch (...) {}
            }
            break;
        }
        case CommandType::Pexpire: {
            if (cmd.arg_count() >= 2) {
                try {
                    storage.pexpire(cmd.arg(0), std::stoull(cmd.arg(1)));
                } catch (...) {}
            }
            break;
        }
        case CommandType::FlushAll: {
            storage.flushall();
            break;
        }
        default:
            break;
        }
    }

    return true;
}

} // namespace aegis
