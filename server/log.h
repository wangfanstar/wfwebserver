#ifndef LOG_H  
#define LOG_H  

#include <stdio.h>  

// 初始化日志系统  
void init_logging(void);  

// 写入日志  
void write_log(const char* format, ...);  

// 关闭日志  
void close_log(void);  

#endif // LOG_H