// declarations.h
#ifndef DECLARATIONS_H
#define DECLARATIONS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <direct.h>
#include <io.h>
#include <ctype.h>

#ifdef _WIN32
#include <windows.h>
#endif

// 版本号
#define LGD_OS_VERSION "LGD-OS 1.1 builds20262251920"
#define BUILDS_DIR "builds"

// 常量
#define MAX_CMD_LEN 1024
#define MAX_PATH_LEN 1024
#define MAX_ARGS 32
#define MAX_HISTORY 100
#define MAX_FILENAME 256
#define MAX_CONTENT_LEN 4096
#define MAX_PIPES 10
#define MAX_OUTPUT_LEN 8192

// Shell上下文
typedef struct ShellContext {
    char current_dir[MAX_PATH_LEN];  // 虚拟当前目录
    char *history[MAX_HISTORY];
    int history_count;
} ShellContext;

// 命令结构
typedef struct CommandEntry {
    char name[MAX_FILENAME];
    int (*function)(ShellContext *ctx, int argc, char *argv[], FILE *in, FILE *out);
} CommandEntry;

// Shell函数
ShellContext *shell_init(void);
void shell_cleanup(ShellContext *ctx);
void shell_display_prompt(ShellContext *ctx);
void shell_run(ShellContext *ctx);
int shell_execute_command(ShellContext *ctx, const char *cmd_line);
int shell_execute_pipeline(ShellContext *ctx, const char *pipeline_cmd);

// 基础命令
int cmd_cd(ShellContext *ctx, int argc, char *argv[], FILE *in, FILE *out);
int cmd_ls(ShellContext *ctx, int argc, char *argv[], FILE *in, FILE *out);
int cmd_ll(ShellContext *ctx, int argc, char *argv[], FILE *in, FILE *out);
int cmd_pwd(ShellContext *ctx, int argc, char *argv[], FILE *in, FILE *out);
int cmd_mkdir(ShellContext *ctx, int argc, char *argv[], FILE *in, FILE *out);
int cmd_rmdir(ShellContext *ctx, int argc, char *argv[], FILE *in, FILE *out);
int cmd_cat(ShellContext *ctx, int argc, char *argv[], FILE *in, FILE *out);
int cmd_touch(ShellContext *ctx, int argc, char *argv[], FILE *in, FILE *out);
int cmd_rm(ShellContext *ctx, int argc, char *argv[], FILE *in, FILE *out);
int cmd_info(ShellContext *ctx, int argc, char *argv[], FILE *in, FILE *out);
int cmd_edit(ShellContext *ctx, int argc, char *argv[], FILE *in, FILE *out);
int cmd_echo(ShellContext *ctx, int argc, char *argv[], FILE *in, FILE *out);
int cmd_grep(ShellContext *ctx, int argc, char *argv[], FILE *in, FILE *out);
int cmd_wc(ShellContext *ctx, int argc, char *argv[], FILE *in, FILE *out);
int cmd_help(ShellContext *ctx, int argc, char *argv[], FILE *in, FILE *out);
int cmd_exit(ShellContext *ctx, int argc, char *argv[], FILE *in, FILE *out);
int cmd_cp(ShellContext *ctx, int argc, char *argv[], FILE *in, FILE *out);
int cmd_mv(ShellContext *ctx, int argc, char *argv[], FILE *in, FILE *out);
// 文件系统工具
int ensure_builds_directory(void);
void create_linux_directory_structure(void);
int create_directory_if_not_exists(const char *path);
int create_directory_if_not_exists_silent(const char *path);
void sync_existing_directories_silent(void);

// 路径转换函数
void virtual_to_real_path(const char *virtual_path, char *real_path);
int is_directory(const char *path);
int is_file(const char *path);
void get_file_info_string(const char *path, char *info_str, int buffer_size);

// 颜色控制函数
void set_console_color(int color);
void reset_console_color(void);
void get_hostname(char *buffer, int size);
void get_username(char *buffer, int size);
void normalize_path(char *path);
#endif // DECLARATIONS_H
