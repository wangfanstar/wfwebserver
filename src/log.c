#include <stdio.h>  
#include <stdlib.h>  
#include <string.h>  
#include <time.h>  
#include <sys/stat.h>  
#include <errno.h>  
#include <stdarg.h>  
#include <unistd.h>  

#define LOG_DIR "../log"  
#define MAX_LOG_FILES 10  
#define MAX_LOG_SIZE (100 * 1024 * 1024)  // 100MB  
#define MAX_TOTAL_SIZE (1024 * 1024 * 1024)  // 1GB  

// 日志文件句柄  
static FILE* current_log = NULL;  
static int current_log_number = 1;  

// 获取当前日志文件大小  
static long get_file_size(FILE* file) {  
    long current_pos = ftell(file);  
    fseek(file, 0, SEEK_END);  
    long size = ftell(file);  
    fseek(file, current_pos, SEEK_SET);  
    return size;  
}  

// 获取所有日志文件的总大小  
static long get_total_log_size() {  
    struct stat st;  
    char log_path[256];  
    long total_size = 0;  
    
    for (int i = 1; i <= MAX_LOG_FILES; i++) {  
        snprintf(log_path, sizeof(log_path), "%s/server_%d.log", LOG_DIR, i);  
        if (stat(log_path, &st) == 0) {  
            total_size += st.st_size;  
        }  
    }  
    return total_size;  
}  

// 轮转日志文件  
static void rotate_log() {  
    if (current_log) {  
        fclose(current_log);  
    }  
    
    current_log_number = (current_log_number % MAX_LOG_FILES) + 1;  
    
    // 如果总大小超过限制，删除最旧的日志  
    while (get_total_log_size() > MAX_TOTAL_SIZE) {  
        char oldest_log[256];  
        snprintf(oldest_log, sizeof(oldest_log), "%s/server_%d.log", LOG_DIR,   
                ((current_log_number - 2 + MAX_LOG_FILES) % MAX_LOG_FILES) + 1);  
        remove(oldest_log);  
    }  
    
    char new_log_path[256];  
    snprintf(new_log_path, sizeof(new_log_path), "%s/server_%d.log", LOG_DIR, current_log_number);  
    current_log = fopen(new_log_path, "w");  
    if (!current_log) {  
        perror("Failed to create new log file");  
        exit(EXIT_FAILURE);  
    }  
}  

void init_logging() {  
    // 创建日志目录  
    mkdir(LOG_DIR, 0755);  
    
    // 查找当前最新的日志文件  
    struct stat st;  
    char log_path[256];  
    for (int i = 1; i <= MAX_LOG_FILES; i++) {  
        snprintf(log_path, sizeof(log_path), "%s/server_%d.log", LOG_DIR, i);  
        if (stat(log_path, &st) == -1) {  
            current_log_number = i;  
            break;  
        }  
    }  
    
    // 打开当前日志文件  
    snprintf(log_path, sizeof(log_path), "%s/server_%d.log", LOG_DIR, current_log_number);  
    current_log = fopen(log_path, "a");  
    if (!current_log) {  
        perror("Failed to open log file");  
        exit(EXIT_FAILURE);  
    }  
}  

void write_log(const char* format, ...) {  
    if (!current_log) return;  
    
    // 检查日志大小  
    if (get_file_size(current_log) > MAX_LOG_SIZE) {  
        rotate_log();  
    }  
    
    // 获取当前时间  
    time_t now;  
    time(&now);  
    char time_str[64];  
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", localtime(&now));  
    
    // 写入时间戳  
    fprintf(current_log, "[%s] ", time_str);  
    
    // 写入日志内容  
    va_list args;  
    va_start(args, format);  
    vfprintf(current_log, format, args);  
    va_end(args);  
    
    // 写入换行符并刷新缓冲区  
    fprintf(current_log, "\n");  
    fflush(current_log);  
}  

void close_log() {  
    if (current_log) {  
        fclose(current_log);  
        current_log = NULL;  
    }  
}