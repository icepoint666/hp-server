#include "timer_list.h"
#include <unistd.h>
#include <sys/epoll.h> /* epoll() */
#include <assert.h>

static int epollfd;

TimerList::TimerList(){
    head = NULL;
    tail = NULL;
}

TimerList::~TimerList(){
    Timer *tmp = head;
    while (tmp){
        head = tmp->next;
        delete tmp;
        tmp = head;
    }
}

void TimerList::add_timer(Timer *timer, Timer *lst_head)
{
    Timer *prev = lst_head;
    Timer *tmp = prev->next;
    while (tmp)
    {
        if (timer->expire < tmp->expire)
        {
            prev->next = timer;
            timer->next = tmp;
            tmp->prev = timer;
            timer->prev = prev;
            break;
        }
        prev = tmp;
        tmp = tmp->next;
    }
    if (!tmp)
    {
        prev->next = timer;
        timer->prev = prev;
        timer->next = NULL;
        tail = timer;
    }
}

void TimerList::add_timer(Timer *timer){

    if (!timer)
        return;
    if (!head){
        head = tail = timer;
        return;
    }
    if (timer->expire < head->expire){
        timer->next = head;
        head->prev = timer;
        head = timer;
        return;
    }
    add_timer(timer, head);
}


void TimerList::adjust_timer(Timer *timer){
    if (!timer)
        return;
    Timer *tmp = timer->next;
    if (!tmp || (timer->expire < tmp->expire))
        return;
    if (timer == head){
        head = head->next;
        head->prev = NULL;
        timer->next = NULL;
        add_timer(timer, head);
    }
    else{
        timer->prev->next = timer->next;
        timer->next->prev = timer->prev;
        add_timer(timer, timer->next);
    }
}

void TimerList::del_timer(Timer *timer){
    if (!timer)
        return;
    if ((timer == head) && (timer == tail)){
        delete timer;
        head = NULL;
        tail = NULL;
        return;
    }
    if (timer == head){
        head = head->next;
        head->prev = NULL;
        delete timer;
        return;
    }
    if (timer == tail){
        tail = tail->prev;
        tail->next = NULL;
        delete timer;
        return;
    }
    timer->prev->next = timer->next;
    timer->next->prev = timer->prev;
    delete timer;
}

void TimerList::tick(){
    if (!head)
        return;
    
    time_t cur = time(NULL);
    Timer *tmp = head;
    while (tmp){
        if (cur < tmp->expire)
            break;
        tmp->close_fd(tmp->conn_data);
        head = tmp->next;
        if (head)
            head->prev = NULL;
        delete tmp;
        tmp = head;
    }
}

void close_fd(timer_data *conn_data){
    epoll_ctl(epollfd, EPOLL_CTL_DEL, conn_data->sockfd, 0);
    assert(conn_data);
    close(conn_data->sockfd);
}