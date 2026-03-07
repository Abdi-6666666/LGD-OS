// commands.c
#include "declarations.h"
#include <io.h>
#include <time.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>  // 添加errno.h
#ifdef _WIN32
#include <windows.h>  // Windows API
#endif

// 将虚拟路径转换为真实路径
void virtual_to_real_path(const char *virtual_path, char *real_path) {
    int i;
    char temp_path[MAX_PATH_LEN];

    // 处理 ~
    if (strcmp(virtual_path, "~") == 0) {
        // 用户家目录
        snprintf(real_path, MAX_PATH_LEN, "%s\\home\\user", BUILDS_DIR);
    } else if (strncmp(virtual_path, "~/", 2) == 0) {
        // ~/path
        char temp[MAX_PATH_LEN];
        strcpy(temp, virtual_path + 2);

        // 规范化路径
        normalize_path(temp);

        if (strcmp(temp, "/") == 0) {
            snprintf(real_path, MAX_PATH_LEN, "%s\\home\\user", BUILDS_DIR);
        } else {
            const char *rel_path = (temp[0] == '/') ? temp + 1 : temp;
            snprintf(temp_path, MAX_PATH_LEN, "%s\\home\\user\\%s", BUILDS_DIR, rel_path);

            // 将路径中的/替换为
            for (i = 0; temp_path[i] != '\0'; i++) {
                if (temp_path[i] == '/') {
                    temp_path[i] = '\\';
                }
            }
            strcpy(real_path, temp_path);
        }
    } else {
        // 普通路径
        char normalized[MAX_PATH_LEN];
        strcpy(normalized, virtual_path);
        normalize_path(normalized);

        if (strcmp(normalized, "/") == 0) {
            strcpy(real_path, BUILDS_DIR);
        } else {
            const char *rel_path = (normalized[0] == '/') ? normalized + 1 : normalized;
            snprintf(temp_path, MAX_PATH_LEN, "%s\\%s", BUILDS_DIR, rel_path);

            // 将路径中的/替换为
            for (i = 0; temp_path[i] != '\0'; i++) {
                if (temp_path[i] == '/') {
                    temp_path[i] = '\\';
                }
            }
            strcpy(real_path, temp_path);
        }
    }
}

// 检查路径是否是目录
int is_directory(const char *path) {
    struct _finddata_t fileinfo;
    intptr_t handle = _findfirst(path, &fileinfo);

    if (handle == -1L) {
        return 0;  /* 文件不存在 */
    }

    int is_dir = (fileinfo.attrib & _A_SUBDIR) != 0;
    _findclose(handle);
    return is_dir;
}

// 检查路径是否是文件
int is_file(const char *path) {
    struct _finddata_t fileinfo;
    intptr_t handle = _findfirst(path, &fileinfo);

    if (handle == -1L) {
        return 0;  /* 文件不存在 */
    }

    int is_file = (fileinfo.attrib & _A_SUBDIR) == 0;
    _findclose(handle);
    return is_file;
}



