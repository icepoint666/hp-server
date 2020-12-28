#include <unistd.h> /* daemon */
#include <iostream>
#include "web_server.h"
#include "config.h"

int main(){
    #if DAEMON
    daemon(1, 0); 
    #endif

    HttpServer server;
    server.port = 8080;
    server.timewait = -1;
    server.user = "root";
    server.passwd = "root";
    server.dbname = "default_db";
    
    int ret = http_server_run(server);
    std::cout << "Exit Status: " << ret << std::endl;
    return ret;
}

