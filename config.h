#ifndef CONFIG_H_
#define CONFIG_H_

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
#define DAEMON 0                        //是否作为后台进程
#define LOG 1                           //是否输出log
#define LOG_ASYNC 1                     //日志写入方式，默认同步，sync: 0, async: 1
#define LOG_BLOCK_QUEUE_SIZE 800        //日志写入async时，block queue的大小

enum{
    SUCCESS = 0,
    LOG_INIT_ERROR = 1001,
    INVALID_PORT_NUMBER = 1002, 
    SOCKET_CREATE_ERROR = 1004,
    INIT_LINGER_ERROR = 1006,
    SOCKET_SET_OPTION_ERROR = 1101,
    SOCKET_BIND_ERROR = 1102,
    SOCKET_LISTEN_ERROR = 1103,
    SOCKET_ACCEPT_ERROR = 1104,
    INTERNAL_SERVER_BUSY = 1105,
    READ_SIGNAL_ERROR = 1200,
    FCNTL_FAILED = 1201,
    EPOLL_CREATE_FAILED = 1301,
    EPOLL_ADD_INVALID_FD = 1302,
    EPOLL_ADD_FAILED = 1303,
    EPOLL_MOD_INVALID_FD = 1304,
    EPOLL_MOD_FAILED = 1305,
    EPOLL_DEL_INVALID_FD = 1306,
    EPOLL_DEL_FAILED = 1307,
    EPOLL_WAIT_FAILED = 1308,
    INVALID_MAX_EVENTS_NUMBER = 1309
};


#endif
