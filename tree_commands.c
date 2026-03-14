// tree_commands.c
#include "declarations.h"
#include <string.h>
#include <io.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

// 颜色定义
#define COLOR_RESET   "\033[0m"
#define COLOR_RED     "\033[31m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_YELLOW  "\033[33m"
#define COLOR_BLUE    "\033[34m"
#define COLOR_MAGENTA "\033[35m"
#define COLOR_CYAN    "\033[36m"
#define COLOR_WHITE   "\033[37m"

#define COLOR_DIR     COLOR_BLUE
#define COLOR_EXE     COLOR_GREEN
#define COLOR_FILE    COLOR_RESET

// 树形显示的全局变量
typedef struct {
    int depth;          // 当前深度
    int show_all;       // 是否显示隐藏文件
    int dir_only;       // 是否只显示目录
    int max_depth;      // 最大深度
    int dir_count;      // 目录计数
    int file_count;     // 文件计数
    FILE *out;         // 输出流
} TreeContext;

// 辅助函数：不区分大小写比较
static int my_strcasecmp(const char *s1, const char *s2) {
    while (*s1 && *s2) {
        int c1 = tolower((unsigned char)*s1);
        int c2 = tolower((unsigned char)*s2);
        if (c1 != c2) {
            return c1 - c2;
        }
        s1++;
        s2++;
    }
    return tolower((unsigned char)*s1) - tolower((unsigned char)*s2);
}
// 打印树形前缀
static void print_tree_prefix(TreeContext *ctx, int is_last, int *last_stack) {
    int i;
    for (i = 0; i < ctx->depth; i++) {
        if (i == ctx->depth - 1) {
            if (is_last) {
                fprintf(ctx->out, "`-- ");
            } else {
                fprintf(ctx->out, "|-- ");
            }
        } else {
            if (last_stack[i]) {
                fprintf(ctx->out, "    ");
            } else {
                fprintf(ctx->out, "|   ");
            }
        }
    }
}

// 打印目录树
static void print_directory_tree(TreeContext *ctx, const char *real_path, int *last_stack) {
    char search_path[MAX_PATH_LEN];
    char sub_path[MAX_PATH_LEN];
    struct _finddata_t fileinfo;
    intptr_t handle;

    // 构建搜索路径
    snprintf(search_path, MAX_PATH_LEN, "%s\\*.*", real_path);

    // 获取目录列表
    int count = 0;
    int entries[256];  // 存储条目索引
    char *names[256];  // 存储条目名称
    int types[256];    // 存储条目类型: 0=目录, 1=文件

    handle = _findfirst(search_path, &fileinfo);
    if (handle == -1L) {
        return;
    }

    // 收集所有条目
    do {
        // 跳过 . 和 ..
        if (strcmp(fileinfo.name, ".") == 0 || strcmp(fileinfo.name, "..") == 0) {
            continue;
        }

        // 检查是否需要显示隐藏文件
        if (!ctx->show_all && fileinfo.name[0] == '.') {
            continue;
        }

        // 检查是否只显示目录
        if (ctx->dir_only && !(fileinfo.attrib & _A_SUBDIR)) {
            continue;
        }

        // 存储条目信息
        entries[count] = count;
        names[count] = malloc(strlen(fileinfo.name) + 1);
        strcpy(names[count], fileinfo.name);

        if (fileinfo.attrib & _A_SUBDIR) {
            types[count] = 0;  // 目录
        } else {
            types[count] = 1;  // 文件
        }

        count++;
    } while (_findnext(handle, &fileinfo) == 0 && count < 256);

    _findclose(handle);

    // 排序：目录在前，文件在后，按名称排序
    for (int i = 0; i < count - 1; i++) {
        for (int j = i + 1; j < count; j++) {
            int swap = 0;

            // 目录在前
            if (types[i] > types[j]) {
                swap = 1;
            }
            // 同类型按名称排序
            else if (types[i] == types[j] && my_strcasecmp(names[i], names[j]) > 0) {
                swap = 1;
            }

            if (swap) {
                // 交换索引
                int temp_entry = entries[i];
                entries[i] = entries[j];
                entries[j] = temp_entry;

                // 交换名称
                char *temp_name = names[i];
                names[i] = names[j];
                names[j] = temp_name;

                // 交换类型
                int temp_type = types[i];
                types[i] = types[j];
                types[j] = temp_type;
            }
        }
    }

    // 打印所有条目
    for (int i = 0; i < count; i++) {
        int idx = entries[i];
        int is_last = (i == count - 1);

        // 打印前缀
        print_tree_prefix(ctx, is_last, last_stack);

        // 打印条目
        if (types[idx] == 0) {  // 目录
            fprintf(ctx->out, COLOR_DIR "%s" COLOR_RESET "\n", names[idx]);
            ctx->dir_count++;

            // 如果不是最大深度，递归处理子目录
            if (ctx->depth < ctx->max_depth) {
                // 构建子目录路径
                snprintf(sub_path, MAX_PATH_LEN, "%s\\%s", real_path, names[idx]);

                // 更新最后标记栈
                last_stack[ctx->depth] = is_last;
                ctx->depth++;

                // 递归处理子目录
                print_directory_tree(ctx, sub_path, last_stack);

                ctx->depth--;
            }
        } else {  // 文件
            // 检查是否是可执行文件
            char *ext = strrchr(names[idx], '.');
            if (ext && (strcasecmp(ext, ".exe") == 0 || strcasecmp(ext, ".bat") == 0 ||
                       strcasecmp(ext, ".cmd") == 0 || strcasecmp(ext, ".com") == 0)) {
                fprintf(ctx->out, COLOR_EXE "%s" COLOR_RESET "\n", names[idx]);
            } else {
                fprintf(ctx->out, "%s\n", names[idx]);
            }
            ctx->file_count++;
        }

        // 释放内存
        free(names[idx]);
    }

    if (count == 0) {
        print_tree_prefix(ctx, 1, last_stack);
        fprintf(ctx->out, "(空)\n");
    }
}

