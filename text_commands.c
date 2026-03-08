// text_commands.c
#include "declarations.h"
#include <string.h>
#include <ctype.h>

// cat命令 - 显示文件内容
int cmd_cat(ShellContext *ctx, int argc, char *argv[], FILE *in, FILE *out) {
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
            char virtual_path[MAX_PATH_LEN];
            if (strcmp(ctx->current_dir, "/") == 0) {
                snprintf(virtual_path, MAX_PATH_LEN, "/%s", argv[i]);
            } else {
                snprintf(virtual_path, MAX_PATH_LEN, "%s/%s", ctx->current_dir, argv[i]);
            }

            virtual_to_real_path(virtual_path, real_path);

            if (_access(real_path, 0) != 0) {
                fprintf(out, "cat: 文件不存在: %s\n", argv[i]);
                continue;
            }

            if (!is_file(real_path)) {
                fprintf(out, "cat: 不是文件: %s\n", argv[i]);
                continue;
            }

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

// edit命令 - 编辑文本文件
int cmd_edit(ShellContext *ctx, int argc, char *argv[], FILE *in, FILE *out) {
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
        char virtual_path[MAX_PATH_LEN];
        if (strcmp(ctx->current_dir, "/") == 0) {
            snprintf(virtual_path, MAX_PATH_LEN, "/%s", argv[i]);
        } else {
            snprintf(virtual_path, MAX_PATH_LEN, "%s/%s", ctx->current_dir, argv[i]);
        }

        virtual_to_real_path(virtual_path, real_path);

        char *ext = strrchr(real_path, '.');
        if (ext == NULL || (strcmp(ext, ".txt") != 0 && strcmp(ext, ".log") != 0)) {
            snprintf(temp_path, MAX_PATH_LEN, "%s.txt", real_path);
        } else {
            strcpy(temp_path, real_path);
        }

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
        for (; i < argc; i++) {
            char real_path[MAX_PATH_LEN];
            char virtual_path[MAX_PATH_LEN];

            if (strcmp(ctx->current_dir, "/") == 0) {
                snprintf(virtual_path, MAX_PATH_LEN, "/%s", argv[i]);
            } else {
                snprintf(virtual_path, MAX_PATH_LEN, "%s/%s", ctx->current_dir, argv[i]);
            }

            virtual_to_real_path(virtual_path, real_path);

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

    if (i >= argc) {
        char line[MAX_CONTENT_LEN];

        while (fgets(line, sizeof(line), in) != NULL) {
            total_lines++;
            total_bytes += strlen(line);

            for (int j = 0; line[j]; j++) {
                total_chars++;
            }

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
        for (; i < argc; i++) {
            char real_path[MAX_PATH_LEN];
            char virtual_path[MAX_PATH_LEN];

            if (strcmp(ctx->current_dir, "/") == 0) {
                snprintf(virtual_path, MAX_PATH_LEN, "/%s", argv[i]);
            } else {
                snprintf(virtual_path, MAX_PATH_LEN, "%s/%s", ctx->current_dir, argv[i]);
            }

            virtual_to_real_path(virtual_path, real_path);

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

                for (int j = 0; line[j]; j++) {
                    file_chars++;
                }

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



#ifdef _WIN32
#include <conio.h>  // 用于 _getch() 函数
#endif

// more命令 - 分页显示文本
int cmd_more(ShellContext *ctx, int argc, char *argv[], FILE *in, FILE *out) {
    int i;
    int lines_per_page = 24;  // 每页显示的行数
    int line_count = 0;
    int total_lines = 0;
    int file_count = 0;
    char line[MAX_CONTENT_LEN];
    char *filenames[MAX_ARGS];
    FILE *fp = NULL;

    // 如果没有文件名参数，从标准输入读取
    if (argc < 2) {
        // 从标准输入读取
        while (fgets(line, sizeof(line), in) != NULL) {
            fprintf(out, "%s", line);
            line_count++;
            total_lines++;

            if (line_count >= lines_per_page) {
                // 暂停，等待用户按键
                fprintf(out, "--更多--(按空格键继续, 按q键退出)");
                fflush(out);

                #ifdef _WIN32
                    int ch = _getch();
                #else
                    int ch = getchar();
                #endif

                fprintf(out, "\r                                \r");  // 清除提示

                if (ch == 'q' || ch == 'Q') {
                    fprintf(out, "\n(已退出)\n");
                    break;
                }
                line_count = 0;
            }
        }

        fprintf(out, "\n总共显示 %d 行\n", total_lines);
        return 0;
    }

    // 有文件名参数
    for (i = 1; i < argc; i++) {
        // 检查是否是选项
        if (strcmp(argv[i], "-n") == 0 && i + 1 < argc) {
            // 设置每页行数
            lines_per_page = atoi(argv[++i]);
            if (lines_per_page <= 0) {
                lines_per_page = 24;
            }
        } else {
            filenames[file_count++] = argv[i];
        }
    }

    // 处理文件
    for (i = 0; i < file_count; i++) {
        char real_path[MAX_PATH_LEN];
        char virtual_path[MAX_PATH_LEN];

        // 构建虚拟路径
        if (strcmp(ctx->current_dir, "/") == 0) {
            snprintf(virtual_path, MAX_PATH_LEN, "/%s", filenames[i]);
        } else {
            snprintf(virtual_path, MAX_PATH_LEN, "%s/%s", ctx->current_dir, filenames[i]);
        }

        // 转换为真实路径
        virtual_to_real_path(virtual_path, real_path);

        // 检查文件是否存在
        if (_access(real_path, 0) != 0) {
            fprintf(out, "more: 文件不存在: %s\n", filenames[i]);
            continue;
        }

        // 打开文件
        fp = fopen(real_path, "r");
        if (fp == NULL) {
            fprintf(out, "more: 无法打开文件: %s\n", filenames[i]);
            continue;
        }

        if (file_count > 1) {
            fprintf(out, "\n=== 文件: %s ===\n\n", filenames[i]);
        }

        line_count = 0;
        int file_lines = 0;

        // 读取并显示文件内容
        while (fgets(line, sizeof(line), fp) != NULL) {
            fprintf(out, "%s", line);
            line_count++;
            file_lines++;
            total_lines++;

            if (line_count >= lines_per_page) {
                // 暂停，等待用户按键
                fprintf(out, "--更多--(按空格键继续, 按q键退出)");
                fflush(out);

                #ifdef _WIN32
                    int ch = _getch();
                #else
                    int ch = getchar();
                #endif

                fprintf(out, "\r                                \r");  // 清除提示

                if (ch == 'q' || ch == 'Q') {
                    fprintf(out, "\n(已退出)\n");
                    break;
                }
                line_count = 0;
            }
        }

        fclose(fp);
        fp = NULL;

        fprintf(out, "\n(%s: 共 %d 行)\n", filenames[i], file_lines);

        if (i < file_count - 1) {
            // 不是最后一个文件，等待用户确认
            fprintf(out, "\n按任意键查看下一个文件，按q键退出...");
            fflush(out);

            #ifdef _WIN32
                int ch = _getch();
            #else
                int ch = getchar();
            #endif

            fprintf(out, "\n");
            if (ch == 'q' || ch == 'Q') {
                break;
            }
        }
    }

    if (file_count > 1) {
        fprintf(out, "\n总共显示 %d 个文件，%d 行\n", i, total_lines);
    }

    return 0;
}
