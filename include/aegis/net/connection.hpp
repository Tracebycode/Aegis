#pragma once

#include "aegis/common/ring_buffer.hpp"
#include "aegis/net/socket.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

namespace aegis {

class Connection {
public:
    Connection(Socket socket, std::string client_ip, uint16_t client_port);
    ~Connection() = default;

    Connection(const Connection&) = delete;
    Connection& operator=(const Connection&) = delete;

    [[nodiscard]] int fd() const noexcept { return socket_.fd(); }
    [[nodiscard]] const std::string& client_ip() const noexcept { return client_ip_; }
    [[nodiscard]] uint16_t client_port() const noexcept { return client_port_; }
    [[nodiscard]] bool is_closed() const noexcept { return is_closed_; }

    // Socket I/O
    ssize_t read_from_socket();
    ssize_t write_to_socket();

    void queue_response(std::string_view resp);
    [[nodiscard]] bool has_pending_write() const noexcept { return !write_buffer_.empty(); }

    [[nodiscard]] ByteBuffer& read_buffer() noexcept { return read_buffer_; }
    [[nodiscard]] const ByteBuffer& read_buffer() const noexcept { return read_buffer_; }

    void close() noexcept {
        is_closed_ = true;
        socket_.close();
    }

private:
    Socket socket_;
    std::string client_ip_;
    uint16_t client_port_;
    ByteBuffer read_buffer_;
    ByteBuffer write_buffer_;
    bool is_closed_{false};
};

} // namespace aegis
