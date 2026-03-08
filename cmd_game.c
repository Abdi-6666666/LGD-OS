
// cmd_game.c
#include "declarations.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <io.h>
#define BOARD_SIZE 9
#define MINE_COUNT 20

// 格子状态
typedef struct {
    int is_mine;           // 是否是地雷
    int is_revealed;       // 是否已翻开
    int is_flagged;        // 是否标记
    int neighbor_mines;    // 周围地雷数
} Cell;

// 游戏状态
typedef struct {
    Cell board[BOARD_SIZE][BOARD_SIZE];
    int game_over;
    int game_won;
    int mines_left;
    int first_move;
} MinesweeperGame;

// 初始化游戏
void init_game(MinesweeperGame *game) {
    int i, j;

    // 清空棋盘
    for (i = 0; i < BOARD_SIZE; i++) {
        for (j = 0; j < BOARD_SIZE; j++) {
            game->board[i][j].is_mine = 0;
            game->board[i][j].is_revealed = 0;
            game->board[i][j].is_flagged = 0;
            game->board[i][j].neighbor_mines = 0;
        }
    }

    game->game_over = 0;
    game->game_won = 0;
    game->mines_left = MINE_COUNT;
    game->first_move = 1;
}

// 放置地雷（避免在第一次点击的位置放置）
void place_mines(MinesweeperGame *game, int safe_row, int safe_col) {
    int mines_placed = 0;
    int row, col;

    srand((unsigned int)time(NULL));

    while (mines_placed < MINE_COUNT) {
        row = rand() % BOARD_SIZE;
        col = rand() % BOARD_SIZE;

        // 避免在安全位置放置地雷
        if (row == safe_row && col == safe_col) {
            continue;
        }

        if (!game->board[row][col].is_mine) {
            game->board[row][col].is_mine = 1;
            mines_placed++;
        }
    }
}

// 计算周围地雷数
void calculate_neighbors(MinesweeperGame *game) {
    int i, j, x, y;
    int count;

    for (i = 0; i < BOARD_SIZE; i++) {
        for (j = 0; j < BOARD_SIZE; j++) {
            if (!game->board[i][j].is_mine) {
                count = 0;

                // 检查周围的8个格子
                for (x = -1; x <= 1; x++) {
                    for (y = -1; y <= 1; y++) {
                        int ni = i + x;
                        int nj = j + y;

                        if (ni >= 0 && ni < BOARD_SIZE && nj >= 0 && nj < BOARD_SIZE) {
                            if (game->board[ni][nj].is_mine) {
                                count++;
                            }
                        }
                    }
                }

                game->board[i][j].neighbor_mines = count;
            }
        }
    }
}

// 展开空白区域（递归）
void reveal_blank(MinesweeperGame *game, int row, int col) {
    int i, j;

    if (row < 0 || row >= BOARD_SIZE || col < 0 || col >= BOARD_SIZE) {
        return;
    }

    if (game->board[row][col].is_revealed || game->board[row][col].is_flagged) {
        return;
    }

    game->board[row][col].is_revealed = 1;

    // 如果当前格子是空白（周围没有地雷），递归展开周围格子
    if (game->board[row][col].neighbor_mines == 0) {
        for (i = -1; i <= 1; i++) {
            for (j = -1; j <= 1; j++) {
                reveal_blank(game, row + i, col + j);
            }
        }
    }
}

// 翻开格子
int reveal_cell(MinesweeperGame *game, int row, int col) {
    if (row < 0 || row >= BOARD_SIZE || col < 0 || col >= BOARD_SIZE) {
        return 0;  // 无效位置
    }

    if (game->board[row][col].is_revealed || game->board[row][col].is_flagged) {
        return 0;  // 已翻开或已标记
    }

    // 第一次移动时放置地雷
    if (game->first_move) {
        place_mines(game, row, col);
        calculate_neighbors(game);
        game->first_move = 0;
    }

    // 翻开格子
    game->board[row][col].is_revealed = 1;

    // 如果踩到地雷
    if (game->board[row][col].is_mine) {
        game->game_over = 1;
        return -1;
    }

    // 如果翻开的是空白格子，展开周围区域
    if (game->board[row][col].neighbor_mines == 0) {
        reveal_blank(game, row, col);
    }

    return 1;
}

// 标记/取消标记格子
int flag_cell(MinesweeperGame *game, int row, int col) {
    if (row < 0 || row >= BOARD_SIZE || col < 0 || col >= BOARD_SIZE) {
        return 0;  // 无效位置
    }

    if (game->board[row][col].is_revealed) {
        return 0;  // 已翻开的格子不能标记
    }

    // 切换标记状态
    if (game->board[row][col].is_flagged) {
        game->board[row][col].is_flagged = 0;
        game->mines_left++;
    } else {
        game->board[row][col].is_flagged = 1;
        game->mines_left--;
    }

    return 1;
}

// 检查是否胜利
int check_win(MinesweeperGame *game) {
    int i, j;

    for (i = 0; i < BOARD_SIZE; i++) {
        for (j = 0; j < BOARD_SIZE; j++) {
            // 如果还有非地雷格子未翻开，则未胜利
            if (!game->board[i][j].is_mine && !game->board[i][j].is_revealed) {
                return 0;
            }
        }
    }

    return 1;
}

