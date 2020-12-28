#ifndef HTTP_CONN_H
#define HTTP_CONN_H

#include <unistd.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include "sql_connection_pool.h"
#include "../config.h"
#include "../lock.h"

class http_conn{
public:
    http_conn(){};
    ~http_conn(){};

    enum METHOD
    {
        GET = 0,
        POST,
        HEAD,
        PUT,
        DELETE,
        TRACE,
        OPTIONS,
        CONNECT,
        PATH
    };
    enum CHECK_STATE
    {
        CHECK_STATE_REQUESTLINE = 0,
        CHECK_STATE_HEADER,
        CHECK_STATE_CONTENT
    };
    enum HTTP_CODE
    {
        NO_REQUEST,//没有完整的请求，一次没有读完一个http request
        GET_REQUEST,//得到了完整的请求
        BAD_REQUEST,//http request语法不对，或者不支持
        NO_RESOURCE,
        FORBIDDEN_REQUEST,
        FILE_REQUEST,
        INTERNAL_ERROR,
        CLOSED_CONNECTION
    };
    enum LINE_STATUS
    {
        LINE_OK = 0,
        LINE_BAD,
        LINE_OPEN
    };
    CHECK_STATE check_state;
    METHOD method;
    bool linger;
    char* url;
    char* version;
    int content_length;
    int cgi; //是否启用cgi
    char* content_; //存储请求头,或者请求内容
    char* file_address;
    char* doc_root;

    int bytes_to_send;
    int bytes_have_send;
    //file mapping
    char *file_address;
    struct stat file_stat;
    struct iovec m_iv[2]; //writev
    int m_iv_count; 

    int timer_flag;
    int improv;
    int state;
    uint32_t conn_event;
    int read_idx;
    int check_idx;
    int start_line;
    int write_idx;

    int sockfd;
    sockaddr_in address;
    char read_buf[READ_BUFFER_SIZE];
    char write_buf[WRITE_BUFFER_SIZE];
    char filename_[FILENAME_LEN];

    char sql_user[100];
    char sql_passwd[100];
    char sql_name[100];

    void initmysql_result(connection_pool *conn_pool);

    HTTP_CODE process_read();
    bool process_write(HTTP_CODE ret);

    HTTP_CODE parse_request_line(char *text);
    HTTP_CODE parse_headers(char *text);
    HTTP_CODE parse_content(char *text);
    HTTP_CODE do_request();
    char *get_line() { return read_buf + start_line; };
    LINE_STATUS parse_line();
    bool add_response(const char *format, ...);
    bool add_content(const char *content);
    bool add_status_line(int status, const char *title);
    bool add_headers(int content_length);
    bool add_content_type();
    bool add_content_length(int content_length);
    bool add_linger();
    bool add_blank_line();
};

#endif