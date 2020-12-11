/* 循环数组实现的阻塞队列:
 * m_back = (m_back + 1) % m_max_size;
 * 线程安全，每个操作前都要先加互斥锁，操作完后，再解锁
 */
#ifndef BLOCK_QUEUE_H_
#define BLOCK_QUEUE_H_

#include <assert.h>
#include <pthread.h>
#include <pthread.h>
#include <sys/time.h>

#include <../lock.h>

template <typename T>
class block_queue{
public:
    block_queue(int max_size_ = 1000){
        assert(max_size_ > 0);
        max_size = max_size_;
        arr = new T[max_size];
        size = 0;
        front_ = -1;
        back_ = -1; 
    }
    ~block_queue(){
        mtx.lock();
        if(arr != NULL)
            delete [] arr;
        mtx.unlock();
    }
    bool full(){
        mtx.lock();
        if(size >= max_size){
            mtx.unlock();
            return true;
        }
        mtx.unlock();
        return false;
    }
    bool empty(){
        mtx.lock();
        if(size == 0){
            mtx.unlock();
            return true;
        }
        mtx.unlock();
        return false;
    }
    bool front(T& val){
        mtx.lock();
        if(size == 0){
            mtx.unlock();
            return false;
        }
        val = arr[front_];
        mtx.unlock();
        return true;
    }

    T* back(T& val){
        mtx.lock();
        if(size == 0){
            mtx.unlock();
            return false;
        }
        val = arr[back_];
        mtx.unlock();
        return true;
    }
    int _size(){
        int tmp = 0;
        mtx.lock();
        tmp = size;
        mtx.unlock();
        return tmp;
    }
    int _max_size(){
        int tmp = 0;
        mtx.lock();
        tmp = _size();
        mtx.unlock();
        return tmp;
    }
    //当有元素push进队列,相当于生产者生产了一个元素,往队列添加元素，需要将所有使用队列的线程先唤醒,若当前没有线程等待条件变量,则唤醒无意义
    bool push(const T& item){
        mtx.lock();
        if(size >= max_size){
            c.broadcast();
            mtx.unlock();
            return false;
        }
        back_ = (back_ + 1) % max_size;
        arr[back_] = item;
        size++;

        c.broadcast();
        mtx.unlock();
        return true;
    }
    //pop时,如果当前队列没有元素,将会等待条件变量
    bool pop(T& item){
        mtx.lock();
        while(size <= 0){
            if(!c.wait(mtx.get())){
                mtx.unlock();
                return false;
            }
        }
        front_ = (front_ + 1) % max_size;
        item = arr[front_];
        size--;
        mtx.unlock();
        return true;
    }

    bool pop(T &item, int ms_timeout){
        struct timespec t = {0, 0};
        struct timeval now = {0, 0};
        gettimeofday(&now, NULL);
        mtx.lock();
        if(size <= 0){
            t.tv_sec = now.tv_sec + ms_timeout / 1000;
            t.tv_nsec = (ms_timeout % 1000) * 1000;
            if (!c.timewait(mtx.get(), t))
            {
                mtx.unlock();
                return false;
            }
        }
        if(size <= 0){
            mtx.unlock();
            return false;
        }
        front_ = (front_ + 1) % max_size;
        item = arr[front_];
        size--;
        mtx.unlock();
        return true;
    }
    bool clear(){
        mtx.lock();
        size = 0;
        front_ = -1;
        back_ = -1;
        mtx.unlock();
    }

private:
    mtx_lock mtx;
    cond c;
    T* arr;
    int size;
    int max_size;
    int front_;
    int back_;
};

#endif