// log.h  
#ifndef LOG_H  
#define LOG_H  

int init_logging(void);  
void write_log(const char* format, ...);  
void close_log(void);  

#endif  

// log.c  
#include <stdio.h>  
#include <stdlib.h>  
#include <string.h>  
#include <time.h>  
#include <sys/stat.h>  
#include <errno.h>  
#include <stdarg.h>  
#include <unistd.h>  

#define LOG_DIR "../logs"  
#define MAX_LOG_FILES 10  
#define MAX_LOG_SIZE (100 * 1024 * 1024)  // 100MB  
#define MAX_TOTAL_SIZE (1024 * 1024 * 1024)  // 1GB  

static FILE* current_log = NULL;  
static int current_log_number = 1;  

static long get_file_size(FILE* file) {  
    if (!file) return 0;  
    
    long current_pos = ftell(file);  
    if (current_pos == -1) {  
        fprintf(stderr, "ftell failed: %s\n", strerror(errno));  
        return 0;  
    }  
    
    if (fseek(file, 0, SEEK_END) != 0) {  
        fprintf(stderr, "fseek failed: %s\n", strerror(errno));  
        return 0;  
    }  
    
    long size = ftell(file);  
    if (fseek(file, current_pos, SEEK_SET) != 0) {  
        fprintf(stderr, "fseek failed: %s\n", strerror(errno));  
        return 0;  
    }  
    
    return size;  
}  

static void rotate_log() {  
    if (current_log) {  
        fclose(current_log);  
        current_log = NULL;  
    }  
    
    current_log_number = (current_log_number % MAX_LOG_FILES) + 1;  
    
    char new_log_path[256];  
    snprintf(new_log_path, sizeof(new_log_path), "%s/server_%d.log", LOG_DIR, current_log_number);  
    
    // 使用 "a+" 模式打开文件，并立即禁用缓冲  
    current_log = fopen(new_log_path, "a+");  
    if (!current_log) {  
        fprintf(stderr, "Failed to create new log file %s: %s\n",   
                new_log_path, strerror(errno));  
        return;  
    }  
    
    // 禁用缓冲  
    setvbuf(current_log, NULL, _IONBF, 0);  
    
    // 写入轮转标记  
    time_t now = time(NULL);  
    char time_str[64];  
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", localtime(&now));  
    fprintf(current_log, "\n=== Log rotated at %s ===\n\n", time_str);  
}  

int init_logging() {  
    // 获取当前工作目录  
    char cwd[256];  
    if (getcwd(cwd, sizeof(cwd)) != NULL) {  
        fprintf(stderr, "Current working directory: %s\n", cwd);  
    }  
    
    // 创建日志目录  
    if (mkdir(LOG_DIR, 0755) != 0 && errno != EEXIST) {  
        fprintf(stderr, "Failed to create log directory %s: %s\n",   
                LOG_DIR, strerror(errno));  
        return -1;  
    }  
    
    char log_path[256];  
    snprintf(log_path, sizeof(log_path), "%s/server_%d.log", LOG_DIR, current_log_number);  
    
    // 使用 "a+" 模式打开文件  
    current_log = fopen(log_path, "a+");  
    if (!current_log) {  
        fprintf(stderr, "Failed to open log file %s: %s\n",   
                log_path, strerror(errno));  
        return -1;  
    }  
    
    // 禁用缓冲  
    setvbuf(current_log, NULL, _IONBF, 0);  
    
    // 写入启动标记  
    time_t now = time(NULL);  
    char time_str[64];  
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", localtime(&now));  
    fprintf(current_log, "\n=== Logging started at %s ===\n\n", time_str);  
    
    return 0;  
}  

void write_log(const char* format, ...) {  
    if (!current_log) {  
        fprintf(stderr, "Logging system not initialized\n");  
        return;  
    }  
    
    // 获取当前时间  
    time_t now = time(NULL);  
    char time_str[64];  
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", localtime(&now));  
    
    // 获取进程ID  
    pid_t pid = getpid();  
    
    // 写入时间戳和进程ID  
    fprintf(current_log, "[%s][PID:%d] ", time_str, pid);  
    
    // 写入日志内容  
    va_list args;  
    va_start(args, format);  
    vfprintf(current_log, format, args);  
    va_end(args);  
    
    // 写入换行符  
    fprintf(current_log, "\n");  
    
    // 检查是否需要轮转  
    if (get_file_size(current_log) > MAX_LOG_SIZE) {  
        rotate_log();  
    }  
}  

void close_log() {  
    if (current_log) {  
        // 写入关闭标记  
        time_t now = time(NULL);  
        char time_str[64];  
        strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", localtime(&now));  
        fprintf(current_log, "\n=== Logging stopped at %s ===\n", time_str);  
        
        fclose(current_log);  
        current_log = NULL;  
    }  
}