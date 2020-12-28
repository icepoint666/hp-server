#ifndef TCP_CONN_H
#define TCP_CONN_H

#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "../config.h"

class tcp_conn{
public:
    tcp_conn(){};
    ~tcp_conn(){};

    int bytes_to_send;
    int timer_flag;
    int improv;
    int state;
    uint32_t conn_event;
    int read_idx;
    int write_idx;

    int sockfd;
    sockaddr_in address;
    char read_buf[READ_BUFFER_SIZE];
    char write_buf[WRITE_BUFFER_SIZE];
};

#endif