// commands.c
// 完全重写 cd 命令
int cmd_cd(ShellContext *ctx, int argc, char *argv[], FILE *in, FILE *out) {
    (void)in;  /* 标记未使用，避免警告 */

    if (argc < 2) {
        /* cd 不带参数，默认到~（用户家目录） */
        char virtual_path[MAX_PATH_LEN];
        char real_path[MAX_PATH_LEN];

        strcpy(virtual_path, "~");
        virtual_to_real_path(virtual_path, real_path);

        if (_access(real_path, 0) == 0 && is_directory(real_path)) {
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
        virtual_to_real_path("~", real_path);

        if (_access(real_path, 0) == 0 && is_directory(real_path)) {
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
    normalize_path(new_virtual_path);

    /* 转换为真实路径检查目录是否存在 */
    char real_path[MAX_PATH_LEN];
    virtual_to_real_path(new_virtual_path, real_path);

    /* 检查目录是否存在 */
    if (_access(real_path, 0) != 0) {
        fprintf(out, "cd: 目录不存在: %s\n", input_path);
        return 1;
    }

    /* 检查是否是目录 */
    if (!is_directory(real_path)) {
        fprintf(out, "cd: 不是目录: %s\n", input_path);
        return 1;
    }

    /* 设置当前目录 */
    strcpy(ctx->current_dir, new_virtual_path);
    fprintf(out, "当前目录: %s\n", ctx->current_dir);

    return 0;
}

// cat命令 - 显示文件内容
int cmd_cat(ShellContext *ctx, int argc, char *argv[], FILE *in, FILE *out) {
    /* 声明所有变量 */
    int i;
    char real_path[MAX_PATH_LEN];
    FILE *fp;
    char buffer[MAX_CONTENT_LEN];
    size_t bytes_read;

    // 如果没有参数，从标准输入读取
    if (argc < 2) {
        if (in == stdin) {
            fprintf(out, "cat: 从标准输入读取，按Ctrl+Z结束输入...\n");
        }

        while ((bytes_read = fread(buffer, 1, MAX_CONTENT_LEN - 1, in)) > 0) {
            buffer[bytes_read] = '\0';
            fprintf(out, "%s", buffer);
        }
        return 0;
    }

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-") == 0) {
            // 从标准输入读取
            while ((bytes_read = fread(buffer, 1, MAX_CONTENT_LEN - 1, in)) > 0) {
                buffer[bytes_read] = '\0';
                fprintf(out, "%s", buffer);
            }
        } else {
            /* 构建虚拟路径 */
            char virtual_path[MAX_PATH_LEN];
            if (strcmp(ctx->current_dir, "/") == 0) {
                snprintf(virtual_path, MAX_PATH_LEN, "/%s", argv[i]);
            } else {
                snprintf(virtual_path, MAX_PATH_LEN, "%s/%s", ctx->current_dir, argv[i]);
            }

            /* 转换为真实路径 */
            virtual_to_real_path(virtual_path, real_path);

            /* 检查文件是否存在 */
            if (_access(real_path, 0) != 0) {
                fprintf(out, "cat: 文件不存在: %s\n", argv[i]);
                continue;
            }

            /* 检查是否是文件 */
            if (!is_file(real_path)) {
                fprintf(out, "cat: 不是文件: %s\n", argv[i]);
                continue;
            }

            /* 打开并显示文件内容 */
            fp = fopen(real_path, "r");
            if (fp == NULL) {
                fprintf(out, "cat: 无法打开文件: %s\n", argv[i]);
                continue;
            }

            while ((bytes_read = fread(buffer, 1, MAX_CONTENT_LEN - 1, fp)) > 0) {
                buffer[bytes_read] = '\0';
                fprintf(out, "%s", buffer);
            }

            fclose(fp);
        }
    }

    return 0;
}

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
    /* 声明所有变量 */
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
    /* 声明所有变量 */
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

// touch命令 - 创建空文件或更新时间戳
int cmd_touch(ShellContext *ctx, int argc, char *argv[], FILE *in, FILE *out) {
    /* 声明所有变量 */
    int i;
    char real_path[MAX_PATH_LEN];
    FILE *fp;

    (void)in;  /* 标记未使用，避免警告 */

    if (argc < 2) {
        fprintf(out, "touch: 缺少参数\n");
        fprintf(out, "用法: touch <文件名>\n");
        return 1;
    }

    for (i = 1; i < argc; i++) {
        /* 构建虚拟路径 */
        char virtual_path[MAX_PATH_LEN];
        if (strcmp(ctx->current_dir, "/") == 0) {
            snprintf(virtual_path, MAX_PATH_LEN, "/%s", argv[i]);
        } else {
            snprintf(virtual_path, MAX_PATH_LEN, "%s/%s", ctx->current_dir, argv[i]);
        }

        /* 转换为真实路径 */
        virtual_to_real_path(virtual_path, real_path);

        /* 检查文件是否已存在 */
        if (_access(real_path, 0) == 0) {
            /* 文件已存在，更新时间戳 */
            fp = fopen(real_path, "r+");
            if (fp != NULL) {
                fclose(fp);
                fprintf(out, "更新时间戳: %s\n", argv[i]);
            } else {
                fprintf(out, "touch: 无法更新文件: %s\n", argv[i]);
            }
        } else {
            /* 创建新文件 */
            fp = fopen(real_path, "w");
            if (fp != NULL) {
                fclose(fp);
                fprintf(out, "已创建空文件: %s\n", argv[i]);
            } else {
                fprintf(out, "touch: 无法创建文件: %s\n", argv[i]);
            }
        }
    }

    return 0;
}

// rm命令 - 删除文件
int cmd_rm(ShellContext *ctx, int argc, char *argv[], FILE *in, FILE *out) {
    /* 声明所有变量 */
    int i;
    char real_path[MAX_PATH_LEN];
    int force = 0;
    int recursive = 0;
    int start_index = 1;

    (void)in;  /* 标记未使用，避免警告 */

    /* 解析选项 */
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-f") == 0) {
            force = 1;
            start_index++;
        } else if (strcmp(argv[i], "-r") == 0) {
            recursive = 1;
            start_index++;
        } else if (argv[i][0] == '-') {
            fprintf(out, "rm: 未知选项: %s\n", argv[i]);
            fprintf(out, "用法: rm [-f] [-r] <文件>\n");
            return 1;
        } else {
            break;
        }
    }

    if (start_index >= argc) {
        fprintf(out, "rm: 缺少参数\n");
        fprintf(out, "用法: rm [-f] [-r] <文件>\n");
        return 1;
    }

    for (i = start_index; i < argc; i++) {
        /* 构建虚拟路径 */
        char virtual_path[MAX_PATH_LEN];
        if (strcmp(ctx->current_dir, "/") == 0) {
            snprintf(virtual_path, MAX_PATH_LEN, "/%s", argv[i]);
        } else {
            snprintf(virtual_path, MAX_PATH_LEN, "%s/%s", ctx->current_dir, argv[i]);
        }

        /* 转换为真实路径 */
        virtual_to_real_path(virtual_path, real_path);

        /* 检查文件是否存在 */
        if (_access(real_path, 0) != 0) {
            if (!force) {
                fprintf(out, "rm: 文件不存在: %s\n", argv[i]);
            }
            continue;
        }

        /* 检查是否是目录 */
        if (is_directory(real_path)) {
            if (recursive) {
                /* 递归删除目录 */
                char cmd[MAX_PATH_LEN + 20];
                snprintf(cmd, MAX_PATH_LEN + 20, "rmdir /s /q \"%s\"", real_path);
                int result = system(cmd);
                if (result == 0) {
                    fprintf(out, "已删除目录: %s\n", argv[i]);
                } else {
                    fprintf(out, "rm: 无法删除目录: %s\n", argv[i]);
                }
            } else {
                fprintf(out, "rm: 无法删除目录 '%s': 是目录 (使用 -r 递归删除)\n", argv[i]);
            }
        } else {
            /* 删除文件 */
            if (remove(real_path) == 0) {
                fprintf(out, "已删除文件: %s\n", argv[i]);
            } else {
                fprintf(out, "rm: 无法删除文件: %s\n", argv[i]);
            }
        }
    }

    return 0;
}

// 获取文件信息字符串
void get_file_info_string(const char *path, char *info_str, int buffer_size) {
    /* 声明所有变量 */
    struct _finddata_t fileinfo;
    intptr_t handle;
    char time_str[20];
    struct tm *tm_info;
    char size_str[20];
    char perms_str[20];

    handle = _findfirst(path, &fileinfo);
    if (handle == -1L) {
        snprintf(info_str, buffer_size, "文件不存在");
        return;
    }

    /* 格式化时间 */
    tm_info = localtime(&fileinfo.time_write);
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm_info);

    /* 格式化大小 */
    if (fileinfo.size < 1024) {
        snprintf(size_str, sizeof(size_str), "%ld 字节", fileinfo.size);
    } else if (fileinfo.size < 1024 * 1024) {
        snprintf(size_str, sizeof(size_str), "%.2f KB", fileinfo.size / 1024.0);
    } else {
        snprintf(size_str, sizeof(size_str), "%.2f MB", fileinfo.size / (1024.0 * 1024.0));
    }

    /* 格式化权限 */
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
    /* 声明所有变量 */
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
        /* 构建虚拟路径 */
        char virtual_path[MAX_PATH_LEN];
        if (strcmp(ctx->current_dir, "/") == 0) {
            snprintf(virtual_path, MAX_PATH_LEN, "/%s", argv[i]);
        } else {
            snprintf(virtual_path, MAX_PATH_LEN, "%s/%s", ctx->current_dir, argv[i]);
        }

        /* 转换为真实路径 */
        virtual_to_real_path(virtual_path, real_path);

        /* 检查文件是否存在 */
        if (_access(real_path, 0) != 0) {
            fprintf(out, "info: 文件或目录不存在: %s\n", argv[i]);
            continue;
        }

        fprintf(out, "=== 信息: %s ===\n", argv[i]);
        fprintf(out, "虚拟路径: %s\n", virtual_path);
        fprintf(out, "真实路径: %s\n", real_path);

        /* 获取详细信息 */
        get_file_info_string(real_path, info_str, sizeof(info_str));
        fprintf(out, "%s\n", info_str);

        fprintf(out, "\n");
    }

    return 0;
}