// 显示棋盘
void print_board(MinesweeperGame *game, FILE *out) {
    int i, j;

    fprintf(out, "\n");
    fprintf(out, "   ");
    for (j = 0; j < BOARD_SIZE; j++) {
        fprintf(out, "%2d ", j);
    }
    fprintf(out, "\n");

    fprintf(out, "  +");
    for (j = 0; j < BOARD_SIZE; j++) {
        fprintf(out, "---");
    }
    fprintf(out, "+\n");

    for (i = 0; i < BOARD_SIZE; i++) {
        fprintf(out, "%d |", i);
        for (j = 0; j < BOARD_SIZE; j++) {
            Cell *cell = &game->board[i][j];

            if (game->game_over && cell->is_mine) {
                // 游戏结束，显示所有地雷
                fprintf(out, " * ");
            } else if (cell->is_flagged) {
                // 标记的格子
                fprintf(out, " ? ");
            } else if (cell->is_revealed) {
                // 已翻开的格子
                if (cell->is_mine) {
                    fprintf(out, " * ");
                } else {
                    // 总是显示数字，包括0
                    fprintf(out, " %d ", cell->neighbor_mines);
                }
            } else {
                // 未翻开的格子
                fprintf(out, " . ");
            }
        }
        fprintf(out, "|\n");
    }

    fprintf(out, "  +");
    for (j = 0; j < BOARD_SIZE; j++) {
        fprintf(out, "---");
    }
    fprintf(out, "+\n");

    fprintf(out, "剩余地雷: %d\n", game->mines_left);

    if (game->game_over) {
        fprintf(out, "\n游戏结束！你踩到地雷了！\n");
    } else if (game->game_won) {
        fprintf(out, "\n恭喜！你赢了！\n");
    }
}

// 显示帮助
void print_help(FILE *out) {
    fprintf(out, "\n=== 扫雷游戏帮助 ===\n");
    fprintf(out, "游戏规则：\n");
    fprintf(out, "  - 9x9 网格，有10个地雷\n");
    fprintf(out, "  - 你的目标是找到所有地雷而不触发它们\n");
    fprintf(out, "\n操作说明：\n");
    fprintf(out, "  - 输入格式: 行 列 操作\n");
    fprintf(out, "  - 行和列范围: 0-8\n");
    fprintf(out, "  - 操作: 1=挖开, 2=标记/取消标记\n");
    fprintf(out, "  - 示例: 4 5 1  (挖开第4行第5列的格子)\n");
    fprintf(out, "          2 3 2  (标记第2行第3列的格子)\n");
    fprintf(out, "  - 输入 'quit' 退出游戏\n");
    fprintf(out, "  - 输入 'help' 显示此帮助\n");
    fprintf(out, "\n符号说明：\n");
    fprintf(out, "  . 未翻开\n");
    fprintf(out, "  ? 标记\n");
    fprintf(out, "  * 地雷\n");
    fprintf(out, "  数字 周围地雷数\n");
    fprintf(out, "===================\n");
}

// 扫雷游戏主函数
int cmd_game(ShellContext *ctx, int argc, char *argv[], FILE *in, FILE *out) {
    (void)ctx;  /* 标记未使用，避免警告 */

    MinesweeperGame game;
    char input[100];
    int row, col, action;
    int result;

    // 初始化随机数种子
    srand((unsigned int)time(NULL));

    // 初始化游戏
    init_game(&game);

    fprintf(out, "\n");
    fprintf(out, "╔══════════════════════════════════════════════════════════════════╗\n");
    fprintf(out, "║                      欢迎来到扫雷游戏！                            ║\n");
    fprintf(out, "║                         9x9 扫雷游戏                              ║\n");
    fprintf(out, "║                         10个地雷                                  ║\n");
    fprintf(out, "╚══════════════════════════════════════════════════════════════════╚\n");

    print_help(out);

    while (!game.game_over && !game.game_won) {
        // 显示当前棋盘
        print_board(&game, out);

        // 获取用户输入
        fprintf(out, "\n输入操作 (行 列 操作) 或输入 'help'/'quit': ");
        fflush(out);

        if (fgets(input, sizeof(input), stdin) == NULL) {
            break;  // EOF
        }

        // 去除换行符
        input[strcspn(input, "\n")] = '\0';

        // 处理特殊命令
        if (strcmp(input, "quit") == 0 || strcmp(input, "exit") == 0) {
            fprintf(out, "\n退出游戏。\n");
            return 0;
        }

        if (strcmp(input, "help") == 0) {
            print_help(out);
            continue;
        }

        // 解析输入
        if (sscanf(input, "%d %d %d", &row, &col, &action) != 3) {
            fprintf(out, "错误: 输入格式不正确。使用: 行 列 操作 (例如: 4 5 1)\n");
            continue;
        }

        // 验证输入范围
        if (row < 0 || row >= BOARD_SIZE || col < 0 || col >= BOARD_SIZE) {
            fprintf(out, "错误: 行和列必须在 0-8 范围内\n");
            continue;
        }

        if (action != 1 && action != 2) {
            fprintf(out, "错误: 操作必须是 1(挖开) 或 2(标记)\n");
            continue;
        }

        // 执行操作
        if (action == 1) {
            result = reveal_cell(&game, row, col);
            if (result == -1) {
                // 踩到地雷
                game.game_over = 1;
            } else if (result == 0) {
                fprintf(out, "这个位置不能挖开！\n");
            }
        } else if (action == 2) {
            if (!flag_cell(&game, row, col)) {
                fprintf(out, "这个位置不能标记！\n");
            }
        }

        // 检查是否胜利
        if (!game.game_over && check_win(&game)) {
            game.game_won = 1;
        }
    }

    // 显示最终棋盘
    print_board(&game, out);

    if (game.game_won) {
        fprintf(out, "\n恭喜！你成功完成了扫雷游戏！\n");
    } else if (game.game_over) {
        fprintf(out, "\n很遗憾，你踩到了地雷！\n");
    }

    fprintf(out, "\n游戏结束。\n");
    return 0;
}

