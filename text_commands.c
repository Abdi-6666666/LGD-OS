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
            fprintf(out,"\n");
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


// more命令 - 分页显示文件内容
int cmd_more(ShellContext *ctx, int argc, char *argv[], FILE *in, FILE *out) {
    (void)ctx;  // 标记未使用

    int lines_per_page = 24;  // 默认每页24行
    FILE *source = NULL;
    int is_stdin = 0;
    int ch;
    int line_count = 0;
    int page_count = 0;
    char buffer[MAX_PATH_LEN];  // 使用MAX_PATH_LEN替代MAX_INPUT_LEN
    int i = 1;

    // 解析选项
    while (i < argc) {
        if (strcmp(argv[i], "-n") == 0) {
            if (i + 1 < argc) {
                lines_per_page = atoi(argv[i + 1]);
                if (lines_per_page <= 0) {
                    lines_per_page = 24;
                }
                i += 2;
            } else {
                fprintf(out, "错误: -n 选项需要参数\n");
                return 1;
            }
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            fprintf(out, "more - 分页显示文件内容\n");
            fprintf(out, "用法: more [选项] [文件名...]\n");
            fprintf(out, "选项:\n");
            fprintf(out, "  -n NUM    每页显示NUM行（默认为24行）\n");
            fprintf(out, "  -h, --help 显示此帮助信息\n");
            fprintf(out, "\n注意:\n");
            fprintf(out, "  1. 如果没有指定文件名，则从标准输入读取\n");
            fprintf(out, "  2. 支持管道，如: ls | more\n");
            fprintf(out, "  3. 支持显示多个文件\n");
            fprintf(out, "\n快捷键:\n");
            fprintf(out, "  空格键: 下一页\n");
            fprintf(out, "  Enter键: 下一行\n");
            fprintf(out, "  q键: 退出\n");
            fprintf(out, "\n示例:\n");
            fprintf(out, "  more file.txt          # 显示文件\n");
            fprintf(out, "  more -n 10 file.txt    # 每页10行显示文件\n");
            fprintf(out, "  cat file.txt | more    # 从管道读取\n");
            fprintf(out, "  ls | more              # 分页显示目录列表\n");
            return 0;
        } else if (argv[i][0] == '-') {
            fprintf(out, "错误: 未知选项 '%s'\n", argv[i]);
            return 1;
        } else {
            break;
        }
    }

    // 判断是否有文件名参数
    if (i < argc) {
        // 有文件名参数，从文件读取
        char real_path[MAX_PATH_LEN];
        virtual_to_real_path(argv[i], real_path);
        source = fopen(real_path, "r");
        if (source == NULL) {
            fprintf(out, "错误: 无法打开文件 '%s'\n", argv[i]);
            return 1;
        }
        is_stdin = 0;
    } else {
        // 没有文件名参数，检查是否从管道输入
        if (in == stdin) {
            // 没有管道输入
            fprintf(out, "错误: 没有指定输入源\n");
            fprintf(out, "用法:\n");
            fprintf(out, "  more 文件名        # 显示文件内容\n");
            fprintf(out, "  命令 | more        # 显示命令输出\n");
            fprintf(out, "\n输入 'more -h' 获取详细帮助\n");
            return 1;
        }
        // 从管道/标准输入读取
        source = in;
        is_stdin = 1;
    }

    // 设置终端为无缓冲模式，以便立即读取按键
    #ifdef _WIN32
    #include <conio.h>
    #else
    #include <termios.h>
    #include <unistd.h>
    struct termios oldt, newt;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    #endif

    // 分页显示
    while (fgets(buffer, sizeof(buffer), source) != NULL) {
        fputs(buffer, out);
        line_count++;

        if (line_count >= lines_per_page) {
            page_count++;
            fprintf(out, "-- 第 %d 页，按空格键继续，Enter下一行，q退出 --", page_count);
            fflush(out);

            // 等待用户输入
            #ifdef _WIN32
            ch = _getch();
            #else
            ch = getchar();
            #endif

            if (ch == 'q' || ch == 'Q') {
                fprintf(out, "\n");
                break;
            } else if (ch == ' ') {
                // 空格键：下一页
                fprintf(out, "\r\033[K");  // 清除当前行
                line_count = 0;
            } else if (ch == '\r' || ch == '\n') {
                // Enter键：下一行
                fprintf(out, "\r\033[K");  // 清除当前行
                // line_count不变，这样只会再显示一行
            } else {
                // 其他按键，显示帮助
                fprintf(out, "\n快捷键: 空格=下一页, Enter=下一行, q=退出\n");
                fprintf(out, "-- 继续显示 --");
                fflush(out);
                #ifdef _WIN32
                ch = _getch();
                #else
                ch = getchar();
                #endif
                if (ch == 'q' || ch == 'Q') {
                    fprintf(out, "\n");
                    break;
                }
                fprintf(out, "\r\033[K");
                line_count = 0;
            }
        }
    }

    // 恢复终端设置
    #ifndef _WIN32
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    #endif

    // 如果不是标准输入，关闭文件
    if (!is_stdin && source != NULL) {
        fclose(source);
    }

    // 如果完全显示完毕
    if (feof(source)) {
        if (page_count > 0) {
            fprintf(out, "\n-- 已到文件末尾 --\n");
        }
    } else if (ferror(source)) {
        fprintf(out, "\n错误: 读取文件时发生错误\n");
        return 1;
    }

    return 0;
}
