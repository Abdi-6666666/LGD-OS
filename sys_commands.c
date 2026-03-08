// sys_commands.c
#include "declarations.h"
#include <string.h>

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
            "more",
            "分页显示文本内容",
            "用法: more [选项] [文件] [文件2 ...]\n"
            "  分页显示文本文件内容，或从标准输入读取。\n"
            "  支持管道：command | more\n"
            "  选项:\n"
            "    -n 数字  设置每页显示的行数（默认24行）\n"
            "  控制键:\n"
            "    空格键   显示下一页\n"
            "    q键      退出\n"
            "    回车键   显示下一行\n"
            "  示例:\n"
            "    more file.txt        分页显示文件内容\n"
            "    cat file.txt | more  通过管道分页显示\n"
            "    ls -l | more         分页显示ls输出\n"
            "    more -n 10 file.txt  每页显示10行\n"
        },
        {
            "game",
            "扫雷游戏",
            "用法: game\n"
            "  启动9x9扫雷游戏，有10个地雷。\n"
            "  游戏规则：\n"
            "    1. 目标是找到所有地雷而不触发它们\n"
            "    2. 输入格式: 行 列 操作\n"
            "    3. 操作: 1=挖开, 2=标记/取消标记\n"
            "    4. 数字表示周围地雷数量\n"
            "    5. 标记用?表示，地雷用*表示\n"
            "  游戏内命令：\n"
            "    help    显示游戏帮助\n"
            "    quit    退出游戏\n"
            "  示例：\n"
            "    game             开始新游戏\n"
            "    4 5 1            挖开第4行第5列的格子\n"
            "    2 3 2            标记第2行第3列的格子\n"
        },
        {
            "tree",
            "树状显示目录结构",
            "用法: tree [选项] [路径]\n"
            "  以树状图显示目录结构。\n"
            "  选项:\n"
            "    -a      显示所有文件，包括隐藏文件\n"
            "    -d      只显示目录\n"
            "    -L 数字 限制显示的层级深度\n"
            "    -h, --help  显示此帮助信息\n"
            "  示例:\n"
            "    tree          显示当前目录的树状图\n"
            "    tree /usr     显示指定目录的树状图\n"
            "    tree -a       显示所有文件（包括隐藏文件）\n"
            "    tree -d       只显示目录\n"
            "    tree -L 2     只显示2层深度\n"
        },
        {
            "osinfo",
            "显示LGD-OS系统信息和作者信息",
            "用法: osinfo\n"
            "  显示LGD-OS的详细信息，包括：\n"
            "  - 系统版本和编译时间\n"
            "  - 开发者信息\n"
            "  - 系统特性\n"
            "  - 项目结构\n"
            "  - 使用说明\n"
            "  示例:\n"
            "    osinfo      显示完整的系统信息\n"
        },
        {
            "help",
            "显示帮助信息",
            "用法: help [命令名]\n"
            "  显示所有可用命令的简要介绍，或指定命令的详细信息。\n"
            "  特殊参数:\n"
            "    all  显示所有命令的详细信息\n"
            "  示例:\n"
            "    help          显示所有命令的简要帮助\n"
            "    help cd       显示cd命令的详细信息\n"
            "    help ls       显示ls命令的详细信息\n"
            "    help all      显示所有命令的详细信息\n"
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
        fprintf(out, " 输入 'help all' 获取所有命令的详细信息\n");
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
        // 检查是否是 "all" 参数
        if (strcmp(argv[1], "all") == 0) {
            // 显示所有命令的详细信息
            fprintf(out, "\n");
            fprintf(out, "╔══════════════════════════════════════════════════════════════════╗\n");
            fprintf(out, "║                    LGD-OS Shell 完整命令手册                     ║\n");
            fprintf(out, "║                    版本: %s                      ║\n", LGD_OS_VERSION);
            fprintf(out, "║                    共 %d 个命令                                ║\n", command_count);
            fprintf(out, "╚══════════════════════════════════════════════════════════════════╚\n");

            for (int i = 0; i < command_count; i++) {
                fprintf(out, "\n");
                fprintf(out, "════════════════════════════════════════════════════════════════════\n");
                fprintf(out, "命令: %s\n", commands[i].name);
                fprintf(out, "════════════════════════════════════════════════════════════════════\n");
                fprintf(out, "\n");
                fprintf(out, "简介: %s\n", commands[i].brief);
                fprintf(out, "\n");
                fprintf(out, "详细信息:\n");
                fprintf(out, "%s\n", commands[i].detail);

                // 在命令之间添加分隔线
                if (i < command_count - 1) {
                    fprintf(out, "\n");
                    fprintf(out, "------------------------------------------------------------------------\n");
                }
            }

            fprintf(out, "\n");
            fprintf(out, "════════════════════════════════════════════════════════════════════\n");
            fprintf(out, "                        LGD-OS Shell 命令手册结束                     \n");
            fprintf(out, "════════════════════════════════════════════════════════════════════\n");
            fprintf(out, "\n");

        } else {
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
                fprintf(out, "输入 'help all' 查看所有命令的详细信息。\n");
                return 1;
            }
        }
    } else {
        fprintf(out, "用法: help [命令名]\n");
        fprintf(out, "  显示所有可用命令的简要介绍，或指定命令的详细信息。\n");
        fprintf(out, "  特殊参数:\n");
        fprintf(out, "    all  显示所有命令的详细信息\n");
        fprintf(out, "  示例:\n");
        fprintf(out, "    help          显示所有命令的简要帮助\n");
        fprintf(out, "    help cd       显示cd命令的详细信息\n");
        fprintf(out, "    help all      显示所有命令的详细信息\n");
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
