#include <stdio.h>  
#include <stdlib.h>  
#include <string.h>  
#include <unistd.h>  
#include <sys/socket.h>  
#include <netinet/in.h>  
#include <sys/stat.h>  
#include <fcntl.h>  

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
        close(client_socket);  
        return;  
    }  
    buffer[bytes_read] = '\0';  

    // 解析HTTP请求，获取请求的文件路径  
    char *path = strstr(buffer, "GET ");  
    if (path == NULL) {  
        close(client_socket);  
        return;  
    }  
    
    path += 4; // 跳过"GET "  
    char *end_path = strchr(path, ' ');  
    if (end_path == NULL) {  
        close(client_socket);  
        return;  
    }  
    *end_path = '\0';  

    // 如果请求根路径，则返回index.html  
    if (strcmp(path, "/") == 0) {  
        path = "/index.html";  
    }  

    // 构建完整的文件路径  
    snprintf(filepath, sizeof(filepath), "%s%s", DEFAULT_ROOT, path);  

    // 打开请求的文件  
    int fd = open(filepath, O_RDONLY);  
    if (fd == -1) {  
        // 文件不存在，返回404  
        const char *not_found = "HTTP/1.1 404 Not Found\r\n"  
                               "Content-Type: text/html\r\n"  
                               "\r\n"  
                               "<html><body><h1>404 Not Found</h1></body></html>";  
        write(client_socket, not_found, strlen(not_found));  
    } else {  
        // 文件存在，返回200和文件内容  
        const char *header = "HTTP/1.1 200 OK\r\n"  
                           "Content-Type: text/html\r\n"  
                           "\r\n";  
        write(client_socket, header, strlen(header));  

        // 读取并发送文件内容  
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

    // 如果提供了端口参数，使用提供的端口  
    if (argc > 1) {  
        port = atoi(argv[1]);  
    }  

    // 创建服务器socket  
    server_socket = socket(AF_INET, SOCK_STREAM, 0);  
    if (server_socket == -1) {  
        perror("Socket creation failed");  
        exit(EXIT_FAILURE);  
    }  

    // 设置socket选项  
    int opt = 1;  
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {  
        perror("Setsockopt failed");  
        exit(EXIT_FAILURE);  
    }  

    // 配置服务器地址  
    server_addr.sin_family = AF_INET;  
    server_addr.sin_addr.s_addr = INADDR_ANY;  
    server_addr.sin_port = htons(port);  

    // 绑定socket  
    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {  
        perror("Bind failed");  
        exit(EXIT_FAILURE);  
    }  

    // 监听连接  
    if (listen(server_socket, 10) == -1) {  
        perror("Listen failed");  
        exit(EXIT_FAILURE);  
    }  

    printf("Server started on port %d...\n", port);  

    // 主循环，接受并处理连接  
    while (1) {  
        client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_len);  
        if (client_socket == -1) {  
            perror("Accept failed");  
            continue;  
        }  

        // 处理客户端请求  
        handle_client(client_socket);  
    }  

    close(server_socket);  
    return 0;  
}
