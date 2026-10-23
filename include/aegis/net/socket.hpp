#pragma once

#include <cstdint>
#include <string>

namespace aegis {

class Socket {
public:
    Socket();
    explicit Socket(int fd);
    ~Socket();

    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;
    Socket(Socket&& other) noexcept;
    Socket& operator=(Socket&& other) noexcept;

    [[nodiscard]] int fd() const noexcept { return fd_; }
    [[nodiscard]] bool is_valid() const noexcept { return fd_ >= 0; }

    bool bind(const std::string& host, uint16_t port);
    bool listen(int backlog = 4096);
    int accept(std::string& client_ip, uint16_t& client_port);

    bool set_nonblocking(bool enable = true);
    bool set_reuseaddr(bool enable = true);
    bool set_reuseport(bool enable = true);
    bool set_tcp_nodelay(bool enable = true);

    void close();

    static Socket create_tcp();

private:
    int fd_{-1};
};

} // namespace aegis
