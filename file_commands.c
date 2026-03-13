// file_commands.c
#include "declarations.h"
#include <string.h>
#include <time.h>
#include <errno.h>
#ifdef _WIN32
#include <windows.h>
#endif

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

// touch命令 - 创建空文件或更新时间戳
int cmd_touch(ShellContext *ctx, int argc, char *argv[], FILE *in, FILE *out) {
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
        char virtual_path[MAX_PATH_LEN];
        if (strcmp(ctx->current_dir, "/") == 0) {
            snprintf(virtual_path, MAX_PATH_LEN, "/%s", argv[i]);
        } else {
            snprintf(virtual_path, MAX_PATH_LEN, "%s/%s", ctx->current_dir, argv[i]);
        }

        virtual_to_real_path(virtual_path, real_path);

        if (_access(real_path, 0) == 0) {
            fp = fopen(real_path, "r+");
            if (fp != NULL) {
                fclose(fp);
                fprintf(out, "更新时间戳: %s\n", argv[i]);
            } else {
                fprintf(out, "touch: 无法更新文件: %s\n", argv[i]);
            }
        } else {
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
        char virtual_path[MAX_PATH_LEN];
        if (strcmp(ctx->current_dir, "/") == 0) {
            snprintf(virtual_path, MAX_PATH_LEN, "/%s", argv[i]);
        } else {
            snprintf(virtual_path, MAX_PATH_LEN, "%s/%s", ctx->current_dir, argv[i]);
        }

        virtual_to_real_path(virtual_path, real_path);

        if (_access(real_path, 0) != 0) {
            if (!force) {
                fprintf(out, "rm: 文件不存在: %s\n", argv[i]);
            }
            continue;
        }

        if (is_directory(real_path)) {
            if (recursive) {
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
            if (remove(real_path) == 0) {
                fprintf(out, "已删除文件: %s\n", argv[i]);
            } else {
                fprintf(out, "rm: 无法删除文件: %s\n", argv[i]);
            }
        }
    }

    return 0;
}
