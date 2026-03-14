// calculator.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include "declarations.h"

// 计算器状态结构
typedef struct {
    double memory;  // 内存值
    int use_radians; // 是否使用弧度
} CalculatorState;

// 计算器状态
static CalculatorState calc_state = {0, 0};

// 帮助函数：判断是否是运算符
static int is_operator(char c) {
    return c == '+' || c == '-' || c == '*' || c == '/' || c == '^' || c == '[';
}

// 帮助函数：获取运算符优先级
static int get_precedence(char op) {
    switch (op) {
        case '+':
        case '-':
            return 1;
        case '*':
        case '/':
            return 2;
        case '^':
            return 3;
        case '[':  // 开方运算
            return 4;
        default:
            return 0;
    }
}

// 帮助函数：执行运算
static double apply_operation(double a, double b, char op) {
    switch (op) {
        case '+':
            return a + b;
        case '-':
            return a - b;
        case '*':
            return a * b;
        case '/':
            if (b == 0) {
                fprintf(stderr, "错误: 除以零\n");
                return 0;
            }
            return a / b;
        case '^':
            return pow(a, b);
        case '[':  // 开方：b 开 a 次方
            if (a <= 0) {
                fprintf(stderr, "错误: 根号运算的根指数必须大于0\n");
                return 0;
            }
            if (b < 0 && fmod(a, 2) == 0) {
                fprintf(stderr, "错误: 负数不能开偶次方\n");
                return 0;
            }
            return pow(b, 1.0 / a);
        default:
            return 0;
    }
}

// 帮助函数：计算表达式
static double evaluate_expression(const char *expr) {
    double numbers[100];
    char operators[100];
    int num_top = -1, op_top = -1;
    int i = 0;
    int negative = 0;

    while (expr[i] != '\0') {
        // 跳过空格
        if (isspace(expr[i])) {
            i++;
            continue;
        }

        // 处理负数
        if (expr[i] == '-' && (i == 0 || expr[i-1] == '(' || is_operator(expr[i-1]))) {
            negative = 1;
            i++;
            continue;
        }

        // 处理数字
        if (isdigit(expr[i]) || expr[i] == '.') {
            double num = 0;
            int decimal = 0;
            double fraction = 1.0;

            // 整数部分
            while (isdigit(expr[i])) {
                num = num * 10 + (expr[i] - '0');
                i++;
            }

            // 小数部分
            if (expr[i] == '.') {
                decimal = 1;
                i++;
                while (isdigit(expr[i])) {
                    fraction *= 0.1;
                    num += (expr[i] - '0') * fraction;
                    i++;
                }
            }

            // 应用负号
            if (negative) {
                num = -num;
                negative = 0;
            }

            // 将数字压入栈
            if (num_top < 99) {
                numbers[++num_top] = num;
            }
            continue;
        }

        // 处理开方运算符
        if (expr[i] == '[') {
            // 检查是否是二元开方运算
            if (num_top >= 0 && i > 0 && (isdigit(expr[i-1]) || expr[i-1] == ')')) {
                // 这是二元开方
                if (op_top < 99) {
                    operators[++op_top] = '[';
                }
                i++;
            } else {
                // 这是一元开方
                if (op_top < 99) {
                    operators[++op_top] = '[';
                }
                // 将2压入栈作为默认的根指数（平方根）
                if (num_top < 99) {
                    numbers[++num_top] = 2;
                }
                i++;
            }
            continue;
        }

        // 处理左括号
        if (expr[i] == '(') {
            if (op_top < 99) {
                operators[++op_top] = expr[i];
            }
            i++;
            continue;
        }

        // 处理右括号
        if (expr[i] == ')') {
            while (op_top >= 0 && operators[op_top] != '(') {
                if (num_top < 1) {
                    fprintf(stderr, "错误: 表达式无效\n");
                    return 0;
                }
                double b = numbers[num_top--];
                double a = numbers[num_top--];
                char op = operators[op_top--];
                double result = apply_operation(a, b, op);
                if (num_top < 99) {
                    numbers[++num_top] = result;
                }
            }
            if (op_top >= 0 && operators[op_top] == '(') {
                op_top--;  // 弹出左括号
            }
            i++;
            continue;
        }

        // 处理运算符
        if (is_operator(expr[i]) && expr[i] != '[') {
            while (op_top >= 0 && get_precedence(operators[op_top]) >= get_precedence(expr[i])) {
                if (num_top < 1) {
                    fprintf(stderr, "错误: 表达式无效\n");
                    return 0;
                }
                double b = numbers[num_top--];
                double a = numbers[num_top--];
                char op = operators[op_top--];
                double result = apply_operation(a, b, op);
                if (num_top < 99) {
                    numbers[++num_top] = result;
                }
            }
            if (op_top < 99) {
                operators[++op_top] = expr[i];
            }
            i++;
            continue;
        }

        // 未知字符
        fprintf(stderr, "错误: 无效字符 '%c'\n", expr[i]);
        return 0;
    }

    // 处理剩余的运算符
    while (op_top >= 0) {
        if (num_top < 1) {
            fprintf(stderr, "错误: 表达式无效\n");
            return 0;
        }
        double b = numbers[num_top--];
        double a = numbers[num_top--];
        char op = operators[op_top--];

        // 处理一元运算符
        if (op == '[' && num_top < 0) {
            // 一元开方
            double result = apply_operation(a, b, op);
            if (num_top < 99) {
                numbers[++num_top] = result;
            }
        } else {
            double result = apply_operation(a, b, op);
            if (num_top < 99) {
                numbers[++num_top] = result;
            }
        }
    }

    if (num_top != 0) {
        fprintf(stderr, "错误: 表达式无效\n");
        return 0;
    }

    return numbers[num_top];
}