// tree命令
int cmd_tree(ShellContext *ctx, int argc, char *argv[], FILE *in, FILE *out) {
    TreeContext tree_ctx = {0};
    char real_path[MAX_PATH_LEN];
    char *target_path = NULL;
    int last_stack[20] = {0};  // 支持最多20层深度

    (void)in;  // 标记未使用

    // 默认设置
    tree_ctx.show_all = 0;
    tree_ctx.dir_only = 0;
    tree_ctx.max_depth = 100;  // 默认不限制深度
    tree_ctx.dir_count = 0;
    tree_ctx.file_count = 0;
    tree_ctx.out = out;

    // 解析参数
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            if (strcmp(argv[i], "-a") == 0) {
                tree_ctx.show_all = 1;
            } else if (strcmp(argv[i], "-d") == 0) {
                tree_ctx.dir_only = 1;
            } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
                fprintf(out, "用法: tree [选项] [路径]\n");
                fprintf(out, "  以树状图列出目录结构\n");
                fprintf(out, "  选项:\n");
                fprintf(out, "    -a      显示所有文件，包括隐藏文件\n");
                fprintf(out, "    -d      只显示目录\n");
                fprintf(out, "    -L 数字 限制显示的层级深度\n");
                fprintf(out, "    -h, --help  显示此帮助信息\n");
                fprintf(out, "  示例:\n");
                fprintf(out, "    tree          显示当前目录的树状图\n");
                fprintf(out, "    tree /usr     显示指定目录的树状图\n");
                fprintf(out, "    tree -a       显示所有文件（包括隐藏文件）\n");
                fprintf(out, "    tree -d       只显示目录\n");
                fprintf(out, "    tree -L 2     只显示2层深度\n");
                return 0;
            } else if (strcmp(argv[i], "-L") == 0 && i + 1 < argc) {
                tree_ctx.max_depth = atoi(argv[++i]);
                if (tree_ctx.max_depth <= 0) {
                    fprintf(out, "错误: 深度必须为正整数\n");
                    return 1;
                }
            } else {
                fprintf(out, "错误: 未知选项 '%s'\n", argv[i]);
                fprintf(out, "使用 'tree -h' 查看帮助\n");
                return 1;
            }
        } else {
            target_path = argv[i];
        }
    }

    // 获取目标路径
    if (target_path == NULL) {
        // 使用当前目录
        virtual_to_real_path(ctx->current_dir, real_path);
    } else {
        virtual_to_real_path(target_path, real_path);
    }

    // 检查路径是否存在
    struct _finddata_t fileinfo;
    intptr_t handle = _findfirst(real_path, &fileinfo);
    if (handle == -1L) {
        fprintf(out, "错误: 路径不存在 '%s'\n", target_path ? target_path : ctx->current_dir);
        return 1;
    }
    _findclose(handle);

    // 检查是否是目录
    if (!(fileinfo.attrib & _A_SUBDIR)) {
        fprintf(out, "错误: '%s' 不是目录\n", target_path ? target_path : ctx->current_dir);
        return 1;
    }

    // 打印根目录
    if (target_path) {
        fprintf(out, COLOR_DIR "%s" COLOR_RESET "\n", target_path);
    } else {
        fprintf(out, COLOR_DIR "%s" COLOR_RESET "\n", ctx->current_dir);
    }

    // 打印树形结构
    tree_ctx.depth = 1;
    print_directory_tree(&tree_ctx, real_path, last_stack);

    // 打印统计信息
    fprintf(out, "\n");
    fprintf(out, "%d 个目录, %d 个文件\n", tree_ctx.dir_count, tree_ctx.file_count);

    return 0;
}
