#include <stdio.h>  
#include <stdlib.h>  
#include <string.h>  
#include <unistd.h>  
#include <sys/socket.h>  
#include <netinet/in.h>  
#include <signal.h>  
#include <errno.h>  
#include <sys/sysinfo.h>  // 用于获取系统信息  
#include "threadpool.h"  
#include "log.h"  
#include "http_handler.h"  

#define DEFAULT_PORT 8081  
#define THREAD_MULTIPLIER 1.5  // 线程数 = CPU核心数 * THREAD_MULTIPLIER  
#define MIN_THREADS 4         // 最小线程数  
#define MAX_THREADS 32        // 最大线程数  
#define QUEUE_MULTIPLIER 8    // 队列大小 = 线程数 * QUEUE_MULTIPLIER  
#define MIN_QUEUE_SIZE 32     // 最小队列大小  
#define MAX_QUEUE_SIZE 256    // 最大队列大小  

static threadpool_t *pool = NULL;  
static int server_socket = -1;  
static volatile sig_atomic_t server_running = 1;  

// 获取最优线程数和队列大小  
static void get_optimal_thread_config(int *thread_count, int *queue_size) {  
    // 获取CPU核心数  
    int num_cores = sysconf(_SC_NPROCESSORS_ONLN);  
    if (num_cores <= 0) {  
        num_cores = 2;  // 默认值  
        write_log("Warning: Could not determine CPU count, using default value: 2");  
    }  

    // 计算线程数  
    *thread_count = (int)(num_cores * THREAD_MULTIPLIER);  
    
    // 确保线程数在合理范围内  
    if (*thread_count < MIN_THREADS) {  
        *thread_count = MIN_THREADS;  
    } else if (*thread_count > MAX_THREADS) {  
        *thread_count = MAX_THREADS;  
    }  

    // 计算队列大小  
    *queue_size = *thread_count * QUEUE_MULTIPLIER;  
    
    // 确保队列大小在合理范围内  
    if (*queue_size < MIN_QUEUE_SIZE) {  
        *queue_size = MIN_QUEUE_SIZE;  
    } else if (*queue_size > MAX_QUEUE_SIZE) {  
        *queue_size = MAX_QUEUE_SIZE;  
    }  
}  

void signal_handler(int signo) {  
    if (signo == SIGINT || signo == SIGTERM) {  
        write_log("Received shutdown signal, cleaning up...");  
        server_running = 0;  
        
        // 创建一个连接来解除accept的阻塞  
        struct sockaddr_in addr;  
        int sock = socket(AF_INET, SOCK_STREAM, 0);  
        if (sock >= 0) {  
            addr.sin_family = AF_INET;  
            addr.sin_port = htons(DEFAULT_PORT);  
            addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);  
            connect(sock, (struct sockaddr*)&addr, sizeof(addr));  
            close(sock);  
        }  
    }  
}  

// 打印系统信息  
static void print_system_info(int thread_count, int queue_size) {  
    struct sysinfo si;  
    if (sysinfo(&si) == 0) {  
        write_log("System Information:");  
        write_log("- Total RAM: %lu MB", si.totalram * si.mem_unit / (1024 * 1024));  
        write_log("- Free RAM: %lu MB", si.freeram * si.mem_unit / (1024 * 1024));  
        write_log("- Process Count: %d", si.procs);  
        write_log("- Load Averages: %.2f, %.2f, %.2f",  
                 si.loads[0] / 65536.0, si.loads[1] / 65536.0, si.loads[2] / 65536.0);  
    }  

    write_log("Server Configuration:");  
    write_log("- CPU Cores: %d", sysconf(_SC_NPROCESSORS_ONLN));  
    write_log("- Worker Threads: %d", thread_count);  
    write_log("- Queue Size: %d", queue_size);  
}  