// 简单方程解析函数 - 新版本
static int parse_equation_simple(const char *equation_str, double *a, double *b, double *c, FILE *out) {
    char eq_copy[256];
    int len = strlen(equation_str);
    int j = 0;

    // 复制并转换为小写
    for (int i = 0; i < len && j < 255; i++) {
        eq_copy[j++] = tolower(equation_str[i]);
    }
    eq_copy[j] = '\0';

    // 找到等号
    char *equal_sign = strchr(eq_copy, '=');
    if (!equal_sign) {
        fprintf(out, "错误: 方程必须包含等号\n");
        return 0;
    }

    *equal_sign = '\0';
    char *left_side = eq_copy;
    char *right_side = equal_sign + 1;

    // 初始化系数
    *a = 0.0;
    *b = 0.0;
    *c = 0.0;

    // 解析右侧，如果是0就忽略，如果不是0就移到左边
    double right_val = 0.0;
    if (right_side[0] != '\0') {
        char *endptr;
        right_val = strtod(right_side, &endptr);
        if (endptr == right_side) {
            // 解析失败
            right_val = 0.0;
        }
    }

    // 处理左边表达式
    char *ptr = left_side;
    double current_coeff = 1.0;
    int is_negative = 0;
    int has_digit = 0;
    double num = 0.0;
    int decimal_point = 0;
    double decimal_div = 1.0;

    // 解析整个表达式
    while (*ptr) {
        if (*ptr == '+' || *ptr == '-') {
            // 保存前一项
            is_negative = (*ptr == '-');
            ptr++;
            continue;
        }

        // 解析数字
        if (isdigit(*ptr) || *ptr == '.') {
            num = 0.0;
            decimal_point = 0;
            decimal_div = 1.0;
            has_digit = 1;

            while (isdigit(*ptr) || *ptr == '.') {
                if (*ptr == '.') {
                    decimal_point = 1;
                } else {
                    if (decimal_point) {
                        decimal_div *= 10.0;
                        num = num + (*ptr - '0') / decimal_div;
                    } else {
                        num = num * 10.0 + (*ptr - '0');
                    }
                }
                ptr++;
            }

            if (is_negative) {
                num = -num;
                is_negative = 0;
            }

            // 检查下一项是什么
            if (*ptr == 'x') {
                ptr++;  // 跳过'x'
                if (*ptr == '^' && *(ptr+1) == '2') {
                    // 这是x^2项
                    *a += num;
                    ptr += 2;  // 跳过'^2'
                } else {
                    // 这是x项
                    *b += num;
                }
            } else {
                // 常数项
                *c += num;
            }

            has_digit = 0;
            continue;
        }

        // 处理单个'x'
        if (*ptr == 'x') {
            double coeff = 1.0;
            if (is_negative) {
                coeff = -1.0;
                is_negative = 0;
            }

            ptr++;  // 跳过'x'

            if (*ptr == '^' && *(ptr+1) == '2') {
                // 这是x^2项
                *a += coeff;
                ptr += 2;  // 跳过'^2'
            } else {
                // 这是x项
                *b += coeff;
            }
            continue;
        }

        // 跳过未知字符
        ptr++;
    }

    // 从c中减去右侧的值
    *c -= right_val;

    return 1;
}

