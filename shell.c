// shell.c
#include "declarations.h"
#include <string.h>
#include <ctype.h>

// 命令表定义
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
    {"help", cmd_help},
    {"exit", cmd_exit},
    {"", NULL}  // 结束标记
};

// 初始化Shell上下文
ShellContext *shell_init(void) {
    ShellContext *ctx = (ShellContext*)calloc(1, sizeof(ShellContext));
    if (!ctx) {
        return NULL;
    }

    // 初始化虚拟当前目录为根目录
    strcpy(ctx->current_dir, "/");

    // 初始化历史记录
    ctx->history_count = 0;
    for (int i = 0; i < MAX_HISTORY; i++) {
        ctx->history[i] = NULL;
    }

    return ctx;
}

// 清理Shell上下文
void shell_cleanup(ShellContext *ctx) {
    if (!ctx) return;

    // 清理历史记录
    for (int i = 0; i < ctx->history_count; i++) {
        if (ctx->history[i]) {
            free(ctx->history[i]);
        }
    }

    free(ctx);
}

// Windows控制台颜色函数
#ifdef _WIN32
void set_console_color(int color) {
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    SetConsoleTextAttribute(hConsole, color);
}

void reset_console_color(void) {
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    SetConsoleTextAttribute(hConsole, 7);  // 默认颜色
}
#else
// Linux/Unix颜色代码
#define ANSI_RESET   "\033[0m"
#define ANSI_BLACK   "\033[30m"
#define ANSI_RED     "\033[31m"
#define ANSI_GREEN   "\033[32m"
#define ANSI_YELLOW  "\033[33m"
#define ANSI_BLUE    "\033[34m"
#define ANSI_MAGENTA "\033[35m"
#define ANSI_CYAN    "\033[36m"
#define ANSI_WHITE   "\033[37m"
#define ANSI_BOLD    "\033[1m"

void set_console_color(int color) {
    // 这里简化实现，实际使用时需要处理
}

void reset_console_color(void) {
    printf(ANSI_RESET);
}
#endif

// 获取主机名
void get_hostname(char *buffer, int size) {
    // Windows系统获取主机名
    #ifdef _WIN32
        DWORD size_needed = size;
        if (GetComputerNameA(buffer, &size_needed)) {
            return;
        }
    #endif
    // 如果获取失败，使用默认值
    strcpy(buffer, "lgd-os");
}

// 获取用户名
void get_username(char *buffer, int size) {
    // Windows系统获取用户名
    #ifdef _WIN32
        DWORD size_needed = size;
        if (GetUserNameA(buffer, &size_needed)) {
            return;
        }
    #endif

    // 尝试从环境变量获取
    char *env_user = getenv("USERNAME");
    if (env_user) {
        strncpy(buffer, env_user, size - 1);
        buffer[size - 1] = '\0';
    } else {
        strcpy(buffer, "user");
    }
}

// 显示提示符 - 经典Linux风格
void shell_display_prompt(ShellContext *ctx) {
    char hostname[256];
    char username[256];

    get_hostname(hostname, sizeof(hostname));
    get_username(username, sizeof(username));

    // 主机名@用户名（绿色）
    set_console_color(FOREGROUND_GREEN | FOREGROUND_INTENSITY);
    printf("%s@%s", hostname, username);

    // 冒号和路径（蓝色）
    set_console_color(FOREGROUND_BLUE | FOREGROUND_INTENSITY);
    printf(":%s", ctx->current_dir);

    // 提示符$（白色）
    set_console_color(FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY);
    printf("$ ");

    reset_console_color();

    fflush(stdout);
}

// 分割命令参数
void split_command(char *cmd, char **args, int *argc) {
    *argc = 0;
    char *token = strtok(cmd, " \t\n");

    while (token != NULL && *argc < MAX_ARGS - 1) {
        args[*argc] = token;
        (*argc)++;
        token = strtok(NULL, " \t\n");
    }
    args[*argc] = NULL;
}