// edit命令 - 编辑文本文件
int cmd_edit(ShellContext *ctx, int argc, char *argv[], FILE *in, FILE *out) {
    /* 声明所有变量 */
    int i;
    char real_path[MAX_PATH_LEN];
    char temp_path[MAX_PATH_LEN];
    char edit_cmd[MAX_PATH_LEN + 50];
    FILE *fp;

    (void)in;  /* 标记未使用，避免警告 */

    if (argc < 2) {
        fprintf(out, "edit: 缺少参数\n");
        fprintf(out, "用法: edit <文件名>\n");
        return 1;
    }

    for (i = 1; i < argc; i++) {
        /* 构建虚拟路径 */
        char virtual_path[MAX_PATH_LEN];
        if (strcmp(ctx->current_dir, "/") == 0) {
            snprintf(virtual_path, MAX_PATH_LEN, "/%s", argv[i]);
        } else {
            snprintf(virtual_path, MAX_PATH_LEN, "%s/%s", ctx->current_dir, argv[i]);
        }

        /* 转换为真实路径 */
        virtual_to_real_path(virtual_path, real_path);

        /* 检查文件扩展名，如果不是.txt则添加.txt */
        char *ext = strrchr(real_path, '.');
        if (ext == NULL || (strcmp(ext, ".txt") != 0 && strcmp(ext, ".log") != 0)) {
            snprintf(temp_path, MAX_PATH_LEN, "%s.txt", real_path);
        } else {
            strcpy(temp_path, real_path);
        }

        /* 如果文件不存在，创建空文件 */
        if (_access(temp_path, 0) != 0) {
            fp = fopen(temp_path, "w");
            if (fp != NULL) {
                fclose(fp);
                fprintf(out, "创建新文件: %s\n", argv[i]);
            } else {
                fprintf(out, "edit: 无法创建文件: %s\n", argv[i]);
                continue;
            }
        }

        /* 使用记事本打开文件 */
        fprintf(out, "正在打开文件: %s\n", temp_path);

        #ifdef _WIN32
            snprintf(edit_cmd, MAX_PATH_LEN + 50, "notepad \"%s\"", temp_path);
        #else
            snprintf(edit_cmd, MAX_PATH_LEN + 50, "xdg-open \"%s\"", temp_path);
        #endif

        int result = system(edit_cmd);

        if (result == 0) {
            fprintf(out, "文件已保存: %s\n", argv[i]);
        } else {
            fprintf(out, "警告: 编辑器返回非零状态: %d\n", result);
        }
    }

    return 0;
}

// echo命令 - 输出文本
int cmd_echo(ShellContext *ctx, int argc, char *argv[], FILE *in, FILE *out) {
    int i;
    int newline = 1;

    (void)ctx;  /* 标记未使用，避免警告 */
    (void)in;   /* 标记未使用，避免警告 */

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-n") == 0) {
            newline = 0;
        } else {
            fprintf(out, "%s", argv[i]);
            if (i < argc - 1) {
                fprintf(out, " ");
            }
        }
    }

    if (newline) {
        fprintf(out, "\n");
    }

    return 0;
}

// grep命令 - 搜索文本
int cmd_grep(ShellContext *ctx, int argc, char *argv[], FILE *in, FILE *out) {
    int i;
    char pattern[MAX_FILENAME] = "";
    int ignore_case = 0;
    int line_number = 0;
    int count_only = 0;

    (void)ctx;  /* 标记未使用，避免警告 */

    if (argc < 2) {
        fprintf(out, "grep: 缺少参数\n");
        fprintf(out, "用法: grep [选项] <模式> [文件...]\n");
        return 1;
    }

    // 解析选项
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-i") == 0) {
            ignore_case = 1;
        } else if (strcmp(argv[i], "-n") == 0) {
            line_number = 1;
        } else if (strcmp(argv[i], "-c") == 0) {
            count_only = 1;
        } else if (argv[i][0] != '-') {
            strcpy(pattern, argv[i]);
            break;
        }
    }

    if (strlen(pattern) == 0) {
        fprintf(out, "grep: 需要搜索模式\n");
        return 1;
    }

    char lower_pattern[MAX_FILENAME];
    if (ignore_case) {
        for (i = 0; pattern[i]; i++) {
            lower_pattern[i] = tolower(pattern[i]);
        }
        lower_pattern[i] = '\0';
    }

    int total_matches = 0;

    // 如果没有文件名，从标准输入读取
    if (i >= argc) {
        char line[MAX_CONTENT_LEN];
        int line_num = 0;

        while (fgets(line, sizeof(line), in) != NULL) {
            line_num++;
            char *match = NULL;

            if (ignore_case) {
                char lower_line[MAX_CONTENT_LEN];
                for (int j = 0; line[j]; j++) {
                    lower_line[j] = tolower(line[j]);
                }
                match = strstr(lower_line, lower_pattern);
            } else {
                match = strstr(line, pattern);
            }

            if (match) {
                total_matches++;
                if (!count_only) {
                    if (line_number) {
                        fprintf(out, "%d:", line_num);
                    }
                    fprintf(out, "%s", line);
                }
            }
        }
    } else {
        // 从文件读取
        for (; i < argc; i++) {
            char real_path[MAX_PATH_LEN];
            char virtual_path[MAX_PATH_LEN];

            /* 构建虚拟路径 */
            if (strcmp(ctx->current_dir, "/") == 0) {
                snprintf(virtual_path, MAX_PATH_LEN, "/%s", argv[i]);
            } else {
                snprintf(virtual_path, MAX_PATH_LEN, "%s/%s", ctx->current_dir, argv[i]);
            }

            /* 转换为真实路径 */
            virtual_to_real_path(virtual_path, real_path);

            /* 检查文件是否存在 */
            if (_access(real_path, 0) != 0) {
                fprintf(out, "grep: 文件不存在: %s\n", argv[i]);
                continue;
            }

            FILE *fp = fopen(real_path, "r");
            if (!fp) {
                fprintf(out, "grep: 无法打开文件: %s\n", argv[i]);
                continue;
            }

            char line[MAX_CONTENT_LEN];
            int line_num = 0;
            int file_matches = 0;

            while (fgets(line, sizeof(line), fp) != NULL) {
                line_num++;
                char *match = NULL;

                if (ignore_case) {
                    char lower_line[MAX_CONTENT_LEN];
                    for (int j = 0; line[j]; j++) {
                        lower_line[j] = tolower(line[j]);
                    }
                    match = strstr(lower_line, lower_pattern);
                } else {
                    match = strstr(line, pattern);
                }

                if (match) {
                    total_matches++;
                    file_matches++;
                    if (!count_only) {
                        if (line_number) {
                            fprintf(out, "%d:", line_num);
                        }
                        fprintf(out, "%s", line);
                    }
                }
            }

            fclose(fp);

            if (count_only) {
                fprintf(out, "%s: %d\n", argv[i], file_matches);
            }
        }
    }

    if (count_only && i < argc) {
        fprintf(out, "总计: %d\n", total_matches);
    }

    return 0;
}

