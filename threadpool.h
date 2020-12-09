#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <list>
#include <cstdio>
#include <exception>
#include <pthread.h>

#include "lock.h"

class HttpServer; //forward declaration

template<typename T>
class threadpool{
public:
    threadpool(HttpServer& server, int actor_mode_, int n_thread_ = 8, int max_requests_ = 10000);
    ~threadpool();
    bool append(T* request, int state);

private:
    /*工作线程运行的函数，它不断从工作队列中取出任务并执行之*/
    static void *worker(void *arg);
    void run();
    HttpServer& server;

    int n_thread;            //线程池中的线程数
    int max_requests;        //请求队列中允许的最大请求数
    pthread_t *threads;      //描述线程池的数组，其大小为n_thread
    std::list<T*> workqueue; //请求队列
    mtx_lock queue_mtx_;     //请求队列的互斥锁
    sem queue_item;          //是否有任务需要处理
    int actor_mode;          //reactor/proactor模式
};

template<typename T>
threadpool<T>::threadpool(HttpServer& server_, int actor_mode_, int n_thread_, int max_requests_):
    server(server_), actor_mode(actor_mode_), n_thread(n_thread_), max_requests(max_requests_){
    if(n_thread <= 0 || max_requests <= 0)
        throw std::exception();
    threads = new pthread_t[n_thread];
    if(!threads)
        throw std::exception();
    for(int i = 0; i < n_thread; i++){
        if(pthread_create(threads+i, NULL, worker, this) != 0){
            delete[] threads;
            throw std::exception();
        }
        if(pthread_detach(threads[i])){ // pthread_detach()指定为unjoinable状态，也就是线程结束后资源会直接自动回收。在调用成功完成之后返回零。其他任何返回值都表示出现了错误
            delete[] threads;
            throw std::exception();
        }
    }
}
template<typename T>
threadpool<T>::~threadpool(){
    delete[] threads;
}
template<typename T>
bool threadpool<T>::append(T* request, int state){ //请求队列需要记录状态
    queue_mtx_.lock();
    if(workqueue.size() >= max_requests){
        queue_mtx_.unlock();
        return false;
    }
    if(state!=-1)request->state = state;
    workqueue.push_back(request);
    queue_mtx_.unlock();
    queue_item.post();
    return true;
}

template <typename T>
void *threadpool<T>::worker(void *arg){
    threadpool *pool = (threadpool *)arg;
    pool->run();
    return pool;
}
template <typename T>
void threadpool<T>::run()
{
    while (true){
        queue_item.wait();
        queue_mtx_.lock();
        if (workqueue.empty()){
            queue_mtx_.unlock();
            continue;
        }
        T *request = workqueue.front();
        workqueue.pop_front();
        queue_mtx_.unlock();
        if (!request)
            continue;
        if (!actor_mode){
            if (!request->state){//read
                if (server.read_once(request)){
                    request->improv = 1;
                    server.process(request);
                }
                else{
                    request->improv = 1;
                    request->timer_flag = 1;
                }
            }
            else{//write
                if (server.write_(request))
                    request->improv = 1;
                else{
                    request->improv = 1;
                    request->timer_flag = 1;
                }
            }
        }
        else{
            server.process(request);
        }
    }
}
#endif