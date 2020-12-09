#ifndef EPOLLER_H_
#define EPOLLER_H_

#include <sys/epoll.h> /* epoll() */
#include <fcntl.h>     /* fnctl() */
#include <unistd.h>    /* close(): close fd*/
#include <vector>
#include <assert.h>    /* assert() */

#include "../error_no.h"

class Epoller {
public:
    Epoller(int max_event = 1024);
    ~Epoller();

    int add_fd(int fd, uint32_t events);
    int mod_fd(int fd, uint32_t events);
    int del_fd(int fd);
        
    int epoll_fd;
    std::vector<struct epoll_event> events_; //epoll内核事件表
};


#endif