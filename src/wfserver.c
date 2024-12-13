#include <stdio.h>  
#include <stdlib.h>  
#include <string.h>  
#include <unistd.h>  
#include <sys/socket.h>  
#include <netinet/in.h>  
#include <arpa/inet.h>  // 用于 inet_ntoa  
#include <sys/stat.h>  
#include <fcntl.h>  
#include <errno.h>  
#include <time.h>  
#include "log.h"  

#define BUFFER_SIZE 1024  
#define DEFAULT_PORT 8081  
#define DEFAULT_ROOT "../html"  

// 获取文件大小  
static off_t get_file_size(const char *filepath) {  
    struct stat st;  
    if (stat(filepath, &st) == 0) {  
        return st.st_size;  
    }  
    return -1;  
}  

// 获取文件MIME类型  
const char* get_mime_type(const char *filepath) {  
    const char *ext = strrchr(filepath, '.');  
    if (ext == NULL) {  
        return "application/octet-stream";  
    }  
    ext++;  // 跳过点号  
    
    if (strcasecmp(ext, "html") == 0 || strcasecmp(ext, "htm") == 0) return "text/html";  
    if (strcasecmp(ext, "css") == 0) return "text/css";  
    if (strcasecmp(ext, "js") == 0) return "application/javascript";  
    if (strcasecmp(ext, "jpg") == 0 || strcasecmp(ext, "jpeg") == 0) return "image/jpeg";  
    if (strcasecmp(ext, "png") == 0) return "image/png";  
    if (strcasecmp(ext, "gif") == 0) return "image/gif";  
    if (strcasecmp(ext, "ico") == 0) return "image/x-icon";  
    
    return "application/octet-stream";  
}  

void handle_client(int client_socket, struct sockaddr_in *client_addr) {  
    char buffer[BUFFER_SIZE];  
    char response[BUFFER_SIZE];  
    char filepath[BUFFER_SIZE];  
    time_t start_time = time(NULL);  
    
    // 获取客户端IP地址  
    char *client_ip = inet_ntoa(client_addr->sin_addr);  
    int client_port = ntohs(client_addr->sin_port);  
    
    write_log("New connection from %s:%d", client_ip, client_port);  
    
    // 读取HTTP请求  
    ssize_t bytes_read = read(client_socket, buffer, sizeof(buffer) - 1);  
    if (bytes_read <= 0) {  
        write_log("Error reading from client %s:%d - %s",   
                 client_ip, client_port, strerror(errno));  
        close(client_socket);  
        return;  
    }  
    buffer[bytes_read] = '\0';  
    
    // 解析HTTP请求方法  
    char method[16] = {0};  
    sscanf(buffer, "%15s", method);  
    
    // 解析HTTP请求，获取请求的文件路径  
    char *path = strstr(buffer, "GET ");  
    if (path == NULL) {  
        write_log("Invalid HTTP request from %s:%d - Method: %s",   
                 client_ip, client_port, method);  
        close(client_socket);  
        return;  
    }  
    
    path += 4;  
    char *end_path = strchr(path, ' ');  
    if (end_path == NULL) {  
        write_log("Malformed HTTP request from %s:%d", client_ip, client_port);  
        close(client_socket);  
        return;  
    }  
    *end_path = '\0';  

    // 解析HTTP版本  
    char http_version[16] = {0};  
    sscanf(end_path + 1, "HTTP/%s", http_version);  
    
    write_log("Request from %s:%d - Method: %s, Path: %s, HTTP Version: %s",   
             client_ip, client_port, method, path, http_version);  

    // 如果请求根路径，则返回index.html  
    if (strcmp(path, "/") == 0) {  
        path = "/index.html";  
    }  

    // 构建完整的文件路径  
    snprintf(filepath, sizeof(filepath), "%s%s", DEFAULT_ROOT, path);  
    
    // 获取文件大小  
    off_t file_size = get_file_size(filepath);  
    const char* mime_type = get_mime_type(filepath);  

    // 打开请求的文件  
    int fd = open(filepath, O_RDONLY);  
    if (fd == -1) {  
        write_log("404 Not Found: %s (requested by %s:%d)",   
                 filepath, client_ip, client_port);  
        
        const char *not_found = "HTTP/1.1 404 Not Found\r\n"  
                               "Content-Type: text/html\r\n"  
                               "\r\n"  
                               "<html><body><h1>404 Not Found</h1></body></html>";  
        write(client_socket, not_found, strlen(not_found));  
    } else {  
        write_log("200 OK: %s (size: %ld bytes, type: %s) for %s:%d",   
                 filepath, file_size, mime_type, client_ip, client_port);  
        
        // 构建HTTP响应头  
        char header[BUFFER_SIZE];  
        snprintf(header, sizeof(header),  
                "HTTP/1.1 200 OK\r\n"  
                "Content-Type: %s\r\n"  
                "Content-Length: %ld\r\n"  
                "\r\n",  
                mime_type, file_size);  
        write(client_socket, header, strlen(header));  

        // 发送文件内容  
        ssize_t total_sent = 0;  
        ssize_t bytes;  
        while ((bytes = read(fd, buffer, sizeof(buffer))) > 0) {  
            ssize_t sent = write(client_socket, buffer, bytes);  
            if (sent > 0) {  
                total_sent += sent;  
            }  
        }  
        
        time_t end_time = time(NULL);  
        write_log("Response completed for %s:%d - Sent %ld bytes in %ld seconds",   
                 client_ip, client_port, total_sent, (end_time - start_time));  
        
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
    write_log("=== Server starting up ===");  

    if (argc > 1) {  
        port = atoi(argv[1]);  
        write_log("Custom port specified: %d", port);  
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

    write_log("Server successfully started on port %d", port);  
    write_log("Document root: %s", DEFAULT_ROOT);  
    write_log("Maximum connections in queue: 10");  
    printf("Server started on port %d...\n", port);  

    while (1) {  
        client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_len);  
        if (client_socket == -1) {  
            write_log("Accept failed: %s", strerror(errno));  
            continue;  
        }  

        handle_client(client_socket, &client_addr);  
    }  

    write_log("=== Server shutting down ===");  
    close_log();  
    close(server_socket);  
    return 0;  
}