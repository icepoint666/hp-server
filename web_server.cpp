#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <iostream>

#include "web_server.h"
#include "tcp_conn.h"


#if __cplusplus >= 201402L
    #include <utility>
    using namespace std::make_unique;
#else
    template<class T, class... Args>
    std::unique_ptr<T> make_unique(Args&&... args){
        return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
    }
#endif

static int pipefd; //暂时设置成了全局变量，如果可以把member function作为函数参数传入，那么就可以不使用这个全局变量
static int epollfd; //暂时设置成了全局变量，便于timer调用

HttpServer::HttpServer(){ //构造函数中不检查 + 抛出异常，放在后面检查
    strcpy(host, "0.0.0.0");
    port = DEFAULT_HTTP_PORT;
    ssl = 0;
    timewait = -1; //epoll timewait = -1无阻塞，timewait = 60000ms = 60s
    http_version = 1;
    listenfd = -1;
    actor_mode = ACTOR_MODE; 
    epoller_ = make_unique<Epoller>(MAX_EVENT_NUMBER);
    users = new tcp_conn[MAX_FD];
    conn_datas = new timer_data[MAX_FD];
    
}

HttpServer::~HttpServer(){
    close(epoller_->epoll_fd);
    close(listenfd);
    close(pipe_fd[0]);
    close(pipe_fd[1]);
    delete[] users;
    delete[] conn_datas;
    delete pool_;
}

int HttpServer::init_socket(){
    int ret;
    struct sockaddr_in addr;
    if(port > 65535 || port < 1024) {
        return INVALID_PORT_NUMBER;
    }
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);
    if((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){ //运算符优先级一定要注意赋值 低于 比较
        return SOCKET_CREATE_ERROR;
    }

    /* Linger: 优雅关闭: 直到所剩数据发送完毕或超时 */
    struct linger opt_linger = {1, 1};
    if(setsockopt(listenfd, SOL_SOCKET, SO_LINGER, &opt_linger, sizeof(opt_linger)) < 0){
        close(listenfd); //LOG_ERROR("Init linger error!");
        return INIT_LINGER_ERROR;
    }

    /* ReuseAddr: 端口复用：只有最后一个套接字会正常接收数据 */
    int optval = 1;
    if(setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, (const void*)&optval, sizeof(int)) < 0) {
        close(listenfd);
        return SOCKET_SET_OPTION_ERROR;
    }

    if(bind(listenfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(listenfd); //LOG_ERROR("Bind Port:%d error!", port_);
        return SOCKET_BIND_ERROR;
    }

    if(listen(listenfd, 6) < 0) { //set backlog uplimit 6
        close(listenfd); //LOG_ERROR("Listen port:%d error!", port_);
        return SOCKET_LISTEN_ERROR;
    }

    timer_.init(TIMESLOT);
    if(set_nonblocking(listenfd) < 0){
        close(listenfd);
        return FCNTL_FAILED;
    }
    return 0;
}

void HttpServer::init_thread_pool(int actor_mode, int thread_number, int max_request){
    pool_ = new threadpool<tcp_conn>(*this, actor_mode, thread_number, max_request);
}

void HttpServer::init_event_mode(int trig_mode) {
    listen_event_ = EPOLLRDHUP;
    conn_event_   = EPOLLONESHOT | EPOLLRDHUP;
    switch (trig_mode){
    case 0: //Listen: LT, Conn: LT
        break;
    case 1: //Listen: LT, Conn: ET
        conn_event_ |= EPOLLET;
        break;
    case 2: //Listen: LT, Conn: ET
        listen_event_ |= EPOLLET;
        break;
    case 3: //Listen: ET, Conn: ET
        listen_event_ |= EPOLLET;
        conn_event_ |= EPOLLET;
        break;
    default:
        listen_event_ |= EPOLLET;
        conn_event_ |= EPOLLET;
        break;
    }
    //LOG_INFO("Trigger mode:", ...);
}

int HttpServer::init_epoller(){
    if(epoller_->epoll_fd < 0)
        return EPOLL_CREATE_FAILED;
    if(epoller_->events_.size() == 0)
        return INVALID_MAX_EVENTS_NUMBER;
    int ret = 0;
    epollfd = epoller_->epoll_fd; //设置静态变量
    if(ret = epoller_->add_fd(listenfd, listen_event_ | EPOLLIN))
        return ret;
}

void sig_handler(int sig){
    int save_errno = errno; //信号处理器会中断原来运行程序，为保证函数的可重入性，保留原来的errno
    int msg = sig;
    send(pipefd, (char *)&msg, 1, 0);
    errno = save_errno; //再存放回去
}

void HttpServer::add_signal(int sig, void(handler)(int), bool restart){
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = handler;
    if (restart)
        sa.sa_flags |= SA_RESTART;
    sigfillset(&sa.sa_mask);
    assert(sigaction(sig, &sa, NULL) != -1);
}

