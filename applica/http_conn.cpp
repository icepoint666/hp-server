#include "http_conn.h"
#include <mysql/mysql.h>
#include <fstream>
#include <map>

mtx_lock m_lock; //加锁比较暴力，实际中应该使用线程安全的map，或者无锁的map
std::map<std::string, std::string> user_map;

/*
> * 0 注册
> * 1 登录
> * 2 登录检测
> * 3 注册检测
> * 5 请求图片
> * 6 请求视频
*/

const char *ok_200_title = "OK";
const char *error_400_title = "Bad Request";
const char *error_400_form = "Your request has bad syntax or is inherently impossible to staisfy.\n";
const char *error_403_title = "Forbidden";
const char *error_403_form = "You do not have permission to get file form this server.\n";
const char *error_404_title = "Not Found";
const char *error_404_form = "The requested file was not found on this server.\n";
const char *error_500_title = "Internal Error";
const char *error_500_form = "There was an unusual problem serving the request file.\n";


void http_conn::initmysql_result(connection_pool* conn_pool){
    MYSQL *mysql = NULL;
    connection_RAII mysqlcon(&mysql, conn_pool);

    if(mysql_query(mysql, "SELECT username, passwd FROM user")){
        LOG_ERROR("SELECT error:%s\n", mysql_error(mysql));
    }

    //从表中检索完整的结果集
    MYSQL_RES *result = mysql_store_result(mysql);

    //返回结果集中的列数
    int num_fields = mysql_num_fields(result);

    //返回所有字段结构的数组
    MYSQL_FIELD *fields = mysql_fetch_fields(result);

    //从结果集中获取下一行，将对应的用户名和密码，存入map中
    while (MYSQL_ROW row = mysql_fetch_row(result))
    {
        std::string temp1(row[0]);
        std::string temp2(row[1]);
        user_map[temp1] = temp2;
    }
}

