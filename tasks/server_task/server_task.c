#include "cache_server_task/cache_server_task.c"
#include "end_server_task/end_server_task.c"
#include "connect_task/connect_task.c"
#include "http_parser/parser.h"
#include "proxy.h"
#include "logger/log.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <inttypes.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <netdb.h>

int init_server_task(server_task* task, char* query){
    task->progress = 0;
    task->aborted = false;
    task->finished = false;
    task->query = query;
    task->server_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if(task->server_socket == -1){
        log_trace("THREAD %d: Not enough memory for allocating server task socket", curthread_id());
        return PR_NOT_ENOUGH_MEMORY;
    }
    fcntl(task->server_socket, F_SETFL, fcntl(task->server_socket, F_GETFL) | O_NONBLOCK);
    int true_val = 1;
    if(setsockopt(task->server_socket, SOL_SOCKET, SO_REUSEADDR, &true_val, sizeof(int))){
        close(task->server_socket);
        log_trace("THREAD %d: Could not set socket option", curthread_id());
        return PR_NOT_ENOUGH_MEMORY;
    }
    struct sockaddr_in addr;
    addr.sin_port = htons(0);
    addr.sin_family = AF_INET;
    memset(addr.sin_zero, 0, 8);
    int aton_val = inet_aton("10.4.0.68", &addr.sin_addr);
    assert(aton_val);
    if(bind(task->server_socket, (struct sockaddr*)&addr, sizeof(struct sockaddr_in))){
        close(task->server_socket);
        log_trace("THREAD %d: Could not bind server task socket", curthread_id());
        return PR_COULD_NOT_CONNECT;
    }

    char* host_val;
    int host_length;
    int find_header_val = find_header_by_name(query, "Host\0", &host_val, &host_length);
    if(find_header_val == PR_NO_SUCH_HEADER){
        close(task->server_socket);
        log_trace("THREAD %d: Could not find host header", curthread_id());
        return PR_NO_SUCH_HEADER;
    }
    char savsymb = host_val[host_length];
    host_val[host_length] = '\0';

    struct hostent hostent;
    char* gethostbuf = malloc(sizeof(char) * PR_GETHOST_BUFSIZ);
    if(!gethostbuf){
        close(task->server_socket);
        log_trace("THREAD %d: Could not allocate buffer for host resolving", curthread_id());
        host_val[host_length] = savsymb;
        return PR_NOT_ENOUGH_MEMORY;
    }
    int gethosterr;
    if(!gethostbyname_r(host_val, &hostent, gethostbuf, PR_GETHOST_BUFSIZ, &gethosterr) || !hostent.h_length){
        close(task->server_socket);
        free(gethostbuf);
        log_trace("THREAD %d: Could not resolve host with error %d:\n%s", curthread_id(), gethosterr, host_val);
        host_val[host_length] = savsymb;
        return PR_NO_SUCH_HOST;
    }
    host_val[host_length] = savsymb;

    if(hostent.h_addrtype != AF_INET){
        close(task->server_socket);
        free(gethostbuf);
        log_trace("THREAD %d: Unsupported host address type %d for:\n%s", curthread_id(), hostent.h_addrtype, host_val);
        return PR_HOST_UNSUPPORTED_AF;
    }

    struct sockaddr_in host_addr;
    host_addr.sin_port = htons(80);
    host_addr.sin_family = AF_INET;
    memset(host_addr.sin_zero, 0, 8);
    memcpy(&host_addr.sin_addr.s_addr, hostent.h_addr_list[0], sizeof(host_addr.sin_addr.s_addr));
    log_trace("THREAD %d: Parsed address %s", curthread_id(), inet_ntoa(host_addr.sin_addr));
    int connect_val = connect(task->server_socket, (struct sockaddr*)&host_addr, sizeof(struct sockaddr_in));
    if(connect_val && errno != EINPROGRESS){
        close(task->server_socket);
        free(gethostbuf);
        log_trace("THREAD %d: Could not connect to host:\n%s", curthread_id(), host_val);
        return PR_COULD_NOT_CONNECT;
    }

    task->end_buf = malloc(sizeof(char) * PR_END_SERVER_BUFSIZ);
    if(!task->end_buf){
        close(task->server_socket);
        free(gethostbuf);
        log_trace("THREAD %d: Not enough memory for allocating end server buffer", curthread_id());
        return PR_NOT_ENOUGH_MEMORY;
    }
    task->clients = malloc(sizeof(struct client_task*) * PR_SERVER_CLIENTS_INIT_CAP);
    if(!task->clients){
        close(task->server_socket);
        free(gethostbuf);
        free(task->end_buf);
        log_trace("THREAD %d: Not enough memory for allocating server clients array", curthread_id());
        return PR_NOT_ENOUGH_MEMORY;
    }

    return PR_SUCCESS;
}

int free_server_task(server_task* task){
    close(task->server_socket);
    free(task->end_buf);
    free(task->clients);
}
