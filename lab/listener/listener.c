#include <stdio.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <ctype.h>
#include <stdlib.h>

#define MYHOST "localhost"
#define MYPORT "8080"
#define MAX_CLIENT_BACKLOG 128
#define MAX_BUFFER_SIZE 4096
#define IS_MULTIPROCESS 1

void handleConnection();
void send_response();

int main() {
    int socket_desc;
    int accept_desc;
    int return_value;
    int enable = 1;
    struct addrinfo *address_resource;
    struct addrinfo hints;
    struct sockaddr_storage remote_addr;
    socklen_t remote_addr_s = sizeof(remote_addr);

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    return_value = getaddrinfo(MYHOST, MYPORT, &hints, &address_resource);
    if (return_value != 0){
        printf("Error: %s (line %d)\n", strerror(errno), __LINE__);
        return return_value;
    }

    socket_desc = socket(
            address_resource->ai_family,
            address_resource->ai_socktype,
            address_resource->ai_protocol
            );
    if (socket_desc == -1){
        printf("Error: %s (line %d)\n", strerror(errno), __LINE__);
        return socket_desc;
    }

    return_value = setsockopt(socket_desc, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int));
    if (return_value == -1 ){
        printf("Error: %s (line %d)\n", strerror(errno), __LINE__);
        return return_value;
    }

    return_value = bind(socket_desc, address_resource->ai_addr, address_resource->ai_addrlen);
    if (return_value == -1 ){
        printf("Error: %s (line %d)\n", strerror(errno), __LINE__);
        return return_value;
    }

    freeaddrinfo(address_resource);

    return_value = listen(socket_desc, MAX_CLIENT_BACKLOG);
    if (return_value == -1 ){
        printf("Error: %s (line %d)\n", strerror(errno), __LINE__);
        return return_value;
    }

    while(1){
        accept_desc = accept(socket_desc, (struct sockaddr *) &remote_addr, &remote_addr_s);
        #if IS_MULTIPROCESS == 1
            int pid = fork();

            if(pid == -1){
                printf("Error: %s (line %d)\n", strerror(errno), __LINE__);
                return pid;
            }else if(pid == 0){
                //child process starts...
                handleConnection(accept_desc);
                close(accept_desc);
                close(socket_desc);
                exit(0);
                //...child process terminated
            }

            close(accept_desc);

        #else
            handleConnection(accept_desc);
            close(accept_desc);
        #endif
    }

    close(socket_desc);

    return 0;
}

void handleConnection(int accept_desc){
    char c;
    int bytes_read;
    int cursor = 0;
    char request_buffer[MAX_BUFFER_SIZE];

    memset(&request_buffer, 0, MAX_BUFFER_SIZE);

    while(1){
        bytes_read = recv(accept_desc, &c, 1, 0);

        if(bytes_read <= 0){
            if (bytes_read == -1 )
                printf("Error: %s (line %d)\n", strerror(errno), __LINE__);
            break;
        }

        if(c == '\n') {
            send_response(accept_desc, request_buffer);
            if(strlen(request_buffer) == 0)
                return;

            memset(&request_buffer, 0, MAX_BUFFER_SIZE);
            cursor = 0;

        }else {
            if (cursor < MAX_BUFFER_SIZE) {
                request_buffer[cursor] = c;
                cursor++;
            }
        }
    }
}

void send_response(int accept_desc, char * request) {
    unsigned long i;
    int bytes_sent;
    char response_buffer[MAX_BUFFER_SIZE];

    memset(response_buffer, 0, MAX_BUFFER_SIZE);

    for (i = 0; i < strlen(request); i++) {
        response_buffer[i] = (char) toupper(request[i]);
    }

    response_buffer[i] = '\n';

    bytes_sent = send(accept_desc, response_buffer, strlen(response_buffer), 0);
    if (bytes_sent == -1)
        printf("Error: %s (line %d)\n", strerror(errno), __LINE__);
}