// wc命令 - 统计字数
int cmd_wc(ShellContext *ctx, int argc, char *argv[], FILE *in, FILE *out) {
    int i;
    int count_lines = 1;
    int count_words = 1;
    int count_chars = 1;
    int count_bytes = 1;

    (void)ctx;  /* 标记未使用，避免警告 */

    // 解析选项
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-l") == 0) {
            count_words = 0;
            count_chars = 0;
            count_bytes = 0;
        } else if (strcmp(argv[i], "-w") == 0) {
            count_lines = 0;
            count_chars = 0;
            count_bytes = 0;
        } else if (strcmp(argv[i], "-c") == 0) {
            count_lines = 0;
            count_words = 0;
            count_chars = 0;
        } else if (strcmp(argv[i], "-m") == 0) {
            count_lines = 0;
            count_words = 0;
            count_bytes = 0;
        } else if (argv[i][0] == '-') {
            // 无效选项
        } else {
            break;
        }
    }

    int total_lines = 0;
    int total_words = 0;
    int total_chars = 0;
    int total_bytes = 0;

    // 如果没有文件名，从标准输入读取
    if (i >= argc) {
        char line[MAX_CONTENT_LEN];

        while (fgets(line, sizeof(line), in) != NULL) {
            total_lines++;
            total_bytes += strlen(line);

            // 统计字符
            for (int j = 0; line[j]; j++) {
                total_chars++;
            }

            // 统计单词
            int in_word = 0;
            for (int j = 0; line[j]; j++) {
                if (isspace(line[j])) {
                    in_word = 0;
                } else if (!in_word) {
                    in_word = 1;
                    total_words++;
                }
            }
        }

        if (count_lines) fprintf(out, "%d ", total_lines);
        if (count_words) fprintf(out, "%d ", total_words);
        if (count_chars) fprintf(out, "%d ", total_chars);
        if (count_bytes) fprintf(out, "%d ", total_bytes);
        fprintf(out, "\n");
    } else {
        // 从文件读取
        for (; i < argc; i++) {
            char real_path[MAX_PATH_LEN];
            char virtual_path[MAX_PATH_LEN];

            /* 构建虚拟路径 */
            if (strcmp(ctx->current_dir, "/") == 0) {
                snprintf(virtual_path, MAX_PATH_LEN, "/%s", argv[i]);
            } else {
                snprintf(virtual_path, MAX_PATH_LEN, "%s/%s", ctx->current_dir, argv[i]);
            }

            /* 转换为真实路径 */
            virtual_to_real_path(virtual_path, real_path);

            /* 检查文件是否存在 */
            if (_access(real_path, 0) != 0) {
                fprintf(out, "wc: 文件不存在: %s\n", argv[i]);
                continue;
            }

            FILE *fp = fopen(real_path, "r");
            if (!fp) {
                fprintf(out, "wc: 无法打开文件: %s\n", argv[i]);
                continue;
            }

            int file_lines = 0;
            int file_words = 0;
            int file_chars = 0;
            int file_bytes = 0;
            char line[MAX_CONTENT_LEN];

            while (fgets(line, sizeof(line), fp) != NULL) {
                file_lines++;
                file_bytes += strlen(line);

                // 统计字符
                for (int j = 0; line[j]; j++) {
                    file_chars++;
                }

                // 统计单词
                int in_word = 0;
                for (int j = 0; line[j]; j++) {
                    if (isspace(line[j])) {
                        in_word = 0;
                    } else if (!in_word) {
                        in_word = 1;
                        file_words++;
                    }
                }
            }

            fclose(fp);

            if (count_lines) fprintf(out, "%d ", file_lines);
            if (count_words) fprintf(out, "%d ", file_words);
            if (count_chars) fprintf(out, "%d ", file_chars);
            if (count_bytes) fprintf(out, "%d ", file_bytes);
            fprintf(out, "%s\n", argv[i]);

            total_lines += file_lines;
            total_words += file_words;
            total_chars += file_chars;
            total_bytes += file_bytes;
        }

        if (argc - i > 1) {
            if (count_lines) fprintf(out, "%d ", total_lines);
            if (count_words) fprintf(out, "%d ", total_words);
            if (count_chars) fprintf(out, "%d ", total_chars);
            if (count_bytes) fprintf(out, "%d ", total_bytes);
            fprintf(out, "总计\n");
        }
    }

    return 0;
}
// commands.c
// 重写 help 命令
int cmd_help(ShellContext *ctx, int argc, char *argv[], FILE *in, FILE *out) {
    (void)ctx;  /* 标记未使用，避免警告 */
    (void)in;   /* 标记未使用，避免警告 */

    // 定义命令信息结构
    typedef struct {
        const char *name;
        const char *brief;
        const char *detail;
    } CommandInfo;

    // 命令信息表
    CommandInfo commands[] = {
        {
            "cd",
            "切换当前工作目录",
            "用法: cd [目录]\n"
            "  切换到指定目录。支持以下格式：\n"
            "  cd /         切换到根目录\n"
            "  cd ~         切换到用户家目录\n"
            "  cd ..        切换到上级目录\n"
            "  cd .         保持当前目录\n"
            "  cd 目录名    切换到当前目录下的子目录\n"
            "  cd /usr/bin  切换到绝对路径\n"
            "  cd ~/Desktop 切换到用户家目录下的Desktop\n"
        },
        {
            "ls",
            "列出目录内容",
            "用法: ls [-a] [路径]\n"
            "  列出指定目录的内容。\n"
            "  选项:\n"
            "    -a  显示所有文件，包括隐藏文件（以.开头的文件）\n"
            "  示例:\n"
            "    ls          列出当前目录内容\n"
            "    ls -a       列出当前目录所有文件\n"
            "    ls /usr     列出/usr目录内容\n"
            "    ls ~        列出用户家目录内容\n"
        },
        {
            "ll",
            "详细列出目录内容",
            "用法: ll [-a] [路径]\n"
            "  详细列出指定目录的内容，包括文件大小、修改时间等信息。\n"
            "  选项:\n"
            "    -a  显示所有文件，包括隐藏文件\n"
            "  输出格式:\n"
            "    类型  大小  修改时间      名称\n"
            "    目录  -     2024-12-28  bin\n"
            "    文件  1024  2024-12-28  file.txt\n"
        },
        {
            "pwd",
            "显示当前工作目录",
            "用法: pwd\n"
            "  显示当前所在的虚拟目录路径。\n"
            "  示例:\n"
            "    pwd\n"
            "    /home/user\n"
        },
        {
            "mkdir",
            "创建新目录",
            "用法: mkdir <目录名> [目录名2 ...]\n"
            "  创建一个或多个新目录。\n"
            "  示例:\n"
            "    mkdir newdir      创建名为newdir的目录\n"
            "    mkdir dir1 dir2   同时创建dir1和dir2目录\n"
        },
        {
            "rmdir",
            "删除空目录",
            "用法: rmdir <目录名> [目录名2 ...]\n"
            "  删除一个或多个空目录。目录必须为空才能删除。\n"
            "  示例:\n"
            "    rmdir emptydir    删除名为emptydir的空目录\n"
        },
        {
            "cp",
            "复制文件或目录",
            "用法: cp [选项] <源> <目标>\n"
            "  用法: cp [选项] <源1> <源2> ... <目标目录>\n"
            "  复制文件或目录。\n"
            "  选项:\n"
            "    -r, -R  递归复制目录\n"
            "    -f      强制覆盖目标文件\n"
            "  示例:\n"
            "    cp file.txt backup.txt      复制文件\n"
            "    cp -r dir1 dir2             复制目录\n"
            "    cp file1.txt file2.txt dir/ 复制多个文件到目录\n"
        },
        {
            "mv",
            "移动/重命名文件或目录",
            "用法: mv [选项] <源> <目标>\n"
            "  用法: mv [选项] <源1> <源2> ... <目标目录>\n"
            "  移动或重命名文件或目录。\n"
            "  选项:\n"
            "    -f  强制覆盖目标文件\n"
            "  示例:\n"
            "    mv old.txt new.txt       重命名文件\n"
            "    mv file.txt dir/         移动文件到目录\n"
            "    mv dir1 dir2             重命名目录\n"
        },
        {
            "cat",
            "显示文件内容",
            "用法: cat [文件] [文件2 ...]\n"
            "  显示一个或多个文件的内容。如果没有指定文件，从标准输入读取。\n"
            "  示例:\n"
            "    cat file.txt      显示文件内容\n"
            "    cat file1.txt file2.txt 显示多个文件内容\n"
            "    cat               从键盘输入内容，按Ctrl+Z结束\n"
        },
        {
            "touch",
            "创建空文件或更新时间戳",
            "用法: touch <文件名> [文件名2 ...]\n"
            "  创建一个或多个空文件，或更新现有文件的时间戳。\n"
            "  示例:\n"
            "    touch newfile.txt     创建新文件\n"
            "    touch file1.txt file2.txt 创建多个文件\n"
            "    touch existing.txt    更新文件时间戳\n"
        },
        {
            "rm",
            "删除文件或目录",
            "用法: rm [选项] <文件/目录> [文件2/目录2 ...]\n"
            "  删除文件或目录。\n"
            "  选项:\n"
            "    -f  强制删除，不提示\n"
            "    -r  递归删除目录\n"
            "  示例:\n"
            "    rm file.txt          删除文件\n"
            "    rm -f old.txt        强制删除文件\n"
            "    rm -r dir            递归删除目录\n"
        },
        {
            "info",
            "显示文件或目录信息",
            "用法: info <文件/目录> [文件2/目录2 ...]\n"
            "  显示文件或目录的详细信息，包括大小、修改时间、权限等。\n"
            "  示例:\n"
            "    info file.txt      显示文件信息\n"
            "    info dir/          显示目录信息\n"
        },
        {
            "edit",
            "编辑文本文件",
            "用法: edit <文件名> [文件名2 ...]\n"
            "  使用记事本打开文本文件进行编辑。如果文件不存在，会创建新文件。\n"
            "  示例:\n"
            "    edit file.txt      编辑文件\n"
            "    edit newfile.txt   创建并编辑新文件\n"
        },
        {
            "echo",
            "输出文本",
            "用法: echo [选项] [文本]\n"
            "  输出指定的文本。\n"
            "  选项:\n"
            "    -n  不输出结尾的换行符\n"
            "  示例:\n"
            "    echo Hello World         输出 Hello World\n"
            "    echo -n \"Hello \"        输出 Hello （不换行）\n"
        },
        {
            "grep",
            "搜索文本",
            "用法: grep [选项] <模式> [文件] [文件2 ...]\n"
            "  在文件中搜索指定的模式。如果没有指定文件，从标准输入读取。\n"
            "  选项:\n"
            "    -i  忽略大小写\n"
            "    -n  显示行号\n"
            "    -c  只统计匹配行数\n"
            "  示例:\n"
            "    grep \"text\" file.txt        搜索包含text的行\n"
            "    grep -i \"TEXT\" file.txt    忽略大小写搜索\n"
            "    grep -n \"text\" file.txt    显示行号\n"
        },
        {
            "wc",
            "统计字数",
            "用法: wc [选项] [文件] [文件2 ...]\n"
            "  统计文件的行数、单词数、字符数或字节数。如果没有指定文件，从标准输入读取。\n"
            "  选项:\n"
            "    -l  只统计行数\n"
            "    -w  只统计单词数\n"
            "    -c  只统计字节数\n"
            "    -m  只统计字符数\n"
            "  默认情况下统计所有四项。\n"
            "  示例:\n"
            "    wc file.txt           统计文件信息\n"
            "    wc -l file.txt        只统计行数\n"
            "    wc -w file.txt        只统计单词数\n"
        },
        {
            "help",
            "显示帮助信息",
            "用法: help [命令名]\n"
            "  显示所有可用命令的简要介绍，或指定命令的详细信息。\n"
            "  示例:\n"
            "    help          显示所有命令\n"
            "    help cd       显示cd命令的详细信息\n"
            "    help ls       显示ls命令的详细信息\n"
        },
        {
            "exit",
            "退出Shell",
            "用法: exit\n"
            "  退出LGD-OS Shell程序。\n"
            "  示例:\n"
            "    exit          退出程序\n"
        },
        {NULL, NULL, NULL}  // 结束标记
    };

    int command_count = sizeof(commands) / sizeof(commands[0]) - 1;  // 减去结束标记

    if (argc == 1) {
        // 显示所有命令的简要介绍
        fprintf(out, "\n");
        fprintf(out, "╔══════════════════════════════════════════════════════════════════╗\n");
        fprintf(out, "║                    LGD-OS Shell 命令帮助                         ║\n");
        fprintf(out, "║                    版本: %s                      ║\n", LGD_OS_VERSION);
        fprintf(out, "╚══════════════════════════════════════════════════════════════════╚\n");
        fprintf(out, "\n");
        fprintf(out, "════════════════════════════════════════════════════════════════════\n");
        fprintf(out, " 输入 'help 命令名' 获取指定命令的详细信息\n");
        fprintf(out, "════════════════════════════════════════════════════════════════════\n");
        fprintf(out, "\n");

        // 计算最大命令名称长度
        int max_name_len = 0;
        for (int i = 0; i < command_count; i++) {
            int len = strlen(commands[i].name);
            if (len > max_name_len) {
                max_name_len = len;
            }
        }

        // 显示所有命令
        fprintf(out, "可用命令 (%d 个):\n\n", command_count);

        for (int i = 0; i < command_count; i++) {
            fprintf(out, "  %-*s  - %s\n", max_name_len, commands[i].name, commands[i].brief);

            // 每4个命令后换行分组
            if ((i + 1) % 4 == 0) {
                fprintf(out, "\n");
            }
        }

        fprintf(out, "\n");
        fprintf(out, "════════════════════════════════════════════════════════════════════\n");
        fprintf(out, "管道功能: 使用 | 连接多个命令\n");
        fprintf(out, "  示例: ls | grep txt | wc -l\n");
        fprintf(out, "\n");
        fprintf(out, "路径支持:\n");
        fprintf(out, "  /          - 根目录\n");
        fprintf(out, "  ~          - 用户家目录 (/home/user)\n");
        fprintf(out, "  ~/path     - 用户家目录下的路径\n");
        fprintf(out, "  .          - 当前目录\n");
        fprintf(out, "  ..         - 上级目录\n");
        fprintf(out, "  /path      - 绝对路径\n");
        fprintf(out, "  path       - 相对路径（相对于当前目录）\n");
        fprintf(out, "  \"path\"     - 带空格或特殊字符的路径（用引号括起来）\n");
        fprintf(out, "\n");
        fprintf(out, "所有操作都在虚拟文件系统中进行，实际文件保存在 builds\\ 目录下。\n");
        fprintf(out, "════════════════════════════════════════════════════════════════════\n");
        fprintf(out, "\n");

    } else if (argc == 2) {
        // 显示指定命令的详细信息
        char *cmd_name = argv[1];
        int found = 0;

        for (int i = 0; i < command_count; i++) {
            if (strcmp(commands[i].name, cmd_name) == 0) {
                found = 1;

                fprintf(out, "\n");
                fprintf(out, "════════════════════════════════════════════════════════════════════\n");
                fprintf(out, "命令: %s\n", commands[i].name);
                fprintf(out, "════════════════════════════════════════════════════════════════════\n");
                fprintf(out, "\n");
                fprintf(out, "简介: %s\n", commands[i].brief);
                fprintf(out, "\n");
                fprintf(out, "详细信息:\n");
                fprintf(out, "%s\n", commands[i].detail);
                fprintf(out, "\n");
                fprintf(out, "════════════════════════════════════════════════════════════════════\n");

                break;
            }
        }

        if (!found) {
            fprintf(out, "错误: 未知命令 '%s'\n", cmd_name);
            fprintf(out, "输入 'help' 查看所有可用命令。\n");
            return 1;
        }
    } else {
        fprintf(out, "用法: help [命令名]\n");
        fprintf(out, "  显示所有可用命令的简要介绍，或指定命令的详细信息。\n");
        fprintf(out, "  示例:\n");
        fprintf(out, "    help          显示所有命令\n");
        fprintf(out, "    help cd       显示cd命令的详细信息\n");
        return 1;
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
// commands.c
// 在文件末尾，help命令之前添加以下代码

// 辅助函数：移除字符串两端的引号
void remove_quotes(char *str) {
    int len = strlen(str);

    if (len >= 2) {
        if ((str[0] == '"' && str[len-1] == '"') ||
            (str[0] == '\'' && str[len-1] == '\'')) {
            // 移动字符串内容，去除引号
            for (int i = 0; i < len - 2; i++) {
                str[i] = str[i+1];
            }
            str[len-2] = '\0';
        }
    }
}

// 辅助函数：复制目录（递归）
int copy_directory(const char *src, const char *dest) {
    char src_path[MAX_PATH_LEN];
    char dest_path[MAX_PATH_LEN];
    char search_pattern[MAX_PATH_LEN];

    // 创建目标目录
    if (_mkdir(dest) != 0) {
        // 如果目录已存在，忽略错误
        if (errno != EEXIST) {
            return 0;
        }
    }

    // 构建搜索模式
    snprintf(search_pattern, MAX_PATH_LEN, "%s\\*.*", src);

    // 遍历源目录
    struct _finddata_t fileinfo;
    intptr_t handle = _findfirst(search_pattern, &fileinfo);

    if (handle == -1L) {
        return 1;  // 目录为空
    }

    do {
        // 跳过当前目录和上级目录
        if (strcmp(fileinfo.name, ".") == 0 || strcmp(fileinfo.name, "..") == 0) {
            continue;
        }

        // 构建源文件路径和目标文件路径
        snprintf(src_path, MAX_PATH_LEN, "%s\\%s", src, fileinfo.name);
        snprintf(dest_path, MAX_PATH_LEN, "%s\\%s", dest, fileinfo.name);

        if (fileinfo.attrib & _A_SUBDIR) {
            // 如果是子目录，递归复制
            copy_directory(src_path, dest_path);
        } else {
            // 如果是文件，复制文件
            CopyFileA(src_path, dest_path, FALSE);
        }
    } while (_findnext(handle, &fileinfo) == 0);

    _findclose(handle);
    return 1;
}

// cp命令 - 复制文件或目录
int cmd_cp(ShellContext *ctx, int argc, char *argv[], FILE *in, FILE *out) {
    /* 声明所有变量 */
    int i;
    int recursive = 0;
    int force = 0;
    int start_index = 1;

    (void)in;  /* 标记未使用，避免警告 */

    if (argc < 3) {
        fprintf(out, "cp: 缺少参数\n");
        fprintf(out, "用法: cp [选项] <源> <目标>\n");
        fprintf(out, "      cp [选项] <源1> <源2> ... <目标目录>\n");
        return 1;
    }

    /* 解析选项 */
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-r") == 0 || strcmp(argv[i], "-R") == 0) {
            recursive = 1;
            start_index++;
        } else if (strcmp(argv[i], "-f") == 0) {
            force = 1;
            start_index++;
        } else if (strcmp(argv[i], "-i") == 0) {
            /* 交互模式暂不支持 */
            start_index++;
        } else if (argv[i][0] == '-') {
            fprintf(out, "cp: 未知选项: %s\n", argv[i]);
            fprintf(out, "用法: cp [-r] [-f] <源> <目标>\n");
            return 1;
        } else {
            break;
        }
    }

    if (argc - start_index < 2) {
        fprintf(out, "cp: 缺少参数\n");
        fprintf(out, "用法: cp [选项] <源> <目标>\n");
        return 1;
    }

    int source_count = argc - start_index - 1;
    int is_multiple_sources = (source_count > 1);
    int result = 0;

    for (i = start_index; i < argc; i++) {
        /* 如果是最后一个参数，是目标路径 */
        if (i == argc - 1) {
            break;
        }

        /* 获取源路径（移除引号） */
        char source_arg[MAX_PATH_LEN];
        strcpy(source_arg, argv[i]);
        remove_quotes(source_arg);

        /* 构建源虚拟路径 */
        char source_virtual[MAX_PATH_LEN];
        if (strcmp(ctx->current_dir, "/") == 0) {
            snprintf(source_virtual, MAX_PATH_LEN, "/%s", source_arg);
        } else {
            snprintf(source_virtual, MAX_PATH_LEN, "%s/%s", ctx->current_dir, source_arg);
        }

        /* 获取目标路径（移除引号） */
        char dest_arg[MAX_PATH_LEN];
        strcpy(dest_arg, argv[argc - 1]);
        remove_quotes(dest_arg);

        /* 构建目标虚拟路径 */
        char dest_virtual[MAX_PATH_LEN];
        if (strcmp(ctx->current_dir, "/") == 0) {
            snprintf(dest_virtual, MAX_PATH_LEN, "/%s", dest_arg);
        } else {
            snprintf(dest_virtual, MAX_PATH_LEN, "%s/%s", ctx->current_dir, dest_arg);
        }

        /* 转换为真实路径 */
        char source_real[MAX_PATH_LEN];
        char dest_real[MAX_PATH_LEN];
        virtual_to_real_path(source_virtual, source_real);
        virtual_to_real_path(dest_virtual, dest_real);

        /* 检查源文件是否存在 */
        if (_access(source_real, 0) != 0) {
            fprintf(out, "cp: 源文件不存在: %s\n", argv[i]);
            result = 1;
            continue;
        }

        /* 检查是否是目录 */
        int source_is_dir = is_directory(source_real);

        /* 如果是多个源文件，目标必须是目录 */
        if (is_multiple_sources && !source_is_dir) {
            /* 检查目标是否存在且是目录 */
            if (_access(dest_real, 0) == 0) {
                if (!is_directory(dest_real)) {
                    fprintf(out, "cp: 目标不是目录: %s\n", argv[argc - 1]);
                    result = 1;
                    continue;
                }
            } else {
                /* 目标不存在，创建目录 */
                if (_mkdir(dest_real) != 0) {
                    fprintf(out, "cp: 无法创建目录: %s\n", argv[argc - 1]);
                    result = 1;
                    continue;
                }
            }

            /* 构建目标文件路径 */
            char final_dest[MAX_PATH_LEN];
            char *filename = strrchr(source_real, '\\');
            if (filename) {
                filename++;  // 跳过反斜杠
            } else {
                filename = source_arg;
            }
            snprintf(final_dest, MAX_PATH_LEN, "%s\\%s", dest_real, filename);

            /* 复制文件 */
            if (CopyFileA(source_real, final_dest, !force) == 0) {
                fprintf(out, "cp: 无法复制文件 '%s' 到 '%s\\%s'\n",
                        argv[i], argv[argc - 1], filename);
                result = 1;
            } else {
                fprintf(out, "已复制: %s -> %s/%s\n",
                        argv[i], argv[argc - 1], filename);
            }
        } else {
            /* 单个源文件 */
            if (source_is_dir) {
                if (recursive) {
                    /* 递归复制目录 */
                    if (copy_directory(source_real, dest_real)) {
                        fprintf(out, "已复制目录: %s -> %s\n",
                                argv[i], argv[argc - 1]);
                    } else {
                        fprintf(out, "cp: 无法复制目录: %s\n", argv[i]);
                        result = 1;
                    }
                } else {
                    fprintf(out, "cp: 略过目录 '%s'\n", argv[i]);
                    fprintf(out, "提示: 使用 -r 或 -R 递归复制目录\n");
                    result = 1;
                }
            } else {
                /* 检查目标是否是目录 */
                int dest_is_dir = 0;
                if (_access(dest_real, 0) == 0) {
                    dest_is_dir = is_directory(dest_real);
                }

                if (dest_is_dir) {
                    /* 目标存在且是目录，复制到目录中 */
                    char *filename = strrchr(source_real, '\\');
                    if (filename) {
                        filename++;  // 跳过反斜杠
                    } else {
                        filename = source_arg;
                    }

                    char final_dest[MAX_PATH_LEN];
                    snprintf(final_dest, MAX_PATH_LEN, "%s\\%s", dest_real, filename);

                    if (CopyFileA(source_real, final_dest, !force) == 0) {
                        fprintf(out, "cp: 无法复制文件 '%s' 到 '%s\\%s'\n",
                                argv[i], argv[argc - 1], filename);
                        result = 1;
                    } else {
                        fprintf(out, "已复制: %s -> %s/%s\n",
                                argv[i], argv[argc - 1], filename);
                    }
                } else {
                    /* 目标不是目录，直接复制/重命名 */
                    if (CopyFileA(source_real, dest_real, !force) == 0) {
                        fprintf(out, "cp: 无法复制文件 '%s' 到 '%s'\n",
                                argv[i], argv[argc - 1]);
                        result = 1;
                    } else {
                        fprintf(out, "已复制: %s -> %s\n",
                                argv[i], argv[argc - 1]);
                    }
                }
            }
        }
    }

    return result;
}