void HttpServer::init_signal_handler(){
    //socketpair()函数用于创建一对无名的、相互连接的socket
    int ret = socketpair(PF_UNIX, SOCK_STREAM, 0, pipe_fd);
    assert(ret != -1);
    set_nonblocking(pipe_fd[1]);
    epoller_->add_fd(pipe_fd[0], EPOLLIN | EPOLLRDHUP);

    signal(SIGPIPE, SIG_IGN);
    add_signal(SIGALRM, sig_handler, false);
    add_signal(SIGTERM, sig_handler, false);
    
    alarm(TIMESLOT);
    pipefd = pipe_fd[1]; //设置静态变量
}

int HttpServer::event_loop(){
    bool timeout = false;
    bool stop_server = false;
    int ret;
    while (!stop_server){
        int number = epoll_wait(epoller_->epoll_fd, &epoller_->events_[0], static_cast<int>(epoller_->events_.size()), timewait);
        if (number < 0 && errno != EINTR){
            return EPOLL_WAIT_FAILED;
        }

        for (int i = 0; i < number; i++){
            int sockfd = epoller_->events_[i].data.fd;
            if (sockfd == listenfd){ //处理客户的连接
                if(handle_accept() > 0)
                    continue;
            }
            else if (epoller_->events_[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {//处理客户端连接关闭的情况
                Timer *timer = conn_datas[sockfd].timer;
                delete_timer(timer, sockfd);//服务器端关闭连接，并移除对应的定时器
            }
            else if ((sockfd == pipe_fd[0]) && (epoller_->events_[i].events & EPOLLIN)) {//处理信号
                if(handle_signal(timeout, stop_server) > 0)
                    continue;
                    //LOG_ERROR("%s", "Handle Signal Failure!");
            }
            else if (epoller_->events_[i].events & EPOLLIN) { //处理从客户连接上接收到的数据
                if(handle_read(sockfd) > 0)
                    continue;
            }
            else if (epoller_->events_[i].events & EPOLLOUT) { //处理向客户连接上发送数据
                if(handle_write(sockfd) > 0)
                    continue;
            }
        }
        if(timeout){
            timer_.timer_handler();
            timeout = false;
            //LOG_INFO("%s", "timer tick");
        }
    }
}

void HttpServer::init_timer(int connfd, struct sockaddr_in client_address){
    conn_datas[connfd].address = client_address;
    conn_datas[connfd].sockfd = connfd;
    Timer *timer = new Timer();
    timer->conn_data = &conn_datas[connfd];
    timer->close_fd = close_fd;
    time_t cur = time(NULL);
    timer->expire = cur + 3 * TIMESLOT;
    conn_datas[connfd].timer = timer;
    timer_.timer_lst.add_timer(timer);
}

void HttpServer::adjust_timer(Timer *timer){
    time_t cur = time(NULL);
    timer->expire = cur + 3 * TIMESLOT;
    timer_.timer_lst.adjust_timer(timer);
    //LOG_INFO("%s", "adjust timer once");
}

void HttpServer::delete_timer(Timer *timer, int sockfd){
    timer->close_fd(&conn_datas[sockfd]);
    if (timer)timer_.timer_lst.del_timer(timer);
    //LOG_INFO("close fd %d", users_timer[sockfd].sockfd);
}

int HttpServer::handle_accept(){
    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);
    int fd;
    do { //第一次执行想当于是LT模式
        if((fd = accept(listenfd, (struct sockaddr *)&addr, &len)) < 0)
            return SOCKET_ACCEPT_ERROR;
    } while(listen_event_ & EPOLLET); //之后循环相当于都是ET模式的处理方式
    epoller_->add_fd(fd, EPOLLIN | conn_event_);
    set_nonblocking(fd);
    init_user(users+fd, fd, addr, conn_event_);
    init_timer(fd, addr);
    return 0;
}

int HttpServer::handle_signal(bool &timeout, bool &stop_server){
    int ret, sig;
    char signals[1024];
    if((ret = recv(pipe_fd[0], signals, sizeof(signals), 0)) <= 0)
        return READ_SIGNAL_ERROR;
    for (int i = 0; i < ret; ++i){
        switch (signals[i]){
            case SIGALRM: timeout = true;break;
            case SIGTERM: stop_server = true;break;
        }
    }
    return 0;
}

int HttpServer::handle_read(int fd){
    Timer *timer = conn_datas[fd].timer;
    if (!actor_mode) { //reactor
        if(timer)adjust_timer(timer);
        pool_->append(users + fd, 0);
        while (true){ //同步I/O: 轮询直到该read被工作队列处理
            if (users[fd].improv){
                if (users[fd].timer_flag){
                    delete_timer(timer, fd);
                    users[fd].timer_flag = 0;
                }
                users[fd].improv = 0;
                break;
            }
        }
    }
    else { //proactor
        if (read_once(users + fd)){ //read_once如果成功，保证这次读必然是成功的
            //LOG_INFO("deal with the client(%s)", inet_ntoa(u[sockfd].get_address()->sin_addr));
            pool_->append(users + fd, -1); //异步交给工作线程来处理
            if(timer)adjust_timer(timer);
        }else{
            delete_timer(timer, fd);
        }
    }
    return 0;
}

int HttpServer::handle_write(int fd){
    Timer *timer = conn_datas[fd].timer;
    if (!actor_mode) { //reactor
        if(timer)adjust_timer(timer);
        pool_->append(users + fd, 1);
        while (true){
            if (users[fd].improv){
                if (users[fd].timer_flag){
                    delete_timer(timer, fd);
                    users[fd].timer_flag = 0;
                }
                users[fd].improv = 0;
                break;
            }
        }
    }
    else { //proactor
        if (write_(users+fd)){
            //LOG_INFO("send data to the client(%s)", inet_ntoa(users[sockfd].get_address()->sin_addr));
            if(timer)adjust_timer(timer);
        }else{
            delete_timer(timer, fd);
        }
    }
    return 0;
}

void HttpServer::init_user(tcp_conn* user, int fd, const sockaddr_in &addr, uint32_t conn_event_){
    user->sockfd = fd;
    user->address = addr;
    user->conn_event = conn_event_;
    init_user(user);
}

void HttpServer::init_user(tcp_conn* user){
    user->bytes_to_send = 0;
    user->read_idx = 0;
    user->write_idx = 0;
    user->state = 0;
    user->timer_flag = 0;
    user->improv = 0;

    memset(user->read_buf, '\0', READ_BUFFER_SIZE);
    memset(user->write_buf, '\0', WRITE_BUFFER_SIZE);
}

//循环读取客户数据，直到无数据可读或对方关闭连接
//非阻塞ET工作模式下，需要一次性将数据读完
bool HttpServer::read_once(tcp_conn* user){
    if (user->read_idx >= READ_BUFFER_SIZE)
        return false;
    int bytes_read = 0;
    //LT / ET处理数据
    do {
        bytes_read = recv(user->sockfd, user->read_buf + user->read_idx, READ_BUFFER_SIZE - user->read_idx, 0);//0：stream没有要读的数据，-1：I/O不可用（或者其他错误）
        if(user->conn_event & EPOLLET){ //ET
            if(bytes_read == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) //对于ET来说，读到EAGAIN是正确返回
                break;
        }else{ //LT +=
            user->read_idx += bytes_read;
        }
        if(bytes_read <= 0)//LT / ET：前面已经判断过ET的EAGAIN情况，所以到这里的都是出现read错误的情况
            return false;
        if(user->conn_event & EPOLLET)//ET +=
            user->read_idx += bytes_read;
    }while(user->conn_event & EPOLLET);
    return true;
}

bool HttpServer::write_(tcp_conn* user){
    std::cout << user->sockfd << " " << user->bytes_to_send << std::endl;
    int temp = 0;
    if (user->bytes_to_send == 0){
        epoller_->mod_fd(user->sockfd, EPOLLIN | user->conn_event);
        init_user(user);
        return true;
    }
    while(1){
        temp = write(user->sockfd, user->write_buf + user->write_idx, user->bytes_to_send - user->write_idx);
        if (temp < 0 && errno == EAGAIN){
            epoller_->mod_fd(user->sockfd, EPOLLOUT | user->conn_event);
            return true;
        }
        if(temp < 0) return false;
        user->bytes_to_send -= temp;
        user->write_idx += temp;

        if (user->bytes_to_send <= 0){
            epoller_->mod_fd(user->sockfd, EPOLLIN | user->conn_event);
            return false;
        }
    }
}

void HttpServer::process_write(tcp_conn* user){
    memcpy(user->write_buf, user->read_buf, (unsigned)std::min(user->read_idx, WRITE_BUFFER_SIZE));
    user->bytes_to_send = std::min(user->read_idx, WRITE_BUFFER_SIZE);
}

void HttpServer::process(tcp_conn* user)
{
    //process_read();//默认一次读完
    // if (read_ret == NO_REQUEST){
    //     mod_fd(sockfd, EPOLLIN | conn_event);
    //     return;
    // }
    process_write(user); //如果写入失败关闭连接
    // if (!write_ret)
    //     close_conn();
    epoller_->mod_fd(user->sockfd, EPOLLOUT | user->conn_event);
}


int set_nonblocking(int fd) { //设置套接字为非阻塞模式，否则recv, send必须要求读完所有字节才返回
    int flags = fcntl(fd, F_GETFL, 0);
    if(fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0)
        return -1;
    return 0;
}

int http_server_run(HttpServer& server){
    int ret = 0;
    if(ret = server.init_socket())
        return ret;
    server.init_thread_pool(INIT_TRIG_MODE, THREAD_NUMBER, WORKQUEUE_MAX_REQUESTS);
    server.init_event_mode(INIT_TRIG_MODE);
    if(ret = server.init_epoller())
        return ret;
    server.init_signal_handler();
    if(ret = server.event_loop())
        return ret;
}