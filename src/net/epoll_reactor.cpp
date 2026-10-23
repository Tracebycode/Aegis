#include "aegis/net/epoll_reactor.hpp"

#include <unistd.h>

namespace aegis {

EpollReactor::EpollReactor(int max_events)
    : max_events_(max_events) {
    epoll_fd_ = ::epoll_create1(EPOLL_CLOEXEC);
}

EpollReactor::~EpollReactor() {
    if (epoll_fd_ >= 0) {
        ::close(epoll_fd_);
        epoll_fd_ = -1;
    }
}

bool EpollReactor::add_fd(int fd, uint32_t events, void* ptr) {
    if (epoll_fd_ < 0 || fd < 0) return false;

    epoll_event ev{};
    ev.events = events;
    if (ptr) {
        ev.data.ptr = ptr;
    } else {
        ev.data.fd = fd;
    }

    return ::epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, fd, &ev) == 0;
}

bool EpollReactor::modify_fd(int fd, uint32_t events, void* ptr) {
    if (epoll_fd_ < 0 || fd < 0) return false;

    epoll_event ev{};
    ev.events = events;
    if (ptr) {
        ev.data.ptr = ptr;
    } else {
        ev.data.fd = fd;
    }

    return ::epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, fd, &ev) == 0;
}

bool EpollReactor::remove_fd(int fd) {
    if (epoll_fd_ < 0 || fd < 0) return false;
    return ::epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, nullptr) == 0;
}

int EpollReactor::wait(std::vector<epoll_event>& active_events, int timeout_ms) {
    if (epoll_fd_ < 0) return -1;

    active_events.resize(static_cast<size_t>(max_events_));
    int nfds = ::epoll_wait(epoll_fd_, active_events.data(), max_events_, timeout_ms);
    if (nfds > 0) {
        active_events.resize(static_cast<size_t>(nfds));
    } else {
        active_events.clear();
    }
    return nfds;
}

} // namespace aegis
