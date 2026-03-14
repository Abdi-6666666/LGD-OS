// sys_commands.c
#include "declarations.h"
#include <string.h>
extern Command commands[];
extern int command_count;
// 获取文件信息字符串
void get_file_info_string(const char *path, char *info_str, int buffer_size) {
    struct _finddata_t fileinfo;
    intptr_t handle = _findfirst(path, &fileinfo);

    if (handle == -1L) {
        snprintf(info_str, buffer_size, "文件不存在");
        return;
    }

    char time_str[20];
    struct tm *tm_info = localtime(&fileinfo.time_write);
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm_info);

    char size_str[20];
    if (fileinfo.size < 1024) {
        snprintf(size_str, sizeof(size_str), "%ld 字节", fileinfo.size);
    } else if (fileinfo.size < 1024 * 1024) {
        snprintf(size_str, sizeof(size_str), "%.2f KB", fileinfo.size / 1024.0);
    } else {
        snprintf(size_str, sizeof(size_str), "%.2f MB", fileinfo.size / (1024.0 * 1024.0));
    }

    char perms_str[20];
    snprintf(perms_str, sizeof(perms_str), "%s%s%s%s",
             (fileinfo.attrib & _A_RDONLY) ? "r" : "-",
             (fileinfo.attrib & _A_HIDDEN) ? "h" : "-",
             (fileinfo.attrib & _A_SYSTEM) ? "s" : "-",
             (fileinfo.attrib & _A_ARCH) ? "a" : "-");

    if (fileinfo.attrib & _A_SUBDIR) {
        snprintf(info_str, buffer_size,
                 "类型: 目录\n"
                 "名称: %s\n"
                 "大小: %s\n"
                 "修改时间: %s\n"
                 "权限: %s\n"
                 "属性: %s",
                 fileinfo.name, size_str, time_str, perms_str,
                 (fileinfo.attrib & _A_SUBDIR) ? "目录" : "文件");
    } else {
        snprintf(info_str, buffer_size,
                 "类型: 文件\n"
                 "名称: %s\n"
                 "大小: %s\n"
                 "修改时间: %s\n"
                 "权限: %s\n"
                 "属性: %s",
                 fileinfo.name, size_str, time_str, perms_str,
                 (fileinfo.attrib & _A_ARCH) ? "存档" : "普通");
    }

    _findclose(handle);
}

// info命令 - 显示文件或目录信息
int cmd_info(ShellContext *ctx, int argc, char *argv[], FILE *in, FILE *out) {
    int i;
    char real_path[MAX_PATH_LEN];
    char info_str[512];

    (void)in;  /* 标记未使用，避免警告 */

    if (argc < 2) {
        fprintf(out, "info: 缺少参数\n");
        fprintf(out, "用法: info <文件或目录>\n");
        return 1;
    }

    for (i = 1; i < argc; i++) {
        char virtual_path[MAX_PATH_LEN];
        if (strcmp(ctx->current_dir, "/") == 0) {
            snprintf(virtual_path, MAX_PATH_LEN, "/%s", argv[i]);
        } else {
            snprintf(virtual_path, MAX_PATH_LEN, "%s/%s", ctx->current_dir, argv[i]);
        }

        virtual_to_real_path(virtual_path, real_path);

        if (_access(real_path, 0) != 0) {
            fprintf(out, "info: 文件或目录不存在: %s\n", argv[i]);
            continue;
        }

        fprintf(out, "=== 信息: %s ===\n", argv[i]);
        fprintf(out, "虚拟路径: %s\n", virtual_path);
        fprintf(out, "真实路径: %s\n", real_path);

        get_file_info_string(real_path, info_str, sizeof(info_str));
        fprintf(out, "%s\n", info_str);

        fprintf(out, "\n");
    }

    return 0;
}

// exit命令
int cmd_exit(ShellContext *ctx, int argc, char *argv[], FILE *in, FILE *out) {
    (void)ctx;  /* 标记未使用，避免警告 */
    (void)argc; /* 标记未使用，避免警告 */
    (void)argv; /* 标记未使用，避免警告 */
    (void)in;   /* 标记未使用，避免警告 */
    (void)out;  /* 标记未使用，避免警告 */

    return 2;  /* 特殊返回值，表示退出 */
}

// 在 cmd_help 函数中添加对 "all" 参数的支持


