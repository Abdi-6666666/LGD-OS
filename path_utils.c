// path_utils.c
#include "declarations.h"

// 获取规范化虚拟路径
/*void get_normalized_virtual_path(const char *virtual_path, char *normalized_virtual) {
    char temp_path[MAX_PATH_LEN];
    char *token;
    char *tokens[100];
    int token_count = 0;

    // 复制虚拟路径
    strcpy(temp_path, virtual_path);

    // 分割路径
    token = strtok(temp_path, "/");
    while (token != NULL && token_count < 100) {
        tokens[token_count++] = token;
        token = strtok(NULL, "/");
    }

    // 处理 .. 和 .
    int i = 0;
    for (int j = 0; j < token_count; j++) {
        if (strcmp(tokens[j], "..") == 0) {
            if (i > 0) i--;
        } else if (strcmp(tokens[j], ".") != 0) {
            tokens[i++] = tokens[j];
        }
    }

    // 构建规范化路径
    normalized_virtual[0] = '\0';
    if (virtual_path[0] == '/') {
        strcat(normalized_virtual, "/");
    }

    for (int j = 0; j < i; j++) {
        strcat(normalized_virtual, tokens[j]);
        if (j < i - 1) {
            strcat(normalized_virtual, "/");
        }
    }

    if (i == 0 && normalized_virtual[0] == '\0') {
        strcpy(normalized_virtual, "/");
    }
}*/

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



// 将虚拟路径转换为真实路径
void virtual_to_real_path(const char *virtual_path, char *real_path) {
    int i;
    char temp_path[MAX_PATH_LEN];

    // 处理 ~
    if (strcmp(virtual_path, "~") == 0) {
        // 用户家目录
        snprintf(real_path, MAX_PATH_LEN, "%s\\home\\user", BUILDS_DIR);
    } else if (strncmp(virtual_path, "~/", 2) == 0) {
        // ~/path
        char temp[MAX_PATH_LEN];
        strcpy(temp, virtual_path + 2);

        // 规范化路径
        normalize_path(temp);

        if (strcmp(temp, "/") == 0) {
            snprintf(real_path, MAX_PATH_LEN, "%s\\home\\user", BUILDS_DIR);
        } else {
            const char *rel_path = (temp[0] == '/') ? temp + 1 : temp;
            snprintf(temp_path, MAX_PATH_LEN, "%s\\home\\user\\%s", BUILDS_DIR, rel_path);

            // 将路径中的/替换为
            for (i = 0; temp_path[i] != '\0'; i++) {
                if (temp_path[i] == '/') {
                    temp_path[i] = '\\';
                }
            }
            strcpy(real_path, temp_path);
        }
    } else {
        // 普通路径
        char normalized[MAX_PATH_LEN];
        strcpy(normalized, virtual_path);
        normalize_path(normalized);

        if (strcmp(normalized, "/") == 0) {
            strcpy(real_path, BUILDS_DIR);
        } else {
            const char *rel_path = (normalized[0] == '/') ? normalized + 1 : normalized;
            snprintf(temp_path, MAX_PATH_LEN, "%s\\%s", BUILDS_DIR, rel_path);

            // 将路径中的/替换为
            for (i = 0; temp_path[i] != '\0'; i++) {
                if (temp_path[i] == '/') {
                    temp_path[i] = '\\';
                }
            }
            strcpy(real_path, temp_path);
        }
    }
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

