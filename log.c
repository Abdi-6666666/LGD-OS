// log.c
#include "log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <ctype.h>

#ifdef _WIN32
    #include <windows.h>
    #include <direct.h>
    #define MKDIR(path) _mkdir(path)
    #define PATH_SEPARATOR '\\'
#else
    #include <unistd.h>
    #include <libgen.h>
    #define MKDIR(path) mkdir(path, 0755)
    #define PATH_SEPARATOR '/'
#endif

// 静态函数声明
static void ensure_log_dir(const char *log_dir);
static void get_log_filename(Logger *logger, char *buffer, size_t size);
static void rotate_log_if_needed(Logger *logger);
static void close_current_logfile(Logger *logger);
static int open_logfile(Logger *logger, const char *filename);
static const char* level_to_string(LogLevel level);
static void cleanup_old_logs(Logger *logger, const char *current_file);

// 日志系统初始化
Logger* log_init(const char *exe_path) {
    Logger *logger = (Logger*)malloc(sizeof(Logger));
    if (!logger) {
        return NULL;
    }

    // 设置默认配置
    logger->config.enabled = 1;
    logger->config.level = LOG_LEVEL_INFO;
    logger->config.max_files = 30;  // 保留30个日志文件
    logger->config.max_file_size = 10 * 1024 * 1024;  // 10MB
    logger->config.use_daily_files = 1;
    logger->current_log_file = NULL;
    logger->current_filename[0] = '\0';

    // 获取可执行文件所在目录
    char exe_dir[1024] = {0};

    if (exe_path && exe_path[0] != '\0') {
        // 从参数获取
        strncpy(exe_dir, exe_path, sizeof(exe_dir) - 1);
    } else {
        // 尝试自动获取
#ifdef _WIN32
        GetModuleFileNameA(NULL, exe_dir, sizeof(exe_dir));
#else
        ssize_t count = readlink("/proc/self/exe", exe_dir, sizeof(exe_dir) - 1);
        if (count <= 0) {
            // 备用方法
            if (getcwd(exe_dir, sizeof(exe_dir)) == NULL) {
                strcpy(exe_dir, ".");
            }
        } else {
            exe_dir[count] = '\0';
        }
#endif
    }

    // 提取目录部分
    char *last_slash = strrchr(exe_dir, '/');
    if (!last_slash) {
        last_slash = strrchr(exe_dir, '\\');
    }

    if (last_slash) {
        *last_slash = '\0';  // 去掉文件名，只保留目录
    }

    // 构建日志目录路径
    snprintf(logger->config.log_dir, sizeof(logger->config.log_dir),
             "%s%clog", exe_dir, PATH_SEPARATOR);

    // 确保日志目录存在
    ensure_log_dir(logger->config.log_dir);

    // 获取当前日志文件名
    char log_filename[1024];
    get_log_filename(logger, log_filename, sizeof(log_filename));

    // 清理旧的日志文件
    cleanup_old_logs(logger, log_filename);

    // 打开日志文件
    if (!open_logfile(logger, log_filename)) {
        free(logger);
        return NULL;
    }

    // 写入启动日志
    LOG_INFO(logger, "=== LGD-OS Shell 日志系统启动 ===");
    LOG_INFO(logger, "日志目录: %s", logger->config.log_dir);
    LOG_INFO(logger, "日志级别: %s", level_to_string(logger->config.level));

    return logger;
}

// 清理日志系统
void log_cleanup(Logger *logger) {
    if (!logger) return;

    if (logger->current_log_file) {
        LOG_INFO(logger, "=== LGD-OS Shell 日志系统关闭 ===");
        fclose(logger->current_log_file);
        logger->current_log_file = NULL;
    }

    free(logger);
}

// 确保日志目录存在
static void ensure_log_dir(const char *log_dir) {
    struct stat st = {0};

    if (stat(log_dir, &st) == -1) {
        // 创建目录
        if (MKDIR(log_dir) != 0) {
            // 如果创建失败，尝试创建父目录
            char parent_dir[1024];
            strncpy(parent_dir, log_dir, sizeof(parent_dir) - 1);

            char *last_sep = strrchr(parent_dir, PATH_SEPARATOR);
            if (last_sep) {
                *last_sep = '\0';
                ensure_log_dir(parent_dir);
                MKDIR(log_dir);
            }
        }
    }
}

// 获取日志文件名
static void get_log_filename(Logger *logger, char *buffer, size_t size) {
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);

    if (logger->config.use_daily_files) {
        // 按日期分割的日志文件
        snprintf(buffer, size, "%s%cshell_%04d%02d%02d.log",
                 logger->config.log_dir, PATH_SEPARATOR,
                 tm_info->tm_year + 1900,
                 tm_info->tm_mon + 1,
                 tm_info->tm_mday);
    } else {
        // 单个日志文件
        snprintf(buffer, size, "%s%cshell.log",
                 logger->config.log_dir, PATH_SEPARATOR);
    }
}

