// shell.c
#include "declarations.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#ifdef _WIN32
#include <windows.h>
#endif

// 命令表
CommandEntry command_table[] = {
    {"cd", cmd_cd},
    {"ls", cmd_ls},
    {"ll", cmd_ll},
    {"pwd", cmd_pwd},
    {"mkdir", cmd_mkdir},
    {"rmdir", cmd_rmdir},
    {"cp", cmd_cp},
    {"mv", cmd_mv},
    {"cat", cmd_cat},
    {"touch", cmd_touch},
    {"rm", cmd_rm},
    {"info", cmd_info},
    {"edit", cmd_edit},
    {"echo", cmd_echo},
    {"grep", cmd_grep},
    {"wc", cmd_wc},
    {"more", cmd_more},
    {"game", cmd_game},
    {"tree", cmd_tree},
    {"osinfo", cmd_osinfo},
    {"help", cmd_help},
    {"exit", cmd_exit},
    {"", NULL}  // 结束标记
};

// 初始化Shell
ShellContext *shell_init(void) {
    ShellContext *ctx = (ShellContext *)malloc(sizeof(ShellContext));
    if (ctx) {
        strcpy(ctx->current_dir, "/");
        ctx->history_count = 0;
    }
    return ctx;
}

// 清理Shell
void shell_cleanup(ShellContext *ctx) {
    if (ctx) {
        for (int i = 0; i < ctx->history_count; i++) {
            free(ctx->history[i]);
        }
        free(ctx);
    }
}

// shell.c
void shell_display_prompt(ShellContext *ctx) {
    char hostname[100] = "sse_107";
    char username[100] = "SSE_107";

    // 获取主机名和用户名
    #ifdef _WIN32
        DWORD size = sizeof(hostname);
        GetComputerNameA(hostname, &size);

        DWORD user_size = sizeof(username);
        GetUserNameA(username, &user_size);
    #endif

    // 输出带颜色的提示符
    printf("\033[32m%s@%s\033[0m:\033[34m%s\033[0m$ ", username, hostname, ctx->current_dir);
    fflush(stdout);
}

// 执行命令

int shell_execute_command(ShellContext *ctx, const char *cmd_line, FILE *in, FILE *out) {
    char *argv[MAX_ARGS];
    int argc = 0;
    char cmd_copy[MAX_CMD_LEN];
    char *token;
    int i;

    // 复制命令字符串
    strcpy(cmd_copy, cmd_line);

    // 分割命令为参数
    token = strtok(cmd_copy, " \t");
    while (token != NULL && argc < MAX_ARGS) {
        argv[argc++] = token;
        token = strtok(NULL, " \t");
    }

    if (argc == 0) {
        return 0;  // 空命令
    }

    // 查找命令
    for (i = 0; command_table[i].name[0] != '\0'; i++) {
        if (strcmp(command_table[i].name, argv[0]) == 0) {
            // 找到命令，执行
            return command_table[i].function(ctx, argc, argv, in, out);
        }
    }

    // 没有找到命令
    fprintf(out, "命令未找到: %s\n", argv[0]);
    return 1;
}

// 执行管道命令
int shell_execute_pipeline(ShellContext *ctx, const char *pipeline_cmd) {
    char cmd1[MAX_CMD_LEN];
    char cmd2[MAX_CMD_LEN];
    char *pipe_pos;

    // 查找管道符
    pipe_pos = strchr(pipeline_cmd, '|');
    if (pipe_pos == NULL) {
        // 没有管道，直接执行单条命令
        return shell_execute_command(ctx, pipeline_cmd, stdin, stdout);
    }

    // 分割命令
    int len = pipe_pos - pipeline_cmd;
    strncpy(cmd1, pipeline_cmd, len);
    cmd1[len] = '\0';

    // 跳过管道符和空格
    char *cmd2_start = pipe_pos + 1;
    while (*cmd2_start == ' ') {
        cmd2_start++;
    }
    strcpy(cmd2, cmd2_start);

    // 提取命令名（第一个单词）
    char cmd1_name[100];
    sscanf(cmd1, "%99s", cmd1_name);  // 从cmd1中读取第一个单词

    char cmd2_name[100];
    sscanf(cmd2, "%99s", cmd2_name);  // 从cmd2中读取第一个单词

    // 检查是否包含game命令
    if (strcmp(cmd1_name, "game") == 0) {
        fprintf(stderr, "错误: 游戏命令 'game' 是交互式程序，不能与管道一起使用。\n");
        fprintf(stderr, "请直接运行 'game' 命令进行游戏。\n");
        return 1;
    }

    if (strcmp(cmd2_name, "game") == 0) {
        fprintf(stderr, "错误: 游戏命令 'game' 是交互式程序，不能与管道一起使用。\n");
        fprintf(stderr, "请直接运行 'game' 命令进行游戏。\n");
        return 1;
    }

    // 创建临时文件
    char temp_filename[MAX_PATH_LEN];
    FILE *temp_file;

    #ifdef _WIN32
        char temp_path[MAX_PATH_LEN];
        GetTempPathA(MAX_PATH_LEN, temp_path);
        GetTempFileNameA(temp_path, "lgd", 0, temp_filename);
    #else
        strcpy(temp_filename, "/tmp/lgd_temp_XXXXXX");
        int fd = mkstemp(temp_filename);
        if (fd == -1) {
            fprintf(stderr, "错误: 无法创建临时文件\n");
            return 1;
        }
        close(fd);
    #endif

    // 执行第一个命令，输出到临时文件
    temp_file = fopen(temp_filename, "w");
    if (temp_file == NULL) {
        fprintf(stderr, "错误: 无法打开临时文件\n");
        return 1;
    }

    int status = shell_execute_command(ctx, cmd1, stdin, temp_file);
    fclose(temp_file);

    if (status != 0) {
        remove(temp_filename);
        return status;
    }

    // 执行第二个命令，从临时文件读取
    temp_file = fopen(temp_filename, "r");
    if (temp_file == NULL) {
        fprintf(stderr, "错误: 无法打开临时文件\n");
        remove(temp_filename);
        return 1;
    }

    status = shell_execute_command(ctx, cmd2, temp_file, stdout);
    fclose(temp_file);

    // 删除临时文件
    remove(temp_filename);

    return status;
}

// 运行Shell
// Shell主循环
void shell_run(ShellContext *ctx) {
    char cmd_line[MAX_CMD_LEN];
    int result;  // 添加变量来存储命令执行结果

    while (1) {
        shell_display_prompt(ctx);

        if (fgets(cmd_line, MAX_CMD_LEN, stdin) == NULL) {
            break;  // EOF
        }

        // 去除换行符
        cmd_line[strcspn(cmd_line, "\n")] = '\0';

        // 空命令
        if (strlen(cmd_line) == 0) {
            continue;
        }

        // 判断是否有管道
        if (strchr(cmd_line, '|') != NULL) {
            result = shell_execute_pipeline(ctx, cmd_line);
        } else {
            result = shell_execute_command(ctx, cmd_line, stdin, stdout);
        }

        // 检查是否需要退出
        if (result == 2) {  // exit命令返回2
            printf("退出LGD-OS。\n");
            break;
        }
    }
}
