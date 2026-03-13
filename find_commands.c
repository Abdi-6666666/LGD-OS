// find_commands.c
#include "declarations.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#else
#include <dirent.h>
#include <unistd.h>
#endif

// 递归搜索目录
void find_recursive(const char *real_path, const char *search_pattern, 
                    int name_only, int type_mode, int print_count, 
                    int *found_count, FILE *out) {
#ifdef _WIN32
    // Windows版本的目录遍历
    WIN32_FIND_DATA find_data;
    char pattern[MAX_PATH_LEN];
    char full_path[MAX_PATH_LEN];
    HANDLE hFind;
    
    // 构建搜索模式
    snprintf(pattern, MAX_PATH_LEN, "%s\\*", real_path);
    
    hFind = FindFirstFile(pattern, &find_data);
    if (hFind == INVALID_HANDLE_VALUE) {
        return;
    }
    
    do {
        // 跳过 "." 和 ".."
        if (strcmp(find_data.cFileName, ".") == 0 || 
            strcmp(find_data.cFileName, "..") == 0) {
            continue;
        }
        
        // 构建完整路径
        snprintf(full_path, MAX_PATH_LEN, "%s\\%s", real_path, find_data.cFileName);
        
        // 检查是否是目录
        int is_dir = (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        
        // 检查文件名是否匹配
        int matches = 0;
        if (name_only) {
            // 只按名称匹配
            if (strstr(find_data.cFileName, search_pattern) != NULL) {
                matches = 1;
            }
        } else {
            // 精确匹配文件名
            if (strcmp(find_data.cFileName, search_pattern) == 0) {
                matches = 1;
            }
        }
        
        // 检查类型
        int type_ok = 1;
        if (type_mode != 0) {
            if (type_mode == 1 && !is_dir) {  // 只找目录
                type_ok = 0;
            } else if (type_mode == 2 && is_dir) {  // 只找文件
                type_ok = 0;
            }
        }
        
        // 打印匹配项
        if (matches && type_ok) {
            if (!print_count) {
                // 打印路径
                char virtual_path[MAX_PATH_LEN];
                real_to_virtual_path(full_path, virtual_path);
                fprintf(out, "%s\n", virtual_path);
            }
            (*found_count)++;
        }
        
        // 如果是目录，递归搜索
        if (is_dir) {
            find_recursive(full_path, search_pattern, name_only, 
                          type_mode, print_count, found_count, out);
        }
        
    } while (FindNextFile(hFind, &find_data) != 0);
    
    FindClose(hFind);
#else
    // Linux版本的目录遍历
    DIR *dir = opendir(real_path);
    if (dir == NULL) {
        return;
    }
    
    struct dirent *entry;
    struct stat statbuf;
    char full_path[MAX_PATH_LEN];
    
    while ((entry = readdir(dir)) != NULL) {
        // 跳过 "." 和 ".."
        if (strcmp(entry->d_name, ".") == 0 || 
            strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        
        // 构建完整路径
        snprintf(full_path, MAX_PATH_LEN, "%s/%s", real_path, entry->d_name);
        
        // 获取文件信息
        if (stat(full_path, &statbuf) == -1) {
            continue;
        }
        
        int is_dir = S_ISDIR(statbuf.st_mode);
        
        // 检查文件名是否匹配
        int matches = 0;
        if (name_only) {
            // 只按名称匹配
            if (strstr(entry->d_name, search_pattern) != NULL) {
                matches = 1;
            }
        } else {
            // 精确匹配文件名
            if (strcmp(entry->d_name, search_pattern) == 0) {
                matches = 1;
            }
        }
        
        // 检查类型
        int type_ok = 1;
        if (type_mode != 0) {
            if (type_mode == 1 && !is_dir) {  // 只找目录
                type_ok = 0;
            } else if (type_mode == 2 && is_dir) {  // 只找文件
                type_ok = 0;
            }
        }
        
        // 打印匹配项
        if (matches && type_ok) {
            if (!print_count) {
                // 打印路径
                char virtual_path[MAX_PATH_LEN];
                real_to_virtual_path(full_path, virtual_path);
                fprintf(out, "%s\n", virtual_path);
            }
            (*found_count)++;
        }
        
        // 如果是目录，递归搜索
        if (is_dir) {
            find_recursive(full_path, search_pattern, name_only, 
                          type_mode, print_count, found_count, out);
        }
    }
    
    closedir(dir);
#endif
}

// find命令的实现
int cmd_find(ShellContext *ctx, int argc, char *argv[], FILE *in, FILE *out) {
    char search_path[MAX_PATH_LEN];
    char search_pattern[MAX_PATH_LEN];
    int name_only = 0;  // 0=精确匹配, 1=部分匹配
    int type_mode = 0;  // 0=全部, 1=只目录, 2=只文件
    int print_count = 0; // 是否只显示数量
    int found_count = 0;
    int i = 1;
    
    // 默认搜索当前目录
    strcpy(search_path, ctx->current_dir);
    
    // 解析参数
    while (i < argc) {
        if (strcmp(argv[i], "-name") == 0) {
            if (i + 1 < argc) {
                strcpy(search_pattern, argv[i + 1]);
                i += 2;
            } else {
                fprintf(out, "错误: -name 参数需要指定模式\n");
                return 1;
            }
        } else if (strcmp(argv[i], "-iname") == 0) {
            if (i + 1 < argc) {
                strcpy(search_pattern, argv[i + 1]);
                name_only = 1;  // 部分匹配
                i += 2;
            } else {
                fprintf(out, "错误: -iname 参数需要指定模式\n");
                return 1;
            }
        } else if (strcmp(argv[i], "-type") == 0) {
            if (i + 1 < argc) {
                if (strcmp(argv[i + 1], "d") == 0) {
                    type_mode = 1;  // 只搜索目录
                } else if (strcmp(argv[i + 1], "f") == 0) {
                    type_mode = 2;  // 只搜索文件
                } else {
                    fprintf(out, "错误: -type 参数必须是 'd' (目录) 或 'f' (文件)\n");
                    return 1;
                }
                i += 2;
            } else {
                fprintf(out, "错误: -type 参数需要指定类型\n");
                return 1;
            }
        } else if (strcmp(argv[i], "-count") == 0) {
            print_count = 1;
            i++;
        } else if (argv[i][0] == '-') {
            fprintf(out, "错误: 未知选项 '%s'\n", argv[i]);
            fprintf(out, "用法: find [路径] [-name 模式] [-iname 模式] [-type d|f] [-count]\n");
            return 1;
        } else {
            // 这是搜索路径
            if (i == 1 || (i == 2 && strcmp(argv[1], "-count") == 0)) {
                // 处理虚拟路径
                char temp_path[MAX_PATH_LEN];
                if (argv[i][0] == '~' || argv[i][0] == '/' || strstr(argv[i], "..") != NULL) {
                    // 绝对路径或包含..，直接使用
                    strcpy(temp_path, argv[i]);
                } else {
                    // 相对路径
                    if (strcmp(ctx->current_dir, "/") == 0) {
                        snprintf(temp_path, MAX_PATH_LEN, "/%s", argv[i]);
                    } else {
                        snprintf(temp_path, MAX_PATH_LEN, "%s/%s", ctx->current_dir, argv[i]);
                    }
                }
                
                char real_temp_path[MAX_PATH_LEN];
                virtual_to_real_path(temp_path, real_temp_path);
                
                // 检查路径是否存在
                if (_access(real_temp_path, 0) == 0) {
                    strcpy(search_path, temp_path);
                } else {
                    fprintf(out, "警告: 路径 '%s' 不存在，使用当前目录\n", argv[i]);
                }
                i++;
            } else {
                fprintf(out, "错误: 参数顺序不正确\n");
                return 1;
            }
        }
    }
    
    // 检查是否指定了搜索模式
    if (search_pattern[0] == '\0') {
        fprintf(out, "错误: 必须指定搜索模式\n");
        fprintf(out, "用法: find [路径] -name 模式\n");
        fprintf(out, "       find [路径] -iname 模式\n");
        fprintf(out, "       find [路径] -type d|f\n");
        return 1;
    }
    
    // 将虚拟搜索路径转换为真实路径
    char real_search_path[MAX_PATH_LEN];
    virtual_to_real_path(search_path, real_search_path);
    
    // 检查路径是否存在
    if (_access(real_search_path, 0) != 0) {
        fprintf(out, "错误: 搜索路径 '%s' 不存在\n", search_path);
        return 1;
    }
    
    // 检查是否是目录
    struct stat statbuf;
    if (stat(real_search_path, &statbuf) != 0) {
        fprintf(out, "错误: 无法访问路径 '%s'\n", search_path);
        return 1;
    }
    
    if (!S_ISDIR(statbuf.st_mode)) {
        fprintf(out, "错误: '%s' 不是一个目录\n", search_path);
        return 1;
    }
    
    // 开始搜索
    find_recursive(real_search_path, search_pattern, name_only, 
                   type_mode, print_count, &found_count, out);
    
    // 如果只显示数量
    if (print_count) {
        fprintf(out, "找到 %d 个匹配项\n", found_count);
    } else if (found_count == 0) {
        fprintf(out, "没有找到匹配项\n");
    }
    
    return 0;
}