// 检查并轮转日志文件
static void rotate_log_if_needed(Logger *logger) {
    if (!logger->current_log_file) return;

    // 检查文件大小
    fseek(logger->current_log_file, 0, SEEK_END);
    long file_size = ftell(logger->current_log_file);
    fseek(logger->current_log_file, 0, SEEK_SET);

    if (file_size > logger->config.max_file_size) {
        // 文件太大，需要轮转
        close_current_logfile(logger);

        // 重命名当前文件
        time_t now = time(NULL);
        struct tm *tm_info = localtime(&now);
        char backup_name[1024];

        snprintf(backup_name, sizeof(backup_name),
                "%s%cshell_%04d%02d%02d_%02d%02d%02d_backup.log",
                logger->config.log_dir, PATH_SEPARATOR,
                tm_info->tm_year + 1900,
                tm_info->tm_mon + 1,
                tm_info->tm_mday,
                tm_info->tm_hour,
                tm_info->tm_min,
                tm_info->tm_sec);

        rename(logger->current_filename, backup_name);

        // 重新打开日志文件
        char new_filename[1024];
        get_log_filename(logger, new_filename, sizeof(new_filename));
        open_logfile(logger, new_filename);
    }
}

// 关闭当前日志文件
static void close_current_logfile(Logger *logger) {
    if (logger->current_log_file) {
        fclose(logger->current_log_file);
        logger->current_log_file = NULL;
    }
}

// 打开日志文件
static int open_logfile(Logger *logger, const char *filename) {
    close_current_logfile(logger);

    logger->current_log_file = fopen(filename, "a");
    if (!logger->current_log_file) {
        return 0;
    }

    strncpy(logger->current_filename, filename, sizeof(logger->current_filename) - 1);
    return 1;
}

// 将日志级别转换为字符串
static const char* level_to_string(LogLevel level) {
    switch (level) {
        case LOG_LEVEL_DEBUG:    return "DEBUG";
        case LOG_LEVEL_INFO:     return "INFO";
        case LOG_LEVEL_WARNING:  return "WARNING";
        case LOG_LEVEL_ERROR:    return "ERROR";
        case LOG_LEVEL_CRITICAL: return "CRITICAL";
        default:                 return "UNKNOWN";
    }
}

// 清理旧的日志文件
static void cleanup_old_logs(Logger *logger, const char *current_file) {
    // 这里可以添加清理旧日志文件的逻辑
    // 由于时间关系，这里只保留框架
    // 实际实现可能需要遍历目录，删除最旧的文件
}

// 记录消息
void log_message(Logger *logger, LogLevel level, const char *function,
                 int line, const char *format, ...) {
    if (!logger || !logger->config.enabled || level < logger->config.level) {
        return;
    }

    // 检查是否需要轮转日志
    rotate_log_if_needed(logger);

    if (!logger->current_log_file) {
        // 重新打开日志文件
        char filename[1024];
        get_log_filename(logger, filename, sizeof(filename));
        if (!open_logfile(logger, filename)) {
            return;
        }
    }

    // 获取当前时间
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);

    // 写入时间戳和日志级别
    fprintf(logger->current_log_file,
            "[%04d-%02d-%02d %02d:%02d:%02d] [%-8s] ",
            tm_info->tm_year + 1900,
            tm_info->tm_mon + 1,
            tm_info->tm_mday,
            tm_info->tm_hour,
            tm_info->tm_min,
            tm_info->tm_sec,
            level_to_string(level));

    // 写入函数名和行号
    if (function && function[0] != '\0') {
        fprintf(logger->current_log_file, "[%s:%d] ", function, line);
    }

    // 处理消息
    va_list args;
    va_start(args, format);
    vfprintf(logger->current_log_file, format, args);
    va_end(args);

    fprintf(logger->current_log_file, "\n");
    fflush(logger->current_log_file);

    // 如果是错误级别，同时在控制台输出
    if (level >= LOG_LEVEL_ERROR) {
        fprintf(stderr, "[%s] ", level_to_string(level));
        va_start(args, format);
        vfprintf(stderr, format, args);
        va_end(args);
        fprintf(stderr, "\n");
    }
}

// 记录命令
void log_command(Logger *logger, const char *command, const char *working_dir) {
    if (!logger || !command || command[0] == '\0') {
        return;
    }

    if (working_dir) {
        LOG_INFO(logger, "CMD [%s]: %s", working_dir, command);
    } else {
        LOG_INFO(logger, "CMD: %s", command);
    }
}
