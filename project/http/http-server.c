#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include <time.h>

#define MYHOST "localhost"
#define MAX_CLIENT_BACKLOG 128
#define MAX_BUFFER_SIZE 4096
#define IS_MULTIPROCESS 1

void handleConnection();
void handleRequest();
void transmitFile();
void transmitError();
char* getHTTPFileType();

int main(int argc, char* argv[]) {
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

    char* DOCROOT = argv[1];
    char* PORT = argv[2];

    return_value = getaddrinfo(MYHOST, PORT, &hints, &address_resource);
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
    while(1) {
        accept_desc = accept(socket_desc, (struct sockaddr *) &remote_addr, &remote_addr_s);
        #if IS_MULTIPROCESS == 1
        int pid = fork();

        if(pid == -1){
                printf("Error: %s (line %d)\n", strerror(errno), __LINE__);
                return pid;
            }else if(pid == 0){
                //child process starts...
                handleConnection(accept_desc, DOCROOT);
                close(accept_desc);
                close(socket_desc);
                exit(0);
                //...child process terminated
            }

            close(accept_desc);

        #else
            handleConnection(accept_desc, DOCROOT);
            close(accept_desc);
        #endif
    }

    close(socket_desc);

    return 0;
}

void handleConnection(int accept_desc, char* root){
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
            if(strlen(request_buffer) == 0)
                return;
            handleRequest(request_buffer, root, accept_desc);
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

void handleRequest(char* request_buffer, char* root, int accept_desc) {
    char command[5];
    char file[MAX_BUFFER_SIZE];
    char version[9];
    memset(&command, 0, 5);
    memset(&file, 0, MAX_BUFFER_SIZE);
    memset(&version, 0, 9);

    sscanf(request_buffer, "%s %s %s", command, file, version);

    if ((strcmp(command, "GET") != 0) && (strcmp(command, "HEAD") != 0)) {
        //printf("Error: Unsupported HTTP Request Command %s (line %d)\n", command, __LINE__);
        return;
    }

    if (strcmp(version, "HTTP/1.1") != 0) {
        printf("Error: Unknown HTTP Version (line %d)\n", __LINE__);
        return;
    }

    FILE *fp;
    char* route = malloc(4096);
    strcpy(route, root);
    strcat(route, file);
    fp = fopen(route, "r");
    if (fp == NULL){
        transmitError(fp, accept_desc);
        printf("Error: %s (line %d)\n", strerror(errno), __LINE__);
        return;
    }

    transmitFile(fp, accept_desc, file);

}

void transmitFile(FILE *fp, int accept_desc, const char* fileName){
    int bytes_sent;
    char response_buffer[MAX_BUFFER_SIZE];
    memset(response_buffer, 0, MAX_BUFFER_SIZE);

    struct stat fs;
    int fd = fileno(fp);
    fstat(fd, &fs);

    unsigned int fileSize = fs.st_size;
    char* sfileSize= malloc(100);
    sprintf(sfileSize, "%d", fileSize);

    char timeBuff[1000];
    time_t currentTime = time(0);
    struct tm tm = *gmtime(&currentTime);
    strftime(timeBuff, sizeof(timeBuff), "%a, %d %b %Y %H:%M:%S %Z", &tm);

    char* fileType = getHTTPFileType(fileName);
    if( fileType == 0){
        printf("Error: Unsupported File Type (line %d)\n", __LINE__);
        return;
    }

    strcpy(response_buffer,"HTTP/1.1 200 OK\n");
    strcat(response_buffer, "Date: ");
    strcat(response_buffer, timeBuff);
    strcat(response_buffer, "\n");
    strcat(response_buffer, "Content-Type: ");
    strcat(response_buffer, fileType);
    strcat(response_buffer, "\n");
    strcat(response_buffer, "Content-Length: ");
    strcat(response_buffer, sfileSize);
    strcat(response_buffer, "\n");
    strcat(response_buffer, "\n");

    bytes_sent = send(accept_desc, response_buffer, strlen(response_buffer), 0);
    if(bytes_sent == -1)
        printf("Error: %s (line %d)\n", strerror(errno), __LINE__);

    char fileBuffer[1];
    while( (fread(fileBuffer, sizeof(char), 1, fp)) > 0){
        bytes_sent = send(accept_desc, fileBuffer, sizeof(fileBuffer), 0);
        if(bytes_sent == -1)
            printf("Error: %s (line %d)\n", strerror(errno), __LINE__);
    }

}

void transmitError(FILE *fp, int accept_desc){
    int bytes_sent;
    char response_buffer[MAX_BUFFER_SIZE];
    memset(response_buffer, 0, MAX_BUFFER_SIZE);

    char timeBuff[1000];
    time_t currentTime = time(0);
    struct tm tm = *gmtime(&currentTime);
    strftime(timeBuff, sizeof(timeBuff), "%a, %d %b %Y %H:%M:%S %Z", &tm);

    char errorText[] = "Error 404: Page Not Found";
    char serrorText[MAX_BUFFER_SIZE];
    sprintf(serrorText, "%d", (int) strlen(errorText));

    strcpy(response_buffer,"HTTP/1.1 404 Not Found\n");
    strcat(response_buffer, "Date: ");
    strcat(response_buffer, timeBuff);
    strcat(response_buffer, "\n");
    strcat(response_buffer, "Content-Type: text/plain");
    strcat(response_buffer, "\n");
    strcat(response_buffer, "Content-Length: ");
    strcat(response_buffer, serrorText);
    strcat(response_buffer, "\n");
    strcat(response_buffer, "\n");
    strcat(response_buffer, errorText);

    bytes_sent = send(accept_desc, response_buffer, strlen(response_buffer), 0);
    if(bytes_sent == -1)
        printf("Error: %s (line %d)\n", strerror(errno), __LINE__);

}

char* getHTTPFileType(const char fileName[MAX_BUFFER_SIZE]){
    char c = ' ';
    unsigned int i = 0;
    while(c != '.'){
        c = fileName[i];
        i++;
    }
    char fileExt[MAX_BUFFER_SIZE];
    memset(fileExt, 0, MAX_BUFFER_SIZE);

    int x= 0;
    for(unsigned int a = i; a < strlen(fileName); a++){
        fileExt[x] = fileName[a];
        x++;
    }

    if(strcmp(fileExt, "jpeg") == 0){
        return "image/jpeg";
    }else if(strcmp(fileExt, "png") == 0){
        return "image/png";
    }else if(strcmp(fileExt, "gif") == 0){
        return "image/gif";
    }else if(strcmp(fileExt, "pdf") == 0){
        return "application/pdf";
    }else if(strcmp(fileExt, "js") == 0){
        return "application/javascript";
    }else if(strcmp(fileExt, "html") == 0){
        return "text/html";
    }else if(strcmp(fileExt, "txt") == 0){
        return "text/plain";
    }else if(strcmp(fileExt, "css") == 0){
        return "text/css";
    }

    return 0;

}
