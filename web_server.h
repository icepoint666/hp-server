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
#include "threadpool.h"
#include "error_no.h"
#include "applica/tcp_conn.h"

//之后要单纯分离出一个config.h
#define MAX_FD 65536                    //最大文件描述符
#define MAX_EVENT_NUMBER 10000          //一次epoll_wait最大同时返回的事件数
#define DEFAULT_HTTP_PORT 80            //默认端口
#define TIMESLOT 5                      //最小超时单位
#define INIT_TRIG_MODE 0                //LT / ET触发模式
#define ACTOR_MODE 0                    //reactor: 0, preactor: 1
#define WORKQUEUE_MAX_REQUESTS 10000    //请求队列中允许的最大请求数
#define THREAD_NUMBER 8                 //工作线程数
#define READ_BUFFER_SIZE 2048
#define WRITE_BUFFER_SIZE 1024         

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