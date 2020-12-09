#include "epoller.h"

Epoller::Epoller(int max_event) {
    epoll_fd = epoll_create(512);
    events_.resize(max_event);
}

Epoller::~Epoller() {
    close(epoll_fd);
}

int Epoller::add_fd(int fd, uint32_t events) {
    if(fd < 0) return EPOLL_ADD_INVALID_FD;
    epoll_event ev = {0};
    ev.data.fd = fd;
    ev.events = events;
    if(epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &ev))
        return EPOLL_ADD_FAILED;
    return 0;
}

int Epoller::mod_fd(int fd, uint32_t events) {
    if(fd < 0) return EPOLL_MOD_INVALID_FD;
    epoll_event ev = {0};
    ev.data.fd = fd;
    ev.events = events;
    if(epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &ev))
        return EPOLL_MOD_FAILED;
    return 0;
}

int Epoller::del_fd(int fd) {
    if(fd < 0) return EPOLL_DEL_INVALID_FD;
    epoll_event ev = {0};
    if(epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, &ev))
        return EPOLL_DEL_FAILED;
    return 0;
}
