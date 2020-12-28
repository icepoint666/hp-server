#ifndef SQL_CONNECTION_POOL_H
#define SQL_CONNECTION_POOL_H

#include <stdio.h>
#include <list>
#include <mysql/mysql.h>
#include <error.h>
#include <string.h>
#include <iostream>
#include <string>
#include <../lock.h>
#include <../log/log.h>

class connection_pool{
public:
    MYSQL *get_connection();                //获取数据库连接
    bool release_connection(MYSQL *conn);   //释放连接
    int get_free_conn();                    //获取空闲连接个数
    void destory_pool();                    //销毁所有连接

    static connection_pool *get_instance(); //单例模式
    void init(std::string url_, std::string user_, std::string password_, std::string db_name_, int port_, int max_conn_);
    
    std::string url;                        //主机地址
    std::string port;                       //数据库端口号
    std::string user;                       //登录数据库用户名
    std::string password;                   //登录数据库密码
    std::string db_name;                    //使用数据库名

private:
    connection_pool();                      //单例模式特性
    ~connection_pool();                     //单例模式特性

    int max_conn;
    int cur_conn;
    int free_conn;
    mtx_lock lk;
    std::list<MYSQL*> conn_list;                 //连接池
    sem reserve;
}

class connection_RAII{
public:
    connection_RAII(MYSQL **SQL, connection_pool *conn_pool);
    ~connection_RAII();
private:
    MYSQL *connRAII;
    connection_pool *poolRAII;
}

#endif