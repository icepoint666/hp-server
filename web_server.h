#ifndef WEB_SERVER_H_
#define WEB_SERVER_H_

#include <unistd.h>
#include <string.h>
#include <signal.h> /*signal相关函数*/
#include <errno.h> /*errno 是记录系统的最后一次错误代码。 代码是一个int型的值，在errno.h中定义，debug的时候这个很关键*/
#include <memory>  /*unique_ptr*/
#include <algorithm>

#include "epoll/epoller.h"
#include "timer/timer_list.h"
#include "timer/timer_collection.h"
#include "log/log.h"
#include "threadpool.h"
#include "config.h"
#include "applica/tcp_conn.h"     

class HttpServer {
public:
    char host[64];
    int port;
    int listenfd;
    int pipe_fd[2]; //pipe for handle signal
    int timewait; //epoll timewait, timewait = 10 * 1000: 在epoll_wait这里会阻塞，只等待10秒，超过10秒，epoll_wait函数结束（继续循环）
    int ssl;
    int http_version;
    int actor_mode;
    int close_log_flag;

    //多线程相关
    int worker_processes;
    int worker_threads;

    //epoll相关
    uint32_t listen_event_;
    uint32_t conn_event_;
    std::unique_ptr<Epoller> epoller_;

    //定时器相关
    timer_data* conn_datas;
    TimerCollection timer_;

    //tcp,http
    tcp_conn *users;
    threadpool<tcp_conn> *pool_;
    int n_thread;

    HttpServer();
    ~HttpServer();
    
    bool init_log();
    int init_socket();
    void init_thread_pool(int actor_mode, int thread_number, int max_request);
    void init_event_mode(int trig_mode);
    int init_epoller();
    int event_loop();

    void add_signal(int sig, void(handler)(int), bool restart);
    void init_signal_handler();
    
    void init_timer(int connfd, struct sockaddr_in client_address);
    void adjust_timer(Timer* timer);
    void delete_timer(Timer* timer, int sockfd);

    int handle_accept();
    int handle_signal(bool &timeout, bool &stop_server);
    int handle_read(int fd);
    int handle_write(int fd);

    void init_user(tcp_conn* user, int sockfd, const sockaddr_in &addr, uint32_t conn_event_);
    void init_user(tcp_conn* user);
    void process_write(tcp_conn* user);
    void process(tcp_conn* user);
    bool read_once(tcp_conn* user);
    bool write_(tcp_conn* user);

};

int set_nonblocking(int fd);
int http_server_run(HttpServer &server);

#endif