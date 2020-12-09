#include <unistd.h> /* daemon */

#include "web_server.h"

#define DAEMON 0
int main(){
    #if DAEMON
    daemon(1, 0); 
    #endif

    HttpServer server;
    server.port = 8080;
    server.timewait = -1;
    http_server_run(server);
    
    return 0;
}

