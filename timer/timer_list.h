#ifndef TIMER_LIST_H_
#define TIMER_LIST_H_

#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <time.h>
#include <netinet/in.h>
#include <arpa/inet.h>

class Timer;

struct timer_data{
    sockaddr_in address;
    int sockfd;
    Timer *timer;
};

class Timer{
public:
    Timer() : prev(NULL), next(NULL) {}

public:
    time_t expire;
    void (* close_fd)(timer_data *);
    timer_data *conn_data;
    Timer *prev;
    Timer *next;
};

class TimerList{
public:
    TimerList();
    ~TimerList();

    void add_timer(Timer *timer);
    void adjust_timer(Timer *timer);
    void del_timer(Timer *timer);
    void tick();

private:
    void add_timer(Timer *timer, Timer *lst_head);
    Timer *head;
    Timer *tail;
};

void close_fd(timer_data *conn_data);

#endif