// 解一元一次方程
static int solve_linear_equation(double a, double b, FILE *out) {
    fprintf(out, "\n=== 实验功能：解一元一次方程 ===\n");
    fprintf(out, "方程: %.2fx + %.2f = 0\n", a, b);

    if (fabs(a) < 1e-10) {
        if (fabs(b) < 1e-10) {
            fprintf(out, "解: 无穷多解 (任意实数)\n");
        } else {
            fprintf(out, "解: 无解\n");
        }
    } else {
        double x = -b / a;
        fprintf(out, "解: x = %.10f\n", x);
    }

    fprintf(out, "====================================\n");
    return 1;
}

// 解一元二次方程
static int solve_quadratic_equation(double a, double b, double c, FILE *out) {
    fprintf(out, "\n=== 实验功能：解一元二次方程 ===\n");
    fprintf(out, "方程: %.2fx^2 + %.2fx + %.2f = 0\n", a, b, c);

    if (fabs(a) < 1e-10) {
        fprintf(out, "注意: a约等0，这实际上是一元一次方程\n");
        return solve_linear_equation(b, c, out);
    }

    // 计算判别式
    double delta = b * b - 4 * a * c;

    fprintf(out, "判别式: %.10f\n", delta);

    if (delta > 1e-10) {
        // 两个实数解
        double sqrt_delta = sqrt(delta);
        double x1 = (-b + sqrt_delta) / (2 * a);
        double x2 = (-b - sqrt_delta) / (2 * a);

        fprintf(out, "解: 两个不同的实数解\n");
        fprintf(out, "x1 = %.10f\n", x1);
        fprintf(out, "x2 = %.10f\n", x2);
    } else if (fabs(delta) < 1e-10) {
        // 一个实数解
        double x = -b / (2 * a);
        fprintf(out, "解: 一个实数解（重根）\n");
        fprintf(out, "x = %.10f\n", x);
    } else {
        // 无实数解
        delta = -delta;  // 取绝对值
        fprintf(out, "解: 无实数解（有两个共轭复数解）\n");
        double real_part = -b / (2 * a);
        double imag_part = sqrt(delta) / (2 * a);

        fprintf(out, "复数解:\n");
        if (fabs(real_part) < 1e-10) real_part = 0.0;
        if (fabs(imag_part) < 1e-10) imag_part = 0.0;

        fprintf(out, "x1 = %.6f + %.6fi\n", real_part, imag_part);
        fprintf(out, "x2 = %.6f - %.6fi\n", real_part, imag_part);
    }

    fprintf(out, "========================================\n");
    return 1;
}

