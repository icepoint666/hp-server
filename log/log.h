#ifndef LOG_H_
#define LOG_H_

#include <stdio.h>
#include <string>
#include "block_queue.h"
#include "../lock.h"

#define LOG_DEBUG(format, ...) if(0 == close_log_flag) {Log::get_instance()->write_log(0, format, ##__VA_ARGS__); Log::get_instance()->flush();}
#define LOG_INFO(format, ...) if(0 == close_log_flag) {Log::get_instance()->write_log(1, format, ##__VA_ARGS__); Log::get_instance()->flush();}
#define LOG_WARN(format, ...) if(0 == close_log_flag) {Log::get_instance()->write_log(2, format, ##__VA_ARGS__); Log::get_instance()->flush();}
#define LOG_ERROR(format, ...) if(0 == close_log_flag) {Log::get_instance()->write_log(3, format, ##__VA_ARGS__); Log::get_instance()->flush();}

class Log{
public:
    //C++11懒汉模式，通过静态局部变量实现
    static Log* get_instance(){
        static Log instance;
        return &instance;
    }
    static void *flush_log_thread(void *args){//类内定义的代表线程工作的函数，要用static void *定义
        Log::get_instance()->async_write_log();
    }
    bool init(const char *file_name, int close_log, int log_buf_size_, int max_lines_ = 3000000, int max_queue_size = 0);
    void write_log(int level, const char *format, ...);
    void flush(void);
private:
    Log();
    Log(const Log&) = delete;
    Log& operator = (const Log&) = delete;
    virtual ~Log();
    void *async_write_log(){
        std::string single_log;
        while(log_queue->pop(single_log)){
            mtx.lock();
            fputs(single_log.c_str(), fp);
            mtx.unlock();
        }
    }

    char dir_name[128]; //路径名
    char log_name[128]; //log文件名
    int max_lines;      //日志最大行数
    int log_buf_size;   //日志缓冲区大小
    long line_count;    //日志行数记录
    int today;          //记录当前是那一天
    FILE* fp;           //打开log文件的指针
    char* buf;
    
    block_queue<std::string> *log_queue; //阻塞队列
    bool async_flag;
    int close_log_flag;
    mtx_lock mtx;
};

#endif