// 执行单个命令
int shell_execute_single_command(ShellContext *ctx, const char *cmd_line, FILE *in, FILE *out) {
    if (!ctx || !cmd_line) {
        return 1;
    }

    // 复制命令
    char cmd_copy[MAX_CMD_LEN];
    strncpy(cmd_copy, cmd_line, MAX_CMD_LEN - 1);
    cmd_copy[MAX_CMD_LEN - 1] = '\0';

    // 去除首尾空白
    char *start = cmd_copy;
    while (*start && (*start == ' ' || *start == '\t')) start++;
    char *end = start + strlen(start) - 1;
    while (end > start && (*end == ' ' || *end == '\t' || *end == '\n')) *end-- = '\0';

    if (*start == '\0') {
        return 0;  // 空命令
    }

    // 添加到历史记录
    if (ctx->history_count < MAX_HISTORY) {
        ctx->history[ctx->history_count] = _strdup(start);
        if (ctx->history[ctx->history_count]) {
            ctx->history_count++;
        }
    }

    // 分割参数
    char *args[MAX_ARGS];
    int argc = 0;
    split_command(start, args, &argc);

    if (argc == 0) {
        return 0;
    }

    // 查找并执行命令
    for (int i = 0; command_table[i].function != NULL; i++) {
        if (strcmp(args[0], command_table[i].name) == 0) {
            return command_table[i].function(ctx, argc, args, in, out);
        }
    }

    // 错误信息用红色
    set_console_color(FOREGROUND_RED | FOREGROUND_INTENSITY);
    fprintf(out, "错误: 未知命令 '%s'\n", args[0]);
    reset_console_color();
    return 1;
}

// 执行管道命令
int shell_execute_pipeline(ShellContext *ctx, const char *pipeline_cmd) {
    if (!ctx || !pipeline_cmd) {
        return 1;
    }

    // 复制管道命令
    char pipeline_copy[MAX_CMD_LEN];
    strncpy(pipeline_copy, pipeline_cmd, MAX_CMD_LEN - 1);
    pipeline_copy[MAX_CMD_LEN - 1] = '\0';

    // 分割管道命令
    char *commands[MAX_PIPES];
    int command_count = 0;
    char *token = strtok(pipeline_copy, "|");

    while (token != NULL && command_count < MAX_PIPES) {
        // 去除命令前后的空白
        char *start = token;
        while (*start && (*start == ' ' || *start == '\t')) start++;
        char *end = start + strlen(start) - 1;
        while (end > start && (*end == ' ' || *end == '\t')) *end-- = '\0';

        commands[command_count] = start;
        command_count++;
        token = strtok(NULL, "|");
    }

    if (command_count == 0) {
        return 1;
    }

    if (command_count == 1) {
        // 单个命令，直接执行
        return shell_execute_single_command(ctx, commands[0], stdin, stdout);
    }

    // 多个命令，需要管道连接
    FILE *pipes[MAX_PIPES][2];  // [命令索引][0:读端, 1:写端]
    int result = 0;

    // 创建临时文件作为管道
    for (int i = 0; i < command_count - 1; i++) {
        pipes[i][1] = tmpfile();  // 临时文件作为写端
        if (!pipes[i][1]) {
            set_console_color(FOREGROUND_RED | FOREGROUND_INTENSITY);
            printf("错误: 无法创建管道\n");
            reset_console_color();
            return 1;
        }
        rewind(pipes[i][1]);  // 确保在开头
    }

    // 执行管道中的每个命令
    for (int i = 0; i < command_count; i++) {
        FILE *input = (i == 0) ? stdin : pipes[i-1][1];
        FILE *output = (i == command_count - 1) ? stdout : pipes[i][1];

        // 重新定位到文件开头以便读取
        if (input != stdin) {
            rewind(input);
        }

        result = shell_execute_single_command(ctx, commands[i], input, output);

        if (result != 0) {
            break;
        }

        // 如果不是最后一个命令，需要将输出刷入
        if (output != stdout) {
            fflush(output);
        }
    }

    // 关闭临时文件
    for (int i = 0; i < command_count - 1; i++) {
        if (pipes[i][1]) {
            fclose(pipes[i][1]);
        }
    }

    return result;
}

// 执行命令
int shell_execute_command(ShellContext *ctx, const char *cmd_line) {
    if (!ctx || !cmd_line) {
        return 1;
    }

    // 检查是否是管道命令
    if (strchr(cmd_line, '|') != NULL) {
        return shell_execute_pipeline(ctx, cmd_line);
    } else {
        return shell_execute_single_command(ctx, cmd_line, stdin, stdout);
    }
}

// Shell主循环
void shell_run(ShellContext *ctx) {
    char line[MAX_CMD_LEN];

    while (1) {
        // 显示提示符
        shell_display_prompt(ctx);

        // 读取输入
        if (fgets(line, sizeof(line), stdin) == NULL) {
            break;  // EOF
        }

        // 执行命令
        int result = shell_execute_command(ctx, line);

        if (result == 2) {  // exit命令的特殊返回值
            break;
        }
    }
}
