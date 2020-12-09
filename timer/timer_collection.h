#ifndef TIMER_COLLECTION_H_
#define TIMER_COLLECTION_H_

#include <unistd.h>
#include <signal.h>
#include "timer_list.h"

class TimerCollection{
public:
    TimerList timer_lst;
    int m_TIMESLOT;
    void init(int timeslot);
    void timer_handler();
};

#endif