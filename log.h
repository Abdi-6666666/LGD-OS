// log.h
#ifndef LOG_H
#define LOG_H

#include <stdio.h>
#include <time.h>

// 日志级别枚举
typedef enum {
    LOG_LEVEL_DEBUG = 0,
    LOG_LEVEL_INFO = 1,
    LOG_LEVEL_WARNING = 2,
    LOG_LEVEL_ERROR = 3,
    LOG_LEVEL_CRITICAL = 4
} LogLevel;

// 日志配置结构
typedef struct {
    int enabled;                // 是否启用日志
    LogLevel level;             // 日志级别
    char log_dir[1024];         // 日志目录路径
    int max_files;              // 最大日志文件数
    long max_file_size;         // 最大文件大小（字节）
    int use_daily_files;        // 是否按日期分割文件
} LogConfig;

// 日志上下文
typedef struct {
    LogConfig config;
    FILE *current_log_file;
    char current_filename[1024];
} Logger;

// 函数声明
Logger* log_init(const char *exe_path);
void log_cleanup(Logger *logger);
void log_message(Logger *logger, LogLevel level, const char *function,
                 int line, const char *format, ...);
void log_command(Logger *logger, const char *command, const char *working_dir);

// 便捷的日志宏
#define LOG_DEBUG(logger, fmt, ...) \
    log_message(logger, LOG_LEVEL_DEBUG, __FUNCTION__, __LINE__, fmt, ##__VA_ARGS__)

#define LOG_INFO(logger, fmt, ...) \
    log_message(logger, LOG_LEVEL_INFO, __FUNCTION__, __LINE__, fmt, ##__VA_ARGS__)

#define LOG_WARNING(logger, fmt, ...) \
    log_message(logger, LOG_LEVEL_WARNING, __FUNCTION__, __LINE__, fmt, ##__VA_ARGS__)

#define LOG_ERROR(logger, fmt, ...) \
    log_message(logger, LOG_LEVEL_ERROR, __FUNCTION__, __LINE__, fmt, ##__VA_ARGS__)

#define LOG_CRITICAL(logger, fmt, ...) \
    log_message(logger, LOG_LEVEL_CRITICAL, __FUNCTION__, __LINE__, fmt, ##__VA_ARGS__)

#endif // LOG_H
