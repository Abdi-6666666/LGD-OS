// dir_commands.c
#include "declarations.h"
#include <string.h>
#include <io.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <time.h>

// ls命令 - 在虚拟路径中列出目录
int cmd_ls(ShellContext *ctx, int argc, char *argv[], FILE *in, FILE *out) {
    /* 声明所有变量 */
    int i;
    int show_all = 0;
    int long_format = 0;
    char real_path[MAX_PATH_LEN];
    char search_path[MAX_PATH_LEN];
    struct _finddata_t fileinfo;
    intptr_t handle;
    int count = 0;
    char *target_path = NULL;

    (void)in;  /* 标记未使用，避免警告 */

    /* 解析参数 */
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-a") == 0) {
            show_all = 1;
        } else if (strcmp(argv[i], "-l") == 0) {
            long_format = 1;
        } else if (argv[i][0] != '-') {
            target_path = argv[i];
        }
    }

    /* 获取目标路径 */
    if (target_path == NULL) {
        /* 使用当前目录 */
        virtual_to_real_path(ctx->current_dir, real_path);
    } else {
        /* 如果是 ~ 开头的路径，需要特殊处理 */
        if (target_path[0] == '~') {
            char temp_path[MAX_PATH_LEN];
            if (target_path[1] == '\0' || target_path[1] == '/') {
                /* ~ 或 ~/path */
                virtual_to_real_path(target_path, temp_path);
            } else {
                /* 其他情况，当作普通路径处理 */
                snprintf(temp_path, MAX_PATH_LEN, "%s", target_path);
            }
            virtual_to_real_path(temp_path, real_path);
        } else {
            virtual_to_real_path(target_path, real_path);
        }
    }

    /* 添加通配符以列出所有文件 */
    snprintf(search_path, MAX_PATH_LEN, "%s\\*.*", real_path);

    /* 在真实路径下列出文件 */
    handle = _findfirst(search_path, &fileinfo);

    if (handle == -1L) {
        fprintf(out, "ls: 目录为空\n");
        return 0;
    }

    if (long_format) {
        // 详细格式
        int file_count = 0;
        int dir_count = 0;
        long total_size = 0;
        char time_str[20];
        struct tm *tm_info;

        fprintf(out, "类型\t大小\t修改时间\t\t名称\n");
        fprintf(out, "----------------------------------------------------------\n");

        do {
            /* 跳过当前目录和上级目录 */
            if (strcmp(fileinfo.name, ".") == 0 || strcmp(fileinfo.name, "..") == 0) {
                continue;
            }

            /* 检查是否需要显示隐藏文件 */
            if (!show_all && fileinfo.name[0] == '.') {
                continue;
            }

            tm_info = localtime(&fileinfo.time_write);
            strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M", tm_info);

            if ((fileinfo.attrib & _A_SUBDIR) != 0) {
                fprintf(out, "[目录]\t%8s\t%s\t%s\n", "-", time_str, fileinfo.name);
                dir_count++;
            } else {
                fprintf(out, "文件\t%8ld\t%s\t%s\n", fileinfo.size, time_str, fileinfo.name);
                file_count++;
                total_size += fileinfo.size;
            }
        } while (_findnext(handle, &fileinfo) == 0);

        fprintf(out, "\n总计: %d 个文件, %d 个目录, %ld 字节\n", file_count, dir_count, total_size);
    } else {
        // 简单格式
        do {
            /* 跳过当前目录和上级目录 */
            if (strcmp(fileinfo.name, ".") == 0 || strcmp(fileinfo.name, "..") == 0) {
                continue;
            }

            /* 检查是否需要显示隐藏文件 */
            if (!show_all && fileinfo.name[0] == '.') {
                continue;
            }

            if ((fileinfo.attrib & _A_SUBDIR) != 0) {
                fprintf(out, "[%s]  ", fileinfo.name);
            } else {
                fprintf(out, "%s  ", fileinfo.name);
            }
            count++;

            if (count % 4 == 0) {
                fprintf(out, "\n");
            }
        } while (_findnext(handle, &fileinfo) == 0);

        if (count % 4 != 0) {
            fprintf(out, "\n");
        }
    }

    _findclose(handle);
    return 0;
}



