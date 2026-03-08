// main.c
#include "declarations.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

void display_version(void) {
    printf("========================================\n");
    printf("  %s\n", LGD_OS_VERSION);
    printf("  Copyright (c) 2025-2026 sse_107\n");
    printf("  All rights reserved.\n");
    printf("========================================\n");
}

void display_welcome(void) {
    printf("\n========================================\n");
    printf("  欢迎使用 LGD-OS Shell\n");
    printf("========================================\n");
    printf("  输入 'help' 查看可用命令\n");
    printf("  输入 'exit' 退出系统\n");
    printf("========================================\n\n");
}

int main(void) {
    // 显示版本
    display_version();

    // 显示欢迎信息
    display_welcome();

    // 创建Linux风格目录结构
    create_linux_directory_structure();

    // 初始化Shell
    ShellContext *ctx = shell_init();
    if (!ctx) {
        fprintf(stderr, "错误: 无法初始化Shell上下文\n");
        return 1;
    }

    // 运行Shell
    shell_run(ctx);

    // 清理
    shell_cleanup(ctx);

    printf("\n感谢使用 LGD-OS Shell，再见！\n");
    return 0;
}