// mv命令 - 移动/重命名文件或目录
int cmd_mv(ShellContext *ctx, int argc, char *argv[], FILE *in, FILE *out) {
    /* 声明所有变量 */
    int i;
    int force = 0;
    int start_index = 1;

    (void)in;  /* 标记未使用，避免警告 */

    if (argc < 3) {
        fprintf(out, "mv: 缺少参数\n");
        fprintf(out, "用法: mv [选项] <源> <目标>\n");
        fprintf(out, "      mv [选项] <源1> <源2> ... <目标目录>\n");
        return 1;
    }

    /* 解析选项 */
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-f") == 0) {
            force = 1;
            start_index++;
        } else if (strcmp(argv[i], "-i") == 0) {
            /* 交互模式暂不支持 */
            start_index++;
        } else if (argv[i][0] == '-') {
            fprintf(out, "mv: 未知选项: %s\n", argv[i]);
            fprintf(out, "用法: mv [-f] <源> <目标>\n");
            return 1;
        } else {
            break;
        }
    }

    if (argc - start_index < 2) {
        fprintf(out, "mv: 缺少参数\n");
        fprintf(out, "用法: mv [选项] <源> <目标>\n");
        return 1;
    }

    int source_count = argc - start_index - 1;
    int is_multiple_sources = (source_count > 1);
    int result = 0;

    for (i = start_index; i < argc; i++) {
        /* 如果是最后一个参数，是目标路径 */
        if (i == argc - 1) {
            break;
        }

        /* 获取源路径（移除引号） */
        char source_arg[MAX_PATH_LEN];
        strcpy(source_arg, argv[i]);
        remove_quotes(source_arg);

        /* 构建源虚拟路径 */
        char source_virtual[MAX_PATH_LEN];
        if (strcmp(ctx->current_dir, "/") == 0) {
            snprintf(source_virtual, MAX_PATH_LEN, "/%s", source_arg);
        } else {
            snprintf(source_virtual, MAX_PATH_LEN, "%s/%s", ctx->current_dir, source_arg);
        }

        /* 获取目标路径（移除引号） */
        char dest_arg[MAX_PATH_LEN];
        strcpy(dest_arg, argv[argc - 1]);
        remove_quotes(dest_arg);

        /* 构建目标虚拟路径 */
        char dest_virtual[MAX_PATH_LEN];
        if (strcmp(ctx->current_dir, "/") == 0) {
            snprintf(dest_virtual, MAX_PATH_LEN, "/%s", dest_arg);
        } else {
            snprintf(dest_virtual, MAX_PATH_LEN, "%s/%s", ctx->current_dir, dest_arg);
        }

        /* 转换为真实路径 */
        char source_real[MAX_PATH_LEN];
        char dest_real[MAX_PATH_LEN];
        virtual_to_real_path(source_virtual, source_real);
        virtual_to_real_path(dest_virtual, dest_real);

        /* 检查源文件是否存在 */
        if (_access(source_real, 0) != 0) {
            fprintf(out, "mv: 源文件不存在: %s\n", argv[i]);
            result = 1;
            continue;
        }

        /* 检查是否是目录 */
        int source_is_dir = is_directory(source_real);

        /* 如果是多个源文件，目标必须是目录 */
        if (is_multiple_sources) {
            /* 检查目标是否存在且是目录 */
            int dest_is_dir = 0;
            if (_access(dest_real, 0) == 0) {
                dest_is_dir = is_directory(dest_real);
            }

            if (!dest_is_dir) {
                fprintf(out, "mv: 目标不是目录: %s\n", argv[argc - 1]);
                result = 1;
                continue;
            }

            /* 构建目标文件路径 */
            char final_dest[MAX_PATH_LEN];
            char *filename = strrchr(source_real, '\\');
            if (filename) {
                filename++;  // 跳过反斜杠
            } else {
                filename = source_arg;
            }
            snprintf(final_dest, MAX_PATH_LEN, "%s\\%s", dest_real, filename);

            /* 移动文件 */
            if (MoveFileA(source_real, final_dest) == 0) {
                if (force) {
                    /* 强制移动：先删除目标文件 */
                    remove(final_dest);
                    if (MoveFileA(source_real, final_dest) == 0) {
                        fprintf(out, "mv: 无法移动文件 '%s' 到 '%s\\%s'\n",
                                argv[i], argv[argc - 1], filename);
                        result = 1;
                    } else {
                        fprintf(out, "已移动: %s -> %s/%s\n",
                                argv[i], argv[argc - 1], filename);
                    }
                } else {
                    fprintf(out, "mv: 无法移动文件 '%s' 到 '%s\\%s'\n",
                            argv[i], argv[argc - 1], filename);
                    result = 1;
                }
            } else {
                fprintf(out, "已移动: %s -> %s/%s\n",
                        argv[i], argv[argc - 1], filename);
            }
        } else {
            /* 单个源文件 */
            /* 检查目标是否是目录 */
            int dest_is_dir = 0;
            if (_access(dest_real, 0) == 0) {
                dest_is_dir = is_directory(dest_real);
            }

            if (dest_is_dir) {
                /* 目标存在且是目录，移动到目录中 */
                char *filename = strrchr(source_real, '\\');
                if (filename) {
                    filename++;  // 跳过反斜杠
                } else {
                    filename = source_arg;
                }

                char final_dest[MAX_PATH_LEN];
                snprintf(final_dest, MAX_PATH_LEN, "%s\\%s", dest_real, filename);

                if (MoveFileA(source_real, final_dest) == 0) {
                    if (force) {
                        /* 强制移动：先删除目标文件 */
                        remove(final_dest);
                        if (MoveFileA(source_real, final_dest) == 0) {
                            fprintf(out, "mv: 无法移动文件 '%s' 到 '%s\\%s'\n",
                                    argv[i], argv[argc - 1], filename);
                            result = 1;
                        } else {
                            fprintf(out, "已移动: %s -> %s/%s\n",
                                    argv[i], argv[argc - 1], filename);
                        }
                    } else {
                        fprintf(out, "mv: 无法移动文件 '%s' 到 '%s\\%s'\n",
                                argv[i], argv[argc - 1], filename);
                        result = 1;
                    }
                } else {
                    fprintf(out, "已移动: %s -> %s/%s\n",
                            argv[i], argv[argc - 1], filename);
                }
            } else {
                /* 目标不是目录，直接移动/重命名 */
                if (MoveFileA(source_real, dest_real) == 0) {
                    if (force) {
                        /* 强制移动：先删除目标文件 */
                        remove(dest_real);
                        if (MoveFileA(source_real, dest_real) == 0) {
                            fprintf(out, "mv: 无法移动文件 '%s' 到 '%s'\n",
                                    argv[i], argv[argc - 1]);
                            result = 1;
                        } else {
                            fprintf(out, "已移动: %s -> %s\n",
                                    argv[i], argv[argc - 1]);
                        }
                    } else {
                        fprintf(out, "mv: 无法移动文件 '%s' 到 '%s'\n",
                                argv[i], argv[argc - 1]);
                        result = 1;
                    }
                } else {
                    fprintf(out, "已移动: %s -> %s\n",
                            argv[i], argv[argc - 1]);
                }
            }
        }
    }

    return result;
}
// 规范化路径：解析 . 和 ..
void normalize_path(char *path) {
    char parts[MAX_ARGS][MAX_FILENAME];
    int part_count = 0;
    char temp_path[MAX_PATH_LEN];
    char result_path[MAX_PATH_LEN];
    int i;

    // 复制临时路径
    strcpy(temp_path, path);

    // 如果是空路径，返回根目录
    if (strlen(temp_path) == 0) {
        strcpy(path, "/");
        return;
    }

    // 如果路径是根目录，直接返回
    if (strcmp(temp_path, "/") == 0) {
        return;
    }

    // 跳过开头的/
    char *start = temp_path;
    if (start[0] == '/') {
        start++;
    }

    // 将路径分割成部分
    char *token = strtok(start, "/");
    while (token != NULL && part_count < MAX_ARGS) {
        strcpy(parts[part_count], token);
        part_count++;
        token = strtok(NULL, "/");
    }

    // 解析 . 和 ..
    int stack[MAX_ARGS];
    int stack_top = 0;

    for (i = 0; i < part_count; i++) {
        if (strcmp(parts[i], ".") == 0) {
            // 当前目录，忽略
            continue;
        } else if (strcmp(parts[i], "..") == 0) {
            // 上级目录
            if (stack_top > 0) {
                stack_top--;
            }
        } else if (strlen(parts[i]) > 0) {
            // 正常目录名
            if (stack_top < MAX_ARGS) {
                stack[stack_top] = i;
                stack_top++;
            }
        }
    }

    // 重新构建路径
    if (stack_top == 0) {
        strcpy(result_path, "/");
    } else {
        result_path[0] = '\0';
        for (i = 0; i < stack_top; i++) {
            strcat(result_path, "/");
            strcat(result_path, parts[stack[i]]);
        }
    }

    strcpy(path, result_path);
}

