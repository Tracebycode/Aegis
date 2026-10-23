#include "aegis/net/connection.hpp"

#include <cerrno>
#include <sys/socket.h>

namespace aegis {

Connection::Connection(Socket socket, std::string client_ip, uint16_t client_port)
    : socket_(std::move(socket)),
      client_ip_(std::move(client_ip)),
      client_port_(client_port) {
    socket_.set_nonblocking(true);
    socket_.set_tcp_nodelay(true);
}

ssize_t Connection::read_from_socket() {
    if (is_closed_ || !socket_.is_valid()) {
        return 0;
    }

    read_buffer_.ensure_writable(4096);
    ssize_t n = ::recv(socket_.fd(), read_buffer_.write_ptr(),
                       read_buffer_.writable_bytes(), 0);

    if (n > 0) {
        read_buffer_.advance_write(static_cast<size_t>(n));
        return n;
    }

    if (n == 0) {
        // EOF
        is_closed_ = true;
        return 0;
    }

    if (errno == EAGAIN || errno == EWOULDBLOCK) {
        return -1;
    }

    // Actual error
    is_closed_ = true;
    return -1;
}

ssize_t Connection::write_to_socket() {
    if (is_closed_ || !socket_.is_valid() || write_buffer_.empty()) {
        return 0;
    }

    ssize_t n = ::send(socket_.fd(), write_buffer_.read_ptr(),
                       write_buffer_.readable_bytes(), MSG_NOSIGNAL);

    if (n > 0) {
        write_buffer_.advance_read(static_cast<size_t>(n));
        return n;
    }

    if (errno == EAGAIN || errno == EWOULDBLOCK) {
        return -1;
    }

    is_closed_ = true;
    return -1;
}

void Connection::queue_response(std::string_view resp) {
    write_buffer_.write(resp);
}

} // namespace aegis