http_conn::LINE_STATUS http_conn::parse_line(){
    char temp;
    for (; checked_idx < read_idx; ++checked_idx){
        temp = read_buf[checked_idx];
        if (temp == '\r'){
            if ((checked_idx + 1) == read_idx)
                return LINE_OPEN;
            else if (read_buf[checked_idx + 1] == '\n'){
                read_buf[checked_idx++] = '\0';
                read_buf[checked_idx++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
        else if (temp == '\n'){
            if (checked_idx > 1 && read_buf[checked_idx - 1] == '\r'){
                read_buf[checked_idx - 1] = '\0';
                read_buf[checked_idx++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
    }
    return LINE_OPEN;
}

http_conn::HTTP_CODE http_conn::process_read(){
    LINE_STATUS line_status = LINE_OK;
    HTTP_CODE ret = NO_REQUEST;
    char *text = 0;
    //前面的判断选项满足时就是第一次IO处理，没有读完这个http请求，第二次继续读（传输大文件时会发生）
    while ((check_state == CHECK_STATE_CONTENT && line_status == LINE_OK) || ((line_status = parse_line()) == LINE_OK)){
        text = get_line();
        start_line = checked_idx;
        LOG_INFO("%s", text);
        switch (check_state){
        case CHECK_STATE_REQUESTLINE:{
            ret = parse_request_line(text);
            if (ret == BAD_REQUEST)
                return BAD_REQUEST;
            break;
        }
        case CHECK_STATE_HEADER:{
            ret = parse_headers(text);
            if (ret == BAD_REQUEST)
                return BAD_REQUEST;
            else if (ret == GET_REQUEST){
                return do_request();
            }
            break;
        }
        case CHECK_STATE_CONTENT:{
            ret = parse_content(text);
            if (ret == GET_REQUEST)
                return do_request();
            line_status = LINE_OPEN;
            break;
        }
        default:
            return INTERNAL_ERROR;
        }
    }
    return NO_REQUEST;

}

http_conn::HTTP_CODE http_conn::do_request()
{
    strcpy(filename_, doc_root);
    int len = strlen(doc_root);
    //printf("url:%s\n", url);
    const char *p = strrchr(url, '/');

    //处理cgi
    if (cgi == 1 && (*(p + 1) == '2' || *(p + 1) == '3')){
        //根据标志判断是登录检测还是注册检测
        char flag = url[1];

        char *url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(url_real, "/");
        strcat(url_real, url + 2);
        strncpy(filename_ + len, url_real, FILENAME_LEN - len - 1);
        free(url_real);

        //将用户名和密码提取出来
        //user=123&passwd=123 (post表单提交的数据）
        char name[100], password[100];
        int i;
        for (i = 5; content_[i] != '&'; ++i)
            name[i - 5] = content_[i];
        name[i - 5] = '\0';

        int j = 0;
        for (i = i + 10; content_[i] != '\0'; ++i, ++j)
            password[j] = content_[i];
        password[j] = '\0';

        if (*(p + 1) == '3')
        {
            //如果是注册，先检测数据库中是否有重名的
            //没有重名的，进行增加数据
            char *sql_insert = (char *)malloc(sizeof(char) * 200);
            strcpy(sql_insert, "INSERT INTO user(username, passwd) VALUES(");
            strcat(sql_insert, "'");
            strcat(sql_insert, name);
            strcat(sql_insert, "', '");
            strcat(sql_insert, password);
            strcat(sql_insert, "')");

            if (user_map.find(name) == user_map.end()){
                m_lock.lock();
                int res = mysql_query(mysql, sql_insert);
                user_map.insert(pair<std::string, std::string>(name, password));
                m_lock.unlock();

                if (!res)
                    strcpy(url, "/log.html");
                else
                    strcpy(url, "/registerError.html");
            }
            else
                strcpy(url, "/registerError.html");
        }
        //如果是登录，直接判断
        //若浏览器端输入的用户名和密码在表中可以查找到，返回1，否则返回0
        else if (*(p + 1) == '2')
        {
            if (user_map.find(name) != user_map.end() && user_map[name] == password)
                strcpy(url, "/welcome.html");
            else
                strcpy(url, "/logError.html");
        }
    }

    if (*(p + 1) == '0')
    {
        char *url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(url_real, "/register.html");
        strncpy(filename_ + len, url_real, strlen(url_real));

        free(url_real);
    }
    else if (*(p + 1) == '1')
    {
        char *url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(url_real, "/log.html");
        strncpy(filename_ + len, url_real, strlen(url_real));

        free(url_real);
    }
    else if (*(p + 1) == '5')
    {
        char *url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(url_real, "/picture.html");
        strncpy(filename_ + len, url_real, strlen(url_real));

        free(url_real);
    }
    else if (*(p + 1) == '6')
    {
        char *url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(url_real, "/video.html");
        strncpy(filename_ + len, url_real, strlen(url_real));

        free(url_real);
    }
    else
        strncpy(filename_ + len, url, FILENAME_LEN - len - 1);
    
    //检查是否有权限获取这个GET的文件
    if (stat(filename_, &file_stat) < 0)
        return NO_RESOURCE;

    if (!(file_stat.st_mode & S_IROTH))
        return FORBIDDEN_REQUEST;

    if (S_ISDIR(file_stat.st_mode))
        return BAD_REQUEST;

    int fd = open(filename_, O_RDONLY);

    file_address = (char *)mmap(0, file_stat.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);
    return FILE_REQUEST;
}

http_conn::HTTP_CODE http_conn::parse_request_line(char *text){
    url = strpbrk(text, " \t");
    if (!url)
    {
        return BAD_REQUEST;
    }
    *url++ = '\0';
    char *method = text;
    if (strcasecmp(method, "GET") == 0)
        method = GET;
    else if (strcasecmp(method, "POST") == 0)
    {
        method = POST;
        cgi = 1;
    }
    else
        return BAD_REQUEST;
    url += strspn(url, " \t");
    version = strpbrk(url, " \t");
    if (!version)
        return BAD_REQUEST;
    *version++ = '\0';
    version += strspn(version, " \t");
    if (strcasecmp(version, "HTTP/1.1") != 0)
        return BAD_REQUEST;
    if (strncasecmp(url, "http://", 7) == 0)
    {
        url += 7;
        url = strchr(url, '/');
    }

    if (strncasecmp(url, "https://", 8) == 0)
    {
        url += 8;
        url = strchr(url, '/');
    }

    if (!url || url[0] != '/')
        return BAD_REQUEST;
    //当url为/时，显示判断界面
    if (strlen(url) == 1)
        strcat(url, "judge.html");
    check_state = CHECK_STATE_HEADER;
    return NO_REQUEST;
}

//解析http请求的一个头部信息
http_conn::HTTP_CODE http_conn::parse_headers(char *text)
{
    if (text[0] == '\0')
    {
        if (content_length != 0)
        {
            check_state = CHECK_STATE_CONTENT;
            return NO_REQUEST;
        }
        return GET_REQUEST;
    }
    else if (strncasecmp(text, "Connection:", 11) == 0)
    {
        text += 11;
        text += strspn(text, " \t");
        if (strcasecmp(text, "keep-alive") == 0){
            linger = true;
        }
    }
    else if (strncasecmp(text, "Content-length:", 15) == 0)
    {
        text += 15;
        text += strspn(text, " \t");
        content_length = atol(text);
    }
    else if (strncasecmp(text, "Host:", 5) == 0)
    {
        text += 5;
        text += strspn(text, " \t");
        m_host = text;
    }
    else
    {
        LOG_INFO("oop!unknow header: %s", text);
    }
    return NO_REQUEST;
}

//判断http请求是否被完整读入
http_conn::HTTP_CODE http_conn::parse_content(char *text)
{
    if (read_idx >= (content_length + checked_idx))
    {
        text[content_length] = '\0';
        //POST请求中最后为输入的用户名和密码
        content_ = text;
        return GET_REQUEST;
    }
    return NO_REQUEST;
}

bool http_conn::add_response(const char *format, ...)
{
    if (m_write_idx >= WRITE_BUFFER_SIZE)
        return false;
    va_list arg_list;
    va_start(arg_list, format);
    int len = vsnprintf(m_write_buf + m_write_idx, WRITE_BUFFER_SIZE - 1 - m_write_idx, format, arg_list);
    if (len >= (WRITE_BUFFER_SIZE - 1 - m_write_idx))
    {
        va_end(arg_list);
        return false;
    }
    m_write_idx += len;
    va_end(arg_list);

    LOG_INFO("request:%s", m_write_buf);

    return true;
}

bool http_conn::add_status_line(int status, const char *title){
    return add_response("%s %d %s\r\n", "HTTP/1.1", status, title);
}

bool http_conn::add_headers(int content_len){
    return add_content_length(content_len) && add_linger() && add_blank_line();
}

bool http_conn::add_content_length(int content_len){
    return add_response("Content-Length:%d\r\n", content_len);
}

bool http_conn::add_content_type(){
    return add_response("Content-Type:%s\r\n", "text/html");
}

bool http_conn::add_linger(){
    return add_response("Connection:%s\r\n", (linger == true) ? "keep-alive" : "close");
}

bool http_conn::add_blank_line(){
    return add_response("%s", "\r\n");
}

bool http_conn::add_content(const char *content){
    return add_response("%s", content);
}

bool http_conn::process_write(http_conn::HTTP_CODE ret){
    switch (ret){
    case INTERNAL_ERROR:
    {
        add_status_line(500, error_500_title);
        add_headers(strlen(error_500_form));
        if (!add_content(error_500_form))
            return false;
        break;
    }
    case BAD_REQUEST:
    {
        add_status_line(404, error_404_title);
        add_headers(strlen(error_404_form));
        if (!add_content(error_404_form))
            return false;
        break;
    }
    case FORBIDDEN_REQUEST:
    {
        add_status_line(403, error_403_title);
        add_headers(strlen(error_403_form));
        if (!add_content(error_403_form))
            return false;
        break;
    }
    case FILE_REQUEST:
    {
        add_status_line(200, ok_200_title);
        if (file_stat.st_size != 0)
        {
            add_headers(file_stat.st_size);
            m_iv[0].iov_base = write_buf;
            m_iv[0].iov_len = write_idx;
            m_iv[1].iov_base = file_address;
            m_iv[1].iov_len = file_stat.st_size;
            m_iv_count = 2;
            bytes_to_send = write_idx + file_stat.st_size;
            return true;
        }
        else
        {
            const char *ok_string = "<html><body></body></html>";
            add_headers(strlen(ok_string));
            if (!add_content(ok_string))
                return false;
        }
    }
    default:
        return false;
    }
    m_iv[0].iov_base = write_buf;
    m_iv[0].iov_len = write_idx;
    m_iv_count = 1;
    bytes_to_send = write_idx;
    return true;

}