int cmd_help(ShellContext *ctx, int argc, char *argv[], FILE *in, FILE *out) {
    (void)ctx;  // 未使用
    (void)in;   // 未使用

    if (argc == 1) {
        // 显示所有命令列表
        fprintf(out, "\n");
        fprintf(out, "╔════════════════════════════════════════════════════════════════╗\n");
        fprintf(out, "║                    LGD-OS Shell 命令列表                       ║\n");
        fprintf(out, "║                    版本: %s                          ║\n", LGD_OS_VERSION);
        fprintf(out, "╚════════════════════════════════════════════════════════════════╝\n");
        fprintf(out, "\n");
        fprintf(out, "══════════════════════════════════════════════════════════════════\n");
        fprintf(out, "  输入 'help <命令名>' 获取特定命令的详细帮助\n");
        fprintf(out, "  输入 'help all' 获取所有命令的详细帮助\n");
        fprintf(out, "══════════════════════════════════════════════════════════════════\n");
        fprintf(out, "\n");

        // 计算最长命令名用于对齐
        int max_name_len = 0;
        for (int i = 0; i < command_count; i++) {
            int len = (int)strlen(commands[i].name);
            if (len > max_name_len) max_name_len = len;
        }

        // 按列显示命令
        int cols = 3;
        int rows = (command_count + cols - 1) / cols;

        for (int row = 0; row < rows; row++) {
            for (int col = 0; col < cols; col++) {
                int idx = row + col * rows;
                if (idx < command_count) {
                    fprintf(out, "  %-*s  - %s",
                           max_name_len,
                           commands[idx].name,
                           commands[idx].brief);
                }
                if (col < cols - 1 && idx < command_count) {
                    fprintf(out, "   ");
                }
            }
            fprintf(out, "\n");
        }

        fprintf(out, "\n");
        fprintf(out, "══════════════════════════════════════════════════════════════════\n");
        fprintf(out, "管道支持: 使用 | 连接多个命令\n");
        fprintf(out, "  例如: ls | grep txt | wc -l\n");
        fprintf(out, "\n");
        fprintf(out, "路径支持:\n");
        fprintf(out, "  /          - 根目录\n");
        fprintf(out, "  ~          - 用户主目录 (/home/user)\n");
        fprintf(out, "  ~/path     - 用户主目录下的路径\n");
        fprintf(out, "  .          - 当前目录\n");
        fprintf(out, "  ..         - 上级目录\n");
        fprintf(out, "  /path      - 绝对路径\n");
        fprintf(out, "  path       - 相对当前目录的路径\n");
        fprintf(out, "  \"path\"     - 包含空格的路径\n");
        fprintf(out, "\n");
        fprintf(out, "内置文件系统位于系统的 builds\\ 目录下\n");
        fprintf(out, "══════════════════════════════════════════════════════════════════\n");

    } else if (argc == 2 && strcmp(argv[1], "all") == 0) {
        // 显示所有命令的详细帮助
        fprintf(out, "\n");
        fprintf(out, "╔════════════════════════════════════════════════════════════════╗\n");
        fprintf(out, "║                    LGD-OS Shell 完整帮助                       ║\n");
        fprintf(out, "║                    版本: %s                          ║\n", LGD_OS_VERSION);
        fprintf(out, "║                    共 %d 个命令                          ║\n", command_count);
        fprintf(out, "╚════════════════════════════════════════════════════════════════╝\n");

        for (int i = 0; i < command_count; i++) {
            fprintf(out, "\n");
            fprintf(out, "══════════════════════════════════════════════════════════════════\n");
            fprintf(out, "命令: %s\n", commands[i].name);
            fprintf(out, "══════════════════════════════════════════════════════════════════\n");
            fprintf(out, "\n");
            fprintf(out, "描述: %s\n", commands[i].brief);
            fprintf(out, "\n");
            fprintf(out, "详细用法:\n");
            fprintf(out, "%s\n", commands[i].detail);

            if (i < command_count - 1) {
                fprintf(out, "\n");
                fprintf(out, "------------------------------------------------------------------------\n");
            }
        }

        fprintf(out, "\n");
        fprintf(out, "══════════════════════════════════════════════════════════════════\n");
        fprintf(out, "                        LGD-OS Shell 帮助手册                     \n");
        fprintf(out, "══════════════════════════════════════════════════════════════════\n");

    } else if (argc == 2) {
        // 显示特定命令的帮助
        char *cmd_name = argv[1];
        int found = 0;

        for (int i = 0; i < command_count; i++) {
            if (strcmp(commands[i].name, cmd_name) == 0) {
                found = 1;

                fprintf(out, "\n");
                fprintf(out, "══════════════════════════════════════════════════════════════════\n");
                fprintf(out, "命令: %s\n", commands[i].name);
                fprintf(out, "══════════════════════════════════════════════════════════════════\n");
                fprintf(out, "\n");
                fprintf(out, "描述: %s\n", commands[i].brief);
                fprintf(out, "\n");
                fprintf(out, "详细用法:\n");
                fprintf(out, "%s\n", commands[i].detail);
                fprintf(out, "\n");
                fprintf(out, "══════════════════════════════════════════════════════════════════\n");
                break;
            }
        }

        if (!found) {
            fprintf(out, "错误: 未知命令 '%s'\n", cmd_name);
            fprintf(out, "输入 'help' 查看可用命令列表\n");
            fprintf(out, "输入 'help all' 查看所有命令的详细帮助\n");
            return 1;
        }
    } else {
        fprintf(out, "用法: help [命令]\n");
        fprintf(out, "  显示所有命令或特定命令的帮助信息\n");
        fprintf(out, "  参数:\n");
        fprintf(out, "    命令    (可选) 要获取帮助的命令名称\n");
        fprintf(out, "  示例:\n");
        fprintf(out, "    help          显示所有命令\n");
        fprintf(out, "    help cd       显示cd命令的详细帮助\n");
        fprintf(out, "    help all      显示所有命令的详细帮助\n");
        return 1;
    }

    return 0;
}
// osinfo命令 - 显示LGD-OS详细信息
int cmd_osinfo(ShellContext *ctx, int argc, char *argv[], FILE *in, FILE *out) {
    (void)ctx;  /* 标记未使用，避免警告 */
    (void)in;   /* 标记未使用，避免警告 */

    if (argc > 1) {
        fprintf(out, "用法: osinfo\n");
        fprintf(out, "  显示LGD-OS的详细信息及作者信息。\n");
        return 0;
    }

    // 获取当前时间和日期
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char time_str[20];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm_info);

    // 获取系统信息
    char hostname[100] = "Unknown";
    char username[100] = "user";

    #ifdef _WIN32
        DWORD size = sizeof(hostname);
        GetComputerNameA(hostname, &size);

        DWORD user_size = sizeof(username);
        GetUserNameA(username, &user_size);
    #else
        gethostname(hostname, sizeof(hostname));
        getlogin_r(username, sizeof(username));
    #endif

    // 显示系统信息
    fprintf(out, "\n");
    fprintf(out, "╔══════════════════════════════════════════════════════════════════╗\n");
    fprintf(out, "║                     LGD-OS 系统信息                              ║\n");
    fprintf(out, "║                    版本: %s                     ║\n", LGD_OS_VERSION);
    fprintf(out, "╚══════════════════════════════════════════════════════════════════╚\n");
    fprintf(out, "\n");
    fprintf(out, "════════════════════════════════════════════════════════════════════\n");
    fprintf(out, "                          系统信息                                  \n");
    fprintf(out, "════════════════════════════════════════════════════════════════════\n");
    fprintf(out, "\n");
    fprintf(out, "系统名称: LGD-OS (Linux-like GUI Desktop Operating System)\n");
    fprintf(out, "版本号: %s\n", LGD_OS_VERSION);
    fprintf(out, "编译时间: %s\n", time_str);
    fprintf(out, "主机名: %s\n", hostname);
    fprintf(out, "当前用户: %s\n", username);
    fprintf(out, "当前目录: %s\n", ctx->current_dir);
    fprintf(out, "\n");
    fprintf(out, "════════════════════════════════════════════════════════════════════\n");
    fprintf(out, "                          开发者信息                                \n");
    fprintf(out, "════════════════════════════════════════════════════════════════════\n");
    fprintf(out, "\n");
    fprintf(out, "项目名称: LGD-OS Shell\n");
    fprintf(out, "开发者: SSE_107\n");
    fprintf(out, "项目类型: Windows控制台应用程序\n");
    fprintf(out, "开发语言: C语言\n");
    fprintf(out, "编译器: MinGW GCC\n");
    fprintf(out, "\n");
    fprintf(out, "════════════════════════════════════════════════════════════════════\n");
    fprintf(out, "                    SSE_107 项目 - 感谢使用！                      \n");
    fprintf(out, "════════════════════════════════════════════════════════════════════\n");
    fprintf(out, "\n");

    return 0;
}