int main(int argc, char *argv[]) {  
    struct sockaddr_in server_addr, client_addr;  
    socklen_t client_len = sizeof(client_addr);  
    int port = DEFAULT_PORT;  
    int thread_count, queue_size;  

    // 初始化日志系统  
    init_logging();  
    write_log("=== Server starting up ===");  

    // 获取最优线程配置  
    get_optimal_thread_config(&thread_count, &queue_size);  
    
    // 打印系统信息  
    print_system_info(thread_count, queue_size);  

    // 设置信号处理  
    struct sigaction sa;  
    memset(&sa, 0, sizeof(sa));  
    sa.sa_handler = signal_handler;  
    sigemptyset(&sa.sa_mask);  
    sa.sa_flags = 0;  
    
    if (sigaction(SIGINT, &sa, NULL) == -1) {  
        write_log("Failed to set up SIGINT handler");  
        exit(EXIT_FAILURE);  
    }  
    if (sigaction(SIGTERM, &sa, NULL) == -1) {  
        write_log("Failed to set up SIGTERM handler");  
        exit(EXIT_FAILURE);  
    }  

    // 处理命令行参数  
    for (int i = 1; i < argc; i++) {  
        if (strcmp(argv[i], "-p") == 0 && i + 1 < argc) {  
            port = atoi(argv[i + 1]);  
            i++;  
        } else if (strcmp(argv[i], "-t") == 0 && i + 1 < argc) {  
            thread_count = atoi(argv[i + 1]);  
            if (thread_count < MIN_THREADS || thread_count > MAX_THREADS) {  
                write_log("Invalid thread count specified. Using calculated value: %d", thread_count);  
            }  
            i++;  
        } else if (strcmp(argv[i], "-q") == 0 && i + 1 < argc) {  
            queue_size = atoi(argv[i + 1]);  
            if (queue_size < MIN_QUEUE_SIZE || queue_size > MAX_QUEUE_SIZE) {  
                write_log("Invalid queue size specified. Using calculated value: %d", queue_size);  
            }  
            i++;  
        }  
    }  

    // 创建线程池  
    pool = threadpool_create(thread_count, queue_size);  
    if (pool == NULL) {  
        write_log("Failed to create thread pool");  
        exit(EXIT_FAILURE);  
    }  
    write_log("Thread pool created with %d threads and queue size %d",   
              thread_count, queue_size);  

    // 创建服务器socket  
    server_socket = socket(AF_INET, SOCK_STREAM, 0);  
    if (server_socket == -1) {  
        write_log("Socket creation failed: %s", strerror(errno));  
        threadpool_destroy(pool);  
        exit(EXIT_FAILURE);  
    }  

    int opt = 1;  
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {  
        write_log("Setsockopt failed: %s", strerror(errno));  
        threadpool_destroy(pool);  
        exit(EXIT_FAILURE);  
    }  

    server_addr.sin_family = AF_INET;  
    server_addr.sin_addr.s_addr = INADDR_ANY;  
    server_addr.sin_port = htons(port);  

    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {  
        write_log("Bind failed: %s", strerror(errno));  
        threadpool_destroy(pool);  
        exit(EXIT_FAILURE);  
    }  

    if (listen(server_socket, queue_size) == -1) {  
        write_log("Listen failed: %s", strerror(errno));  
        threadpool_destroy(pool);  
        exit(EXIT_FAILURE);  
    }  

    write_log("Server successfully started on port %d", port);  
    printf("Server running on port %d with %d threads...\n", port, thread_count);  
    printf("Press Ctrl+C to stop the server\n");  

    // 主服务循环  
    while (server_running) {  
        int client_socket = accept(server_socket,   
                                 (struct sockaddr *)&client_addr,   
                                 &client_len);  
        if (client_socket == -1) {  
            if (!server_running) {  
                break;  
            }  
            write_log("Accept failed: %s", strerror(errno));  
            continue;  
        }  

        if (server_running) {  
            if (threadpool_add_task(pool, client_socket, client_addr) != 0) {  
                write_log("Failed to add task to thread pool");  
                close(client_socket);  
                continue;  
            }  
        } else {  
            close(client_socket);  
        }  
    }  

    write_log("Server shutting down...");  
    
    // 清理资源  
    if (pool) {  
        threadpool_destroy(pool);  
    }  
    if (server_socket != -1) {  
        close(server_socket);  
    }  
    write_log("=== Server shutdown complete ===");  
    close_log();  
    return 0;  
}