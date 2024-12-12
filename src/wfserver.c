#include <stdio.h>  
#include <stdlib.h>  
#include <string.h>  
#include <unistd.h>  
#include <sys/socket.h>  
#include <netinet/in.h>  
#include <sys/stat.h>  
#include <fcntl.h>  
#include <errno.h>  
#include "log.h"  

#define BUFFER_SIZE 1024  
#define DEFAULT_PORT 8081  
#define DEFAULT_ROOT "../html"  

void handle_client(int client_socket) {  
    char buffer[BUFFER_SIZE];  
    char response[BUFFER_SIZE];  
    char filepath[BUFFER_SIZE];  
    
    // 读取HTTP请求  
    ssize_t bytes_read = read(client_socket, buffer, sizeof(buffer) - 1);  
    if (bytes_read <= 0) {  
        write_log("Error reading from client socket");  
        close(client_socket);  
        return;  
    }  
    buffer[bytes_read] = '\0';  

    // 解析HTTP请求，获取请求的文件路径  
    char *path = strstr(buffer, "GET ");  
    if (path == NULL) {  
        write_log("Invalid HTTP request received");  
        close(client_socket);  
        return;  
    }  
    
    path += 4;  
    char *end_path = strchr(path, ' ');  
    if (end_path == NULL) {  
        write_log("Malformed HTTP request");  
        close(client_socket);  
        return;  
    }  
    *end_path = '\0';  

    write_log("Received request for path: %s", path);  

    // 如果请求根路径，则返回index.html  
    if (strcmp(path, "/") == 0) {  
        path = "/index.html";  
    }  

    // 构建完整的文件路径  
    snprintf(filepath, sizeof(filepath), "%s%s", DEFAULT_ROOT, path);  

    // 打开请求的文件  
    int fd = open(filepath, O_RDONLY);  
    if (fd == -1) {  
        write_log("404 Not Found: %s", filepath);  
        const char *not_found = "HTTP/1.1 404 Not Found\r\n"  
                               "Content-Type: text/html\r\n"  
                               "\r\n"  
                               "<html><body><h1>404 Not Found</h1></body></html>";  
        write(client_socket, not_found, strlen(not_found));  
    } else {  
        write_log("200 OK: %s", filepath);  
        const char *header = "HTTP/1.1 200 OK\r\n"  
                           "Content-Type: text/html\r\n"  
                           "\r\n";  
        write(client_socket, header, strlen(header));  

        ssize_t bytes;  
        while ((bytes = read(fd, buffer, sizeof(buffer))) > 0) {  
            write(client_socket, buffer, bytes);  
        }  
        close(fd);  
    }  

    close(client_socket);  
}  

int main(int argc, char *argv[]) {  
    int server_socket, client_socket;  
    struct sockaddr_in server_addr, client_addr;  
    socklen_t client_len = sizeof(client_addr);  
    int port = DEFAULT_PORT;  

    // 初始化日志系统  
    init_logging();  
    write_log("Server starting up...");  

    if (argc > 1) {  
        port = atoi(argv[1]);  
    }  

    server_socket = socket(AF_INET, SOCK_STREAM, 0);  
    if (server_socket == -1) {  
        write_log("Socket creation failed: %s", strerror(errno));  
        exit(EXIT_FAILURE);  
    }  

    int opt = 1;  
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {  
        write_log("Setsockopt failed: %s", strerror(errno));  
        exit(EXIT_FAILURE);  
    }  

    server_addr.sin_family = AF_INET;  
    server_addr.sin_addr.s_addr = INADDR_ANY;  
    server_addr.sin_port = htons(port);  

    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {  
        write_log("Bind failed: %s", strerror(errno));  
        exit(EXIT_FAILURE);  
    }  

    if (listen(server_socket, 10) == -1) {  
        write_log("Listen failed: %s", strerror(errno));  
        exit(EXIT_FAILURE);  
    }  

    write_log("Server started on port %d", port);  
    printf("Server started on port %d...\n", port);  

    while (1) {  
        client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_len);  
        if (client_socket == -1) {  
            write_log("Accept failed: %s", strerror(errno));  
            continue;  
        }  

        write_log("New client connection accepted");  
        handle_client(client_socket);  
    }  

    close_log();  
    close(server_socket);  
    return 0;  
}