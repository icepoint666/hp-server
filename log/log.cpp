#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <stdarg.h>
#include <pthread.h>
#include <iostream>
#include "log.h"

Log::Log(){
    line_count = 0;
    async_flag = false;    
}

Log::~Log(){
    if(fp != NULL)
        fclose(fp);
}

//异步需要设置阻塞队列的长度，同步不需要设置
bool Log::init(const char* file_name, int close_log, int log_buf_size_, int max_lines_, int max_queue_size){
    //如果设置了max_queue_size,则设置为异步
    if(max_queue_size >= 1){
        async_flag = true;
        log_queue = new block_queue<std::string>(max_queue_size);
        pthread_t tid;
        pthread_create(&tid, NULL, flush_log_thread, NULL); //flush_log_thread为回调函数,这里表示创建线程异步写日志
    }
    close_log_flag = close_log;
    log_buf_size = log_buf_size_;
    buf = new char[log_buf_size];
    memset(buf, '\0', log_buf_size);
    max_lines = max_lines_;
    
    time_t t = time(NULL);
    struct tm *sys_tm = localtime(&t);
    struct tm my_tm = *sys_tm;

    const char *p = strrchr(file_name, '/');
    char log_full_name[256] = {0};

    if (p == NULL){
        snprintf(log_full_name, 255, "%d_%02d_%02d_%s", my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday, file_name);
    }else{
        strcpy(log_name, p + 1);
        strncpy(dir_name, file_name, p - file_name + 1);
        snprintf(log_full_name, 255, "%s%d_%02d_%02d_%s", dir_name, my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday, log_name);
    }
    today = my_tm.tm_mday;
    
    fp = fopen(log_full_name, "a");
    if (fp == NULL)
        return false;
    return true; 
}

void Log::write_log(int level, const char *format, ...)
{
    struct timeval now = {0, 0};
    gettimeofday(&now, NULL);
    time_t t = now.tv_sec;
    struct tm *sys_tm = localtime(&t);
    struct tm my_tm = *sys_tm;
    char s[16] = {0};
    switch (level){
    case 0:strcpy(s, "[debug]:");break;
    case 1:strcpy(s, "[info]:");break;
    case 2:strcpy(s, "[warn]:");break;
    case 3:strcpy(s, "[erro]:");break;
    default:strcpy(s, "[info]:");break;
    }
    mtx.lock();
    line_count++;

    if (today != my_tm.tm_mday || line_count % max_lines == 0) {//everyday log
        char new_log[256] = {0};
        fflush(fp);
        fclose(fp);
        char tail[16] = {0};
       
        snprintf(tail, 16, "%d_%02d_%02d_", my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday);
       
        if (today != my_tm.tm_mday){
            snprintf(new_log, 255, "%s%s%s", dir_name, tail, log_name);
            today = my_tm.tm_mday;
            line_count = 0;
        }
        else{
            snprintf(new_log, 255, "%s%s%s.%lld", dir_name, tail, log_name, line_count / max_lines);
        }
        fp = fopen(new_log, "a");
    }
 
    mtx.unlock();

    va_list valst;
    va_start(valst, format);

    std::string log_str;
    mtx.lock();

    //写入的具体时间内容格式
    int n = snprintf(buf, 48, "%d-%02d-%02d %02d:%02d:%02d.%06ld %s ",
                     my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday,
                     my_tm.tm_hour, my_tm.tm_min, my_tm.tm_sec, now.tv_usec, s);
    
    int m = vsnprintf(buf + n, log_buf_size - 1, format, valst);
    buf[n + m] = '\n';
    buf[n + m + 1] = '\0';
    log_str = buf;

    mtx.unlock();

    if (async_flag && !log_queue->full()){
        log_queue->push(log_str);
    }
    else{
        mtx.lock();
        fputs(log_str.c_str(), fp);
        fputs(log_str.c_str(), stdout);
        mtx.unlock();
    }

    va_end(valst);
}

void Log::flush(void){
    mtx.lock();
    fflush(fp);
    fflush(stdout);
    mtx.unlock();
}