// cd命令 - 虚拟目录切换
int cmd_cd(ShellContext *ctx, int argc, char *argv[], FILE *in, FILE *out) {
    (void)in;  /* 标记未使用，避免警告 */

    if (argc < 2) {
        /* cd 不带参数，默认到~（用户家目录） */
        char virtual_path[MAX_PATH_LEN];
        char real_path[MAX_PATH_LEN];

        strcpy(virtual_path, "~");
        virtual_to_real_path(virtual_path, real_path);  // 使用 virtual_to_real_path

        if (_access(real_path, 0) == 0 && is_directory(real_path)) {  // 使用 is_directory
            strcpy(ctx->current_dir, "/home/user");
            fprintf(out, "当前目录: /home/user\n");
        } else {
            fprintf(out, "cd: 目录不存在: ~\n");
            return 1;
        }
        return 0;
    }

    /* 获取用户输入的路径 */
    char input_path[MAX_PATH_LEN];
    strcpy(input_path, argv[1]);

    /* 处理特殊情况 */
    if (strcmp(input_path, "/") == 0) {
        /* 切换到根目录 */
        strcpy(ctx->current_dir, "/");
        fprintf(out, "当前目录: /\n");
        return 0;
    } else if (strcmp(input_path, "~") == 0) {
        /* 切换到用户家目录 */
        char real_path[MAX_PATH_LEN];
        virtual_to_real_path("~", real_path);  // 使用 virtual_to_real_path

        if (_access(real_path, 0) == 0 && is_directory(real_path)) {  // 使用 is_directory
            strcpy(ctx->current_dir, "/home/user");
            fprintf(out, "当前目录: /home/user\n");
        } else {
            fprintf(out, "cd: 目录不存在: ~\n");
            return 1;
        }
        return 0;
    } else if (strcmp(input_path, "..") == 0) {
        /* 上级目录 */
        if (strcmp(ctx->current_dir, "/") == 0) {
            /* 已经在根目录，保持不动 */
            fprintf(out, "当前目录: /\n");
        } else {
            /* 找到最后一个/ */
            char temp[MAX_PATH_LEN];
            strcpy(temp, ctx->current_dir);
            char *last_slash = strrchr(temp, '/');
            if (last_slash) {
                *last_slash = '\0';
                if (strlen(temp) == 0) {
                    strcpy(temp, "/");
                }
            }
            strcpy(ctx->current_dir, temp);
            fprintf(out, "当前目录: %s\n", ctx->current_dir);
        }
        return 0;
    } else if (strcmp(input_path, ".") == 0) {
        /* 当前目录 */
        fprintf(out, "当前目录: %s\n", ctx->current_dir);
        return 0;
    }

    /* 构建新路径 */
    char new_virtual_path[MAX_PATH_LEN];

    if (input_path[0] == '/') {
        /* 绝对路径 */
        strcpy(new_virtual_path, input_path);
    } else if (strncmp(input_path, "~/", 2) == 0) {
        /* ~/path 形式 */
        snprintf(new_virtual_path, MAX_PATH_LEN, "/home/user/%s", input_path + 2);
    } else {
        /* 相对路径 */
        if (strcmp(ctx->current_dir, "/") == 0) {
            snprintf(new_virtual_path, MAX_PATH_LEN, "/%s", input_path);
        } else {
            snprintf(new_virtual_path, MAX_PATH_LEN, "%s/%s", ctx->current_dir, input_path);
        }
    }

    /* 规范化路径（处理 . 和 ..）*/
    normalize_path(new_virtual_path);  // 使用 normalize_path

    /* 转换为真实路径检查目录是否存在 */
    char real_path[MAX_PATH_LEN];
    virtual_to_real_path(new_virtual_path, real_path);  // 使用 virtual_to_real_path

    /* 检查目录是否存在 */
    if (_access(real_path, 0) != 0) {
        fprintf(out, "cd: 目录不存在: %s\n", input_path);
        return 1;
    }

    /* 检查是否是目录 */
    if (!is_directory(real_path)) {  // 使用 is_directory
        fprintf(out, "cd: 不是目录: %s\n", input_path);
        return 1;
    }

    /* 设置当前目录 */
    strcpy(ctx->current_dir, new_virtual_path);
    fprintf(out, "当前目录: %s\n", ctx->current_dir);

    return 0;
}



// ll命令 - 详细列出目录
int cmd_ll(ShellContext *ctx, int argc, char *argv[], FILE *in, FILE *out) {
    /* ll 只是 ls -l 的别名，调用 ls 函数 */
    char *new_argv[MAX_ARGS + 2];
    int new_argc = 0;

    new_argv[new_argc++] = "ls";
    new_argv[new_argc++] = "-l";

    for (int i = 1; i < argc; i++) {
        new_argv[new_argc++] = argv[i];
    }
    new_argv[new_argc] = NULL;

    return cmd_ls(ctx, new_argc, new_argv, in, out);
}

// pwd命令 - 显示虚拟路径
int cmd_pwd(ShellContext *ctx, int argc, char *argv[], FILE *in, FILE *out) {
    (void)argc;  /* 标记未使用，避免警告 */
    (void)argv;  /* 标记未使用，避免警告 */
    (void)in;    /* 标记未使用，避免警告 */

    fprintf(out, "%s\n", ctx->current_dir);
    return 0;
}

// mkdir命令 - 在虚拟路径中创建目录
int cmd_mkdir(ShellContext *ctx, int argc, char *argv[], FILE *in, FILE *out) {
    int i;
    int result = 0;
    char virtual_path[MAX_PATH_LEN];
    char real_path[MAX_PATH_LEN];

    (void)in;  /* 标记未使用，避免警告 */

    if (argc < 2) {
        fprintf(out, "mkdir: 缺少参数\n");
        fprintf(out, "用法: mkdir <目录名>\n");
        return 1;
    }

    for (i = 1; i < argc; i++) {
        /* 构建完整的虚拟路径 */
        if (strcmp(ctx->current_dir, "/") == 0) {
            snprintf(virtual_path, MAX_PATH_LEN, "/%s", argv[i]);
        } else {
            snprintf(virtual_path, MAX_PATH_LEN, "%s/%s", ctx->current_dir, argv[i]);
        }

        /* 转换为真实路径 */
        virtual_to_real_path(virtual_path, real_path);

        if (_mkdir(real_path) != 0) {
            fprintf(out, "mkdir: 无法创建目录 '%s'\n", argv[i]);
            result = 1;
        } else {
            fprintf(out, "已创建目录: %s\n", argv[i]);
        }
    }

    return result;
}

// rmdir命令 - 在虚拟路径中删除目录
int cmd_rmdir(ShellContext *ctx, int argc, char *argv[], FILE *in, FILE *out) {
    int i;
    int result = 0;
    char virtual_path[MAX_PATH_LEN];
    char real_path[MAX_PATH_LEN];

    (void)in;  /* 标记未使用，避免警告 */

    if (argc < 2) {
        fprintf(out, "rmdir: 缺少参数\n");
        fprintf(out, "用法: rmdir <目录名>\n");
        return 1;
    }

    for (i = 1; i < argc; i++) {
        /* 构建完整的虚拟路径 */
        if (strcmp(ctx->current_dir, "/") == 0) {
            snprintf(virtual_path, MAX_PATH_LEN, "/%s", argv[i]);
        } else {
            snprintf(virtual_path, MAX_PATH_LEN, "%s/%s", ctx->current_dir, argv[i]);
        }

        /* 转换为真实路径 */
        virtual_to_real_path(virtual_path, real_path);

        if (_rmdir(real_path) != 0) {
            fprintf(out, "rmdir: 无法删除目录 '%s'\n", argv[i]);
            result = 1;
        } else {
            fprintf(out, "已删除目录: %s\n", argv[i]);
        }
    }

    return result;
}