// 主计算器函数
int cmd_calc(ShellContext *ctx, int argc, char *argv[], FILE *in, FILE *out) {
    (void)ctx;  // 未使用
    (void)in;   // 未使用

    if (argc < 2) {
        // 显示计算器帮助
        fprintf(out, "用法: calc [选项] <表达式>\n");
        fprintf(out, "  计算数学表达式或解方程\n");
        fprintf(out, "\n");
        fprintf(out, "选项:\n");
        fprintf(out, "  -D          计算模式（默认）\n");
        fprintf(out, "  -E          方程求解模式（实验功能）\n");
        fprintf(out, "\n");
        fprintf(out, "计算模式支持的操作:\n");
        fprintf(out, "  + - * /      加减乘除\n");
        fprintf(out, "  ^           乘方，如 2^3=8\n");
        fprintf(out, "  [           开方，如 2[4=2 (2次根号4)\n");
        fprintf(out, "               [4=2 (默认平方根)\n");
        fprintf(out, "               3[8=2 (3次根号8)\n");
        fprintf(out, "  ()          括号\n");
        fprintf(out, "\n");
        fprintf(out, "方程模式支持的格式:\n");
        fprintf(out, "  一元一次方程: ax + b = 0 或 ax = b\n");
        fprintf(out, "  一元二次方程: ax^2 + bx + c = 0\n");
        fprintf(out, "  注意: 系数必须明确写出，如 2x^2+3x-5=0\n");
        fprintf(out, "        支持省略系数: x^2+3x-5=0 (a=1)\n");
        fprintf(out, "        支持负号: -x^2+3x-5=0\n");
        fprintf(out, "\n");
        fprintf(out, "示例:\n");
        fprintf(out, "  calc 2+3 * 4           # 计算表达式\n");
        fprintf(out, "  calc -D 2+3 * 4        # 计算表达式\n");
        fprintf(out, "  calc -E 2x^2+3x-5=0 # 解一元二次方程\n");
        fprintf(out, "  calc -E 4x+2=0      # 解一元一次方程\n");
        fprintf(out, "  calc -E x^2-4=0     # 解方程\n");
        fprintf(out, "  calc -E x^2+2x+1=0  # 解方程（重根）\n");
        fprintf(out, "  calc -E x^2+2x+2=0  # 解方程（复数解）\n");
        return 0;
    }

    // 检查模式选项
    int mode = 0;  // 0=默认计算模式, 1=方程模式
    int start_arg = 1;

    if (argc >= 2 && argv[1][0] == '-') {
        if (strcmp(argv[1], "-D") == 0) {
            mode = 0;  // 计算模式
            start_arg = 2;
        } else if (strcmp(argv[1], "-E") == 0) {
            mode = 1;  // 方程模式
            start_arg = 2;
        } else {
            fprintf(out, "错误: 未知选项 '%s'\n", argv[1]);
            fprintf(out, "使用 'calc' 查看帮助\n");
            return 1;
        }
    }

    if (start_arg >= argc) {
        fprintf(out, "错误: 缺少表达式或方程\n");
        fprintf(out, "使用 'calc' 查看帮助\n");
        return 1;
    }

    // 合并所有参数为一个字符串
    char expression[1024] = "";
    for (int i = start_arg; i < argc; i++) {
        if (i > start_arg && expression[0] != '\0') {
            strcat(expression, " ");
        }
        strncat(expression, argv[i], sizeof(expression) - strlen(expression) - 1);
    }

    if (mode == 0) {
        // 计算模式
        fprintf(out, "计算表达式: %s\n", expression);
        double result = evaluate_expression(expression);

        // 检查是否是整数
        if (fabs(result - round(result)) < 1e-10) {
            fprintf(out, "结果: %.0f\n", result);
        } else {
            fprintf(out, "结果: %.10f\n", result);
        }
    } else {
        // 方程模式
        fprintf(out, "\n[实验功能] 方程求解模式\n");
        fprintf(out, "注意: 这是一个实验性功能\n");
        fprintf(out, "支持的方程格式:\n");
        fprintf(out, "  一元一次: ax + b = 0 或 ax = b\n");
        fprintf(out, "  一元二次: ax^2 + bx + c = 0\n");
        fprintf(out, "示例: 2x^2+3x-5=0, 4x+2=0, x^2-4=0\n\n");

        double a = 0.0, b = 0.0, c = 0.0;

        if (!parse_equation_simple(expression, &a, &b, &c, out)) {
            fprintf(out, "错误: 无法解析方程 '%s'\n", expression);
            fprintf(out, "请检查方程格式是否正确\n");
            fprintf(out, "正确格式: ax^2 + bx + c = 0 或 ax + b = 0\n");
            fprintf(out, "         (可以省略空格，x^2表示x的平方)\n");
            return 1;
        }

        fprintf(out, "解析结果: a=%.6f, b=%.6f, c=%.6f\n", a, b, c);

        // 检查是否二次方程
        int is_quadratic = 0;
        char eq_lower[256];

        // 转换为小写
        for (int i = 0; expression[i] && i < 255; i++) {
            eq_lower[i] = tolower(expression[i]);
        }
        eq_lower[255] = '\0';

        // 检查是否包含x^2
        if (strstr(eq_lower, "x^2") != NULL) {
            is_quadratic = 1;
        } else if (fabs(a) > 1e-10) {
            // 如果有a系数，认为是二次方程
            is_quadratic = 1;
        }

        if (is_quadratic) {
            solve_quadratic_equation(a, b, c, out);
        } else {
            solve_linear_equation(b, c, out);
        }
    }

    return 0;
}
