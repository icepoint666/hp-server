#include "timer_collection.h"

void TimerCollection::init(int timeslot){
    m_TIMESLOT = timeslot;
}

void TimerCollection::timer_handler()
{
    timer_lst.tick();
    alarm(m_TIMESLOT);
}