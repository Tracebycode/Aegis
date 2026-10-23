#include "aegis/common/ring_buffer.hpp"
#include "aegis/net/epoll_reactor.hpp"
#include "aegis/net/socket.hpp"

#include <gtest/gtest.h>

TEST(NetTest, ByteBufferBasics) {
    aegis::ByteBuffer buf(64);
    EXPECT_TRUE(buf.empty());
    EXPECT_EQ(buf.readable_bytes(), 0);

    buf.write("Hello World");
    EXPECT_FALSE(buf.empty());
    EXPECT_EQ(buf.readable_bytes(), 11);
    EXPECT_EQ(buf.read_view(), "Hello World");

    buf.advance_read(6);
    EXPECT_EQ(buf.readable_bytes(), 5);
    EXPECT_EQ(buf.read_view(), "World");

    buf.advance_read(5);
    EXPECT_TRUE(buf.empty());
}

TEST(NetTest, SocketAndReactorLifecycle) {
    auto sock = aegis::Socket::create_tcp();
    ASSERT_TRUE(sock.is_valid());
    EXPECT_TRUE(sock.set_nonblocking(true));
    EXPECT_TRUE(sock.set_reuseaddr(true));
    EXPECT_TRUE(sock.set_tcp_nodelay(true));

    aegis::EpollReactor reactor(64);
    ASSERT_TRUE(reactor.is_valid());
    EXPECT_TRUE(reactor.add_fd(sock.fd(), EPOLLIN));
    EXPECT_TRUE(reactor.modify_fd(sock.fd(), EPOLLIN | EPOLLOUT));
    EXPECT_TRUE(reactor.remove_fd(sock.fd()));
}
