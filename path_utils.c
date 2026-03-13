#include "declarations.h"
#include <string.h>
#include <stdio.h>

#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#include <io.h>
#define getcwd _getcwd
#define access _access
#else
#include <unistd.h>
#include <sys/stat.h>
#endif

// 简化路径（去除./和../） - 保持为static函数
static void simplify_path(char *path) {
    char parts[100][MAX_PATH_LEN];
    int part_count = 0;
    char *token;
    char temp[MAX_PATH_LEN];

    strcpy(temp, path);

    // 分割路径
    token = strtok(temp, "/");
    while (token != NULL) {
        if (strcmp(token, ".") == 0) {
            // 忽略当前目录
        } else if (strcmp(token, "..") == 0) {
            // 上级目录
            if (part_count > 0) {
                part_count--;
            }
        } else {
            strcpy(parts[part_count], token);
            part_count++;
        }
        token = strtok(NULL, "/");
    }

    // 重建路径
    if (part_count == 0) {
        strcpy(path, "/");
    } else {
        strcpy(path, "/");
        for (int i = 0; i < part_count; i++) {
            strcat(path, parts[i]);
            if (i < part_count - 1) {
                strcat(path, "/");
            }
        }
    }
}

// 将虚拟路径转换为真实路径
void virtual_to_real_path(const char *virtual_path, char *real_path) {
    char cwd[MAX_PATH_LEN];
    char temp_path[MAX_PATH_LEN];

    // 获取当前工作目录
    getcwd(cwd, MAX_PATH_LEN);

    // 复制虚拟路径到临时变量
    strcpy(temp_path, virtual_path);

    // 处理特殊路径
    if (strcmp(temp_path, "~") == 0) {
        strcpy(temp_path, "/home/user");
    } else if (strncmp(temp_path, "~/", 2) == 0) {
        // 处理 ~/path
        char temp[MAX_PATH_LEN];
        strcpy(temp, "/home/user");
        strcat(temp, temp_path + 1);
        strcpy(temp_path, temp);
    }

    // 确保路径不以/结尾（除非是根目录）
    int len = strlen(temp_path);
    if (len > 1 && temp_path[len-1] == '/') {
        temp_path[len-1] = '\0';
    }

    // 简化路径
    simplify_path(temp_path);

    // 构建真实路径
    snprintf(real_path, MAX_PATH_LEN, "%s\\builds%s", cwd, temp_path);

    // 将Unix路径分隔符转换为Windows风格
    for (int i = 0; real_path[i] != '\0'; i++) {
        if (real_path[i] == '/') {
            real_path[i] = '\\';
        }
    }
}

// 将真实路径转换为虚拟路径
void real_to_virtual_path(const char *real_path, char *virtual_path) {
    char cwd[MAX_PATH_LEN];
    char builds_path[MAX_PATH_LEN];
    const char *ptr = real_path;

    // 获取当前工作目录
    getcwd(cwd, MAX_PATH_LEN);

    // 构建builds目录的路径
    snprintf(builds_path, MAX_PATH_LEN, "%s\\builds", cwd);

    // 检查real_path是否以builds_path开头
    if (strstr(real_path, builds_path) == real_path) {
        // 跳过builds_path部分
        ptr = real_path + strlen(builds_path);

        // 如果路径以\\或/开头，再跳过
        if (ptr[0] == '\\' || ptr[0] == '/') {
            ptr++;
        }
    }

    // 构建虚拟路径
    if (strlen(ptr) == 0) {
        strcpy(virtual_path, "/");
    } else {
        // 将Windows路径分隔符转换为Unix风格
        int j = 0;
        for (int i = 0; ptr[i] != '\0' && j < MAX_PATH_LEN - 2; i++) {
            if (ptr[i] == '\\') {
                virtual_path[j++] = '/';
            } else {
                virtual_path[j++] = ptr[i];
            }
        }

        // 确保以/开头
        if (virtual_path[0] != '/') {
            // 向前移动一位
            for (int i = j; i > 0; i--) {
                virtual_path[i] = virtual_path[i-1];
            }
            virtual_path[0] = '/';
            j++;
        }

        virtual_path[j] = '\0';

        // 简化路径（去除./和../）
        simplify_path(virtual_path);
    }
}
// 规范化路径：解析 . 和 ..
void normalize_path(char *path) {
    char parts[MAX_ARGS][MAX_FILENAME];
    int part_count = 0;
    char temp_path[MAX_PATH_LEN];
    char result_path[MAX_PATH_LEN];
    int i;

    // 复制临时路径
    strcpy(temp_path, path);

    // 如果是空路径，返回根目录
    if (strlen(temp_path) == 0) {
        strcpy(path, "/");
        return;
    }

    // 如果路径是根目录，直接返回
    if (strcmp(temp_path, "/") == 0) {
        return;
    }

    // 跳过开头的/
    char *start = temp_path;
    if (start[0] == '/') {
        start++;
    }

    // 将路径分割成部分
    char *token = strtok(start, "/");
    while (token != NULL && part_count < MAX_ARGS) {
        strcpy(parts[part_count], token);
        part_count++;
        token = strtok(NULL, "/");
    }

    // 解析 . 和 ..
    int stack[MAX_ARGS];
    int stack_top = 0;

    for (i = 0; i < part_count; i++) {
        if (strcmp(parts[i], ".") == 0) {
            // 当前目录，忽略
            continue;
        } else if (strcmp(parts[i], "..") == 0) {
            // 上级目录
            if (stack_top > 0) {
                stack_top--;
            }
        } else if (strlen(parts[i]) > 0) {
            // 正常目录名
            if (stack_top < MAX_ARGS) {
                stack[stack_top] = i;
                stack_top++;
            }
        }
    }

    // 重新构建路径
    if (stack_top == 0) {
        strcpy(result_path, "/");
    } else {
        result_path[0] = '\0';
        for (i = 0; i < stack_top; i++) {
            strcat(result_path, "/");
            strcat(result_path, parts[stack[i]]);
        }
    }

    strcpy(path, result_path);
}







// 检查路径是否是目录
int is_directory(const char *path) {
    struct _finddata_t fileinfo;
    intptr_t handle = _findfirst(path, &fileinfo);

    if (handle == -1L) {
        return 0;  /* 文件不存在 */
    }

    int is_dir = (fileinfo.attrib & _A_SUBDIR) != 0;
    _findclose(handle);
    return is_dir;
}


// 检查路径是否是文件
int is_file(const char *path) {
    struct _finddata_t fileinfo;
    intptr_t handle = _findfirst(path, &fileinfo);

    if (handle == -1L) {
        return 0;  /* 文件不存在 */
    }

    int is_file = (fileinfo.attrib & _A_SUBDIR) == 0;
    _findclose(handle);
    return is_file;
}


// 辅助函数：移除字符串两端的引号
void remove_quotes(char *str) {
    int len = strlen(str);

    if (len >= 2) {
        if ((str[0] == '"' && str[len-1] == '"') ||
            (str[0] == '\'' && str[len-1] == '\'')) {
            // 移动字符串内容，去除引号
            for (int i = 0; i < len - 2; i++) {
                str[i] = str[i+1];
            }
            str[len-2] = '\0';
        }
    }
}

