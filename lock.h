#ifndef LOCK_H_
#define LOCK_H_

#include <exception>
#include <pthread.h>
#include <semaphore.h>

//RAII包装了mutex_lock, semaphore, condition_variable
class sem{
public:
    sem(){
        if(sem_init(&m_sem, 0, 0)!=0)//initializes the unnamed semaphore at the address pointed to by sem. 
            throw std::exception();
    }
    sem(int num){
        if(sem_init(&m_sem, 0, num)!=0)
            throw std::exception();
    }
    ~sem(){
        sem_destroy(&m_sem);
    }
    bool wait(){
        return sem_wait(&m_sem) == 0;//decrements (locks) the semaphore pointed to by sem. if > 0, proceed and return, else wait
    }
    bool post(){
        return sem_post(&m_sem) == 0;
    }
private:
    sem_t m_sem;
};

class mtx_lock{
public:
    mtx_lock(){
        if(pthread_mutex_init(&m_mutex, NULL) != 0)
            throw std::exception();
    }
    ~mtx_lock(){
        pthread_mutex_destroy(&m_mutex);
    }
    bool lock(){
        return pthread_mutex_lock(&m_mutex) == 0;
    }
    bool unlock(){
        return pthread_mutex_unlock(&m_mutex) == 0;
    }
    pthread_mutex_t *get(){
        return &m_mutex;
    }
private:
    pthread_mutex_t m_mutex;
};

class cond{
public:
    cond(){
        if(pthread_cond_init(&m_cond, NULL) != 0)
            throw std::exception();
    }
    ~cond(){
        pthread_cond_destroy(&m_cond);
    }
    bool wait(pthread_mutex_t *m_mutex){
        int ret = pthread_cond_wait(&m_cond, m_mutex);
        return ret == 0;
    }
    bool timewait(pthread_mutex_t *m_mutex, struct timespec t){
        int ret = pthread_cond_timedwait(&m_cond, m_mutex, &t);
        return ret == 0;
    }
    bool signal(){
        return pthread_cond_signal(&m_cond) == 0;
    }
    bool broadcast(){
        return pthread_cond_broadcast(&m_cond) == 0;
    }
private:
    pthread_cond_t m_cond;
};

#endif