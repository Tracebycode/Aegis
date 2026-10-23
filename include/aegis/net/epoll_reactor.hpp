#pragma once

#include <cstdint>
#include <sys/epoll.h>
#include <vector>

namespace aegis {

class EpollReactor {
public:
    explicit EpollReactor(int max_events = 1024);
    ~EpollReactor();

    EpollReactor(const EpollReactor&) = delete;
    EpollReactor& operator=(const EpollReactor&) = delete;

    bool add_fd(int fd, uint32_t events, void* ptr = nullptr);
    bool modify_fd(int fd, uint32_t events, void* ptr = nullptr);
    bool remove_fd(int fd);

    // Waits for events, populating active_events. Returns number of events ready.
    int wait(std::vector<epoll_event>& active_events, int timeout_ms);

    [[nodiscard]] int fd() const noexcept { return epoll_fd_; }
    [[nodiscard]] bool is_valid() const noexcept { return epoll_fd_ >= 0; }

private:
    int epoll_fd_{-1};
    int max_events_;
};

} // namespace aegis
