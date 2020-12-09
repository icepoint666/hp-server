#ifndef TCP_CONN_H
#define TCP_CONN_H

#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define READ_BUFFER_SIZE 2048
#define WRITE_BUFFER_SIZE 1024  //之后要单纯分离出一个config.h

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