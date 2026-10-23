#include "aegis/net/server.hpp"

#include <arpa/inet.h>
#include <gtest/gtest.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <chrono>
#include <string>
#include <thread>

namespace {

int connect_to_server(const std::string& host, uint16_t port) {
    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return -1;

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    ::inet_pton(AF_INET, host.c_str(), &addr.sin_addr);

    for (int retry = 0; retry < 10; ++retry) {
        if (::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0) {
            return fd;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    ::close(fd);
    return -1;
}

std::string send_and_receive(int fd, const std::string& request) {
    ::send(fd, request.data(), request.size(), 0);

    char buf[1024];
    ssize_t n = ::recv(fd, buf, sizeof(buf) - 1, 0);
    if (n > 0) {
        buf[n] = '\0';
        return std::string(buf, static_cast<size_t>(n));
    }
    return "";
}

} // namespace

TEST(ServerIntegrationTest, FullRedisCommandFlow) {
    aegis::ServerConfig config;
    config.host = "127.0.0.1";
    config.port = 16379;
    config.worker_threads = 2;
    config.num_shards = 16;

    aegis::Server server(config);
    ASSERT_TRUE(server.start());

    int client_fd = connect_to_server(config.host, config.port);
    ASSERT_GE(client_fd, 0);

    // 1. PING
    std::string resp = send_and_receive(client_fd, "*1\r\n$4\r\nPING\r\n");
    EXPECT_EQ(resp, "+PONG\r\n");

    // 2. SET key val
    resp = send_and_receive(client_fd, "*3\r\n$3\r\nSET\r\n$3\r\nfoo\r\n$3\r\nbar\r\n");
    EXPECT_EQ(resp, "+OK\r\n");

    // 3. GET key
    resp = send_and_receive(client_fd, "*2\r\n$3\r\nGET\r\n$3\r\nfoo\r\n");
    EXPECT_EQ(resp, "$3\r\nbar\r\n");

    // 4. EXISTS key
    resp = send_and_receive(client_fd, "*2\r\n$6\r\nEXISTS\r\n$3\r\nfoo\r\n");
    EXPECT_EQ(resp, ":1\r\n");

    // 5. DBSIZE
    resp = send_and_receive(client_fd, "*1\r\n$6\r\nDBSIZE\r\n");
    EXPECT_EQ(resp, ":1\r\n");

    // 6. DEL key
    resp = send_and_receive(client_fd, "*2\r\n$3\r\nDEL\r\n$3\r\nfoo\r\n");
    EXPECT_EQ(resp, ":1\r\n");

    // 7. GET after DEL -> null bulk
    resp = send_and_receive(client_fd, "*2\r\n$3\r\nGET\r\n$3\r\nfoo\r\n");
    EXPECT_EQ(resp, "$-1\r\n");

    // 8. QUIT
    resp = send_and_receive(client_fd, "*1\r\n$4\r\nQUIT\r\n");
    EXPECT_EQ(resp, "+OK\r\n");

    ::close(client_fd);
    server.stop();
}
