#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <windows.h>
#include <ctype.h>

#define MAX_PIPE_COMMANDS 10
#define MAX_NAME_LEN 256
#define MAX_PATH_LEN 1024
#define MAX_INPUT 256
#define MAX_ARGS 32
#define MAX_HISTORY 50
#define SAVE_FILE "lgdos_save.txt"
#define BUILDS_DIR "bulids"
#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#include <direct.h>
#else
#include <unistd.h>
#endif

typedef enum {
    FILE_TYPE,
    DIR_TYPE
} EntryType;

typedef struct FileEntry {
    char name[MAX_NAME_LEN];
    char real_path[MAX_PATH_LEN];  
    EntryType type;
    char* content;      
    size_t size;        
    time_t create_time;
    time_t modify_time;
    struct FileEntry* parent;
    struct FileEntry* children;
    struct FileEntry* next;
} FileEntry;

typedef struct {
    FileEntry* root;
    FileEntry* current_dir;
    int file_count;
    int dir_count;
    char prev_dir[MAX_PATH_LEN];
    char dir_history[MAX_HISTORY][MAX_PATH_LEN];
    int history_count;
} FileSystem;

typedef struct {
    FileSystem* fs;
    int running;
    char username[32];
    char hostname[64];
} ShellContext;

typedef int (*CommandHandler)(ShellContext* ctx, int argc, char** argv, FILE* input, FILE* output);

typedef struct {
    const char* name;
    const char* description;
    const char* usage;
    const char* example;
    CommandHandler handler;
} CommandEntry;

typedef struct {
    char* argv[MAX_ARGS];
    int argc;
} ParsedCommand;

static CommandEntry command_table[];
static int cmd_grep_stdin(char* pattern, FILE* input, FILE* output, int show_line_numbers, int ignore_case);
static int cmd_grep_file(char* pattern, const char* filename, FILE* output,int show_line_numbers, int ignore_case);
static int cmd_wc_stdin(FILE* input, FILE* output, int count_lines, int count_words, int count_chars,const char* filename);
static int cmd_wc_file(const char* filename, int* lines, int* words, int* chars);
static int cmd_cat_stdin(FILE* input, FILE* output, int show_line_numbers);

static int cmd_cat_edit(ShellContext* ctx, const char* filename, FILE* input, FILE* output);
static int cmd_cat_info(ShellContext* ctx, const char* filename, FILE* input, FILE* output);
static int cmd_head_stdin(FILE* input, FILE* output, int lines);
static int cmd_head_file(ShellContext* ctx, const char* filename, FILE* input, FILE* output, int lines);
static int cmd_tail_stdin(FILE* input, FILE* output, int lines);
static int cmd_tail_file(ShellContext* ctx, const char* filename, FILE* input, FILE* output, int lines);
static int create_real_directory(const char* relative_path);
static void create_etc_passwd_file(FileEntry* etc_dir);
static void create_etc_hosts_file(FileEntry* etc_dir);
static void create_user_initial_files(FileEntry* user_dir);
static int ensure_builds_directory(void);
static char* read_real_file(const char* filepath);
static int edit_real_file(FileEntry* file);
static int show_real_file_info(FileEntry* file);
static int display_real_file_content(FileEntry* file, int show_line_numbers);
static char* read_real_file(const char* filepath); 
static int edit_file_content(FileEntry* file); 
static FileEntry* create_entry(const char* name, EntryType type);
static void free_entry(FileEntry* entry);
static FileEntry* find_child(FileEntry* parent, const char* name);
static void add_child(FileEntry* parent, FileEntry* child);
static int remove_child(FileEntry* parent, const char* name);
static void fs_push_history(FileSystem* fs, const char* path);
static char* decode_content(const char* encoded, size_t* out_size);
static void save_file_system_recursive(FILE* fp, FileEntry* entry, const char* base_path);
static void load_file_system_entry(FileSystem* fs, const char* line);
static void ensure_directory_path(FileSystem* fs, const char* path);
static int ensure_builds_directory();
static void print_formatted_time(FILETIME file_time);
static void sync_subdirectories(FileEntry* parent_dir, const char* real_path);
static int parse_pipeline(const char* input, ParsedCommand* commands, int max_commands);
static int parse_arguments(const char* input, char* args[MAX_ARGS]);
static int execute_pipeline(ShellContext* ctx, const char* input);
static int execute_command_with_pipe(ShellContext* ctx, int argc, char** argv, int input_fd, int output_fd);
static int cmd_cat_file(ShellContext* ctx, const char* filename, FILE* input, FILE* output, int show_line_numbers);
static int create_directory_if_not_exists_silent(const char* relative_path);
static void sync_directory_recursive_silent(FileSystem* fs, FileEntry* virtual_parent, const char* real_path);

FileSystem* fs_init(void);
void fs_destroy(FileSystem* fs);
char* fs_get_current_path(FileSystem* fs);
int fs_change_dir(FileSystem* fs, const char* path);
void fs_list_dir(FileSystem* fs, int detailed, int show_all);
void setup_sample_filesystem(FileSystem* fs);
void shell_execute_command(ShellContext* ctx, const char* input);
void sync_existing_directories_silent(FileSystem* fs);

int cmd_ls(ShellContext* ctx, int argc, char** argv, FILE* input, FILE* output);
int cmd_cd(ShellContext* ctx, int argc, char** argv, FILE* input, FILE* output);
int cmd_pwd(ShellContext* ctx, int argc, char** argv, FILE* input, FILE* output);
int cmd_mkdir(ShellContext* ctx, int argc, char** argv, FILE* input, FILE* output);
int cmd_rmdir(ShellContext* ctx, int argc, char** argv, FILE* input, FILE* output);
int cmd_rm(ShellContext* ctx, int argc, char** argv, FILE* input, FILE* output);
int cmd_mv(ShellContext* ctx, int argc, char** argv, FILE* input, FILE* output);
int cmd_cat(ShellContext* ctx, int argc, char** argv, FILE* input, FILE* output);
int cmd_touch(ShellContext* ctx, int argc, char** argv, FILE* input, FILE* output);
int cmd_edit(ShellContext* ctx, int argc, char** argv, FILE* input, FILE* output);
int cmd_info(ShellContext* ctx, int argc, char** argv, FILE* input, FILE* output);
int cmd_grep(ShellContext* ctx, int argc, char** argv, FILE* input, FILE* output);
int cmd_wc(ShellContext* ctx, int argc, char** argv, FILE* input, FILE* output);
int cmd_head(ShellContext* ctx, int argc, char** argv, FILE* input, FILE* output);
int cmd_tail(ShellContext* ctx, int argc, char** argv, FILE* input, FILE* output);
int cmd_clear(ShellContext* ctx, int argc, char** argv, FILE* input, FILE* output);
int cmd_exit(ShellContext* ctx, int argc, char** argv, FILE* input, FILE* output);
int cmd_help(ShellContext* ctx, int argc, char** argv, FILE* input, FILE* output);
int cmd_ll(ShellContext* ctx, int argc, char** argv, FILE* input, FILE* output);

static int execute_single_command(ShellContext* ctx, int argc, char** argv, FILE* input, FILE* output);
void shell_show_prompt(ShellContext* ctx);
void shell_run(ShellContext* ctx);
void sync_real_directories(FileSystem* fs);


void sync_existing_directories_silent(FileSystem* fs) {
    if (!fs || !fs->root) return;
    
    
    sync_directory_recursive_silent(fs, fs->root, BUILDS_DIR);
}

static void sync_directory_recursive_silent(FileSystem* fs, FileEntry* virtual_parent, const char* real_path) {
    char search_path[MAX_PATH_LEN];
    sprintf(search_path, "%s\\*", real_path);
    
    WIN32_FIND_DATA find_data;
    HANDLE hFind = FindFirstFile(search_path, &find_data);
    
    if (hFind == INVALID_HANDLE_VALUE) {
        return;
    }
    
    do {
        if (strcmp(find_data.cFileName, ".") == 0 || strcmp(find_data.cFileName, "..") == 0) {
            continue;
        }
        
        char full_real_path[MAX_PATH_LEN];
        sprintf(full_real_path, "%s\\%s", real_path, find_data.cFileName);
        
        
        FileEntry* existing = find_child(virtual_parent, find_data.cFileName);
        
        if (!existing) {
            if (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            
                FileEntry* dir = create_entry(find_data.cFileName, DIR_TYPE);
                if (dir) {
                    strcpy(dir->real_path, full_real_path);
                    dir->parent = virtual_parent;
                    add_child(virtual_parent, dir);
                    fs->dir_count++;
                    
                   
                    sync_directory_recursive_silent(fs, dir, full_real_path);
                }
            } else {
               
                FileEntry* file = create_entry(find_data.cFileName, FILE_TYPE);
                if (file) {
                    strcpy(file->real_path, full_real_path);
                    file->size = find_data.nFileSizeLow;
                    file->create_time = time(NULL);
                    file->modify_time = time(NULL);
                    file->parent = virtual_parent;
                    add_child(virtual_parent, file);
                    fs->file_count++;
                }
            }
        } else if (existing->type == DIR_TYPE) {
            
            sync_directory_recursive_silent(fs, existing, full_real_path);
        }
    } while (FindNextFile(hFind, &find_data) != 0);
    
    FindClose(hFind);
}
static int create_directory_if_not_exists_silent(const char* relative_path) {
    char full_path[MAX_PATH_LEN];
    sprintf(full_path, "%s\\%s", BUILDS_DIR, relative_path);
    
    DWORD attr = GetFileAttributes(full_path);
    if (attr != INVALID_FILE_ATTRIBUTES) {
        if (attr & FILE_ATTRIBUTE_DIRECTORY) {
            return 0;  
        } else {
            return 0;  
        }
    }
    
    if (CreateDirectory(full_path, NULL)) {
        return 1;
    } else {
        DWORD error = GetLastError();
        if (error == ERROR_ALREADY_EXISTS) {
            return 0;
        }
        return 0;
    }
}
static int cmd_grep_file(char* pattern, const char* filename, FILE* output,
                        int show_line_numbers, int ignore_case) {
    fprintf(output, "grep: 在文件 %s 中搜索 '%s'\n", filename, pattern);
    fprintf(output, "注意: 完整功能正在开发中\n");
    return 0;
}

static int cmd_wc_stdin(FILE* input, FILE* output, 
                       int count_lines, int count_words, int count_chars,
                       const char* filename) {
    fprintf(output, "wc: 统计标准输入\n");
    fprintf(output, "注意: 完整功能正在开发中\n");
    return 0;
}

static int cmd_wc_file(const char* filename, int* lines, int* words, int* chars) {
    *lines = *words = *chars = 0;
    return 1;
}

static int cmd_cat_stdin(FILE* input, FILE* output, int show_line_numbers) {
    fprintf(output, "cat: 从标准输入读取\n");
    fprintf(output, "注意: 完整功能正在开发中\n");
    return 0;
}


static int cmd_cat_edit(ShellContext* ctx, const char* filename, 
                       FILE* input, FILE* output) {
    fprintf(output, "cat -e: 编辑文件 %s\n", filename);
    fprintf(output, "注意: 完整功能正在开发中\n");
    return 0;
}

static int cmd_cat_info(ShellContext* ctx, const char* filename, 
                       FILE* input, FILE* output) {
    fprintf(output, "cat -c: 显示文件信息 %s\n", filename);
    fprintf(output, "注意: 完整功能正在开发中\n");
    return 0;
}

static int execute_single_command(ShellContext* ctx, int argc, char** argv, 
                                 FILE* input, FILE* output) {
    if (argc == 0) return 0;
    
    char* command_name = argv[0];
    CommandEntry* cmd = NULL;
    
    for (int i = 0; command_table[i].name != NULL; i++) {
        if (strcmp(command_name, command_table[i].name) == 0) {
            cmd = &command_table[i];
            break;
        }
    }
    
    if (cmd) {
        return cmd->handler(ctx, argc, argv, input, output);
    } else {
        fprintf(output, "%s: 未找到命令\n", command_name);
        return 1;
    }
}
int cmd_head(ShellContext* ctx, int argc, char** argv, FILE* input, FILE* output) {
    int lines = 10;  
    
    
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-n") == 0 && i + 1 < argc) {
            lines = atoi(argv[++i]);
        } else if (argv[i][0] == '-') {
            fprintf(output, "head: 无效选项 '%s'\n", argv[i]);
            return 1;
        }
    }
    
    char buffer[4096];
    int line_count = 0;
    
    while (fgets(buffer, sizeof(buffer), input) && line_count < lines) {
        fputs(buffer, output);
        line_count++;
    }
    
    return 0;
}

int cmd_tail(ShellContext* ctx, int argc, char** argv, FILE* input, FILE* output) {
    int lines = 10;  
    
    
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-n") == 0 && i + 1 < argc) {
            lines = atoi(argv[++i]);
        } else if (argv[i][0] == '-') {
            fprintf(output, "tail: 无效选项 '%s'\n", argv[i]);
            return 1;
        }
    }
    

    char* line_buffer[1000];
    int line_count = 0;
    
    for (int i = 0; i < 1000; i++) {
        line_buffer[i] = malloc(4096);
        if (!line_buffer[i]) break;
    }
    
    while (line_count < 1000 && fgets(line_buffer[line_count], 4096, input)) {
        line_count++;
    }
    
    
    int start = (line_count - lines > 0) ? (line_count - lines) : 0;
    for (int i = start; i < line_count; i++) {
        fputs(line_buffer[i], output);
    }
    
    
    for (int i = 0; i < 1000; i++) {
        free(line_buffer[i]);
    }
    
    return 0;
}

int cmd_wc(ShellContext* ctx, int argc, char** argv, FILE* input, FILE* output) {
    int count_lines = 0;
    int count_words = 0;
    int count_chars = 0;
    int show_all = 1;
    
   
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-l") == 0) {
            count_lines = 1;
            show_all = 0;
        } else if (strcmp(argv[i], "-w") == 0) {
            count_words = 1;
            show_all = 0;
        } else if (strcmp(argv[i], "-c") == 0) {
            count_chars = 1;
            show_all = 0;
        } else if (argv[i][0] == '-') {
            fprintf(output, "wc: 无效选项 '%s'\n", argv[i]);
            return 1;
        }
    }
    
    if (show_all) {
        count_lines = count_words = count_chars = 1;
    }
    
    
    if (argc == 1 || (argc == 2 && argv[1][0] == '-')) {
        return cmd_wc_stdin(input, output, count_lines, count_words, count_chars, NULL);
    }
    
    
    int total_lines = 0, total_words = 0, total_chars = 0;
    int file_count = 0;
    
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-') continue;  
        
        FileEntry* file = find_child(ctx->fs->current_dir, argv[i]);
        if (!file) {
            fprintf(output, "wc: 无法打开 '%s': 没有那个文件或目录\n", argv[i]);
            continue;
        }
        
        if (file->type != FILE_TYPE) {
            fprintf(output, "wc: %s: 是一个目录\n", argv[i]);
            continue;
        }
        
        int lines, words, chars;
        if (!cmd_wc_file(file->real_path, &lines, &words, &chars)) {
            fprintf(output, "wc: 无法读取文件 '%s'\n", argv[i]);
            continue;
        }
        
        fprintf(output, "%6d %6d %6d %s\n", lines, words, chars, argv[i]);
        
        total_lines += lines;
        total_words += words;
        total_chars += chars;
        file_count++;
    }
    
    if (file_count > 1) {
        fprintf(output, "%6d %6d %6d 总计\n", total_lines, total_words, total_chars);
    }
    
    return 0;
}
int cmd_grep(ShellContext* ctx, int argc, char** argv, FILE* input, FILE* output) {
    if (argc < 2) {
        fprintf(output, "grep: 缺少模式\n");
        fprintf(output, "用法: grep <模式> [文件...]\n");
        return 1;
    }
    
    char* pattern = argv[1];
    int show_line_numbers = 0;
    int ignore_case = 0;
    
    
    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "-n") == 0) {
            show_line_numbers = 1;
        } else if (strcmp(argv[i], "-i") == 0) {
            ignore_case = 1;
        } else if (argv[i][0] == '-') {
            fprintf(output, "grep: 无效选项 '%s'\n", argv[i]);
            return 1;
        }
    }
    
    
    if (argc == 2 || (argc == 3 && (show_line_numbers || ignore_case))) {
        return cmd_grep_stdin(pattern, input, output, show_line_numbers, ignore_case);
    }
    
    
    int result = 0;
    for (int i = 2; i < argc; i++) {
        if (argv[i][0] == '-') continue;  
        
        FileEntry* file = find_child(ctx->fs->current_dir, argv[i]);
        if (!file) {
            fprintf(output, "grep: 无法打开 '%s': 没有那个文件或目录\n", argv[i]);
            result = 1;
            continue;
        }
        
        if (file->type != FILE_TYPE) {
            fprintf(output, "grep: %s: 是一个目录\n", argv[i]);
            result = 1;
            continue;
        }
        
        if (!cmd_grep_file(pattern, file->real_path, output, 
                          show_line_numbers, ignore_case)) {
            result = 1;
        }
    }
    
    return result;
}

static int cmd_grep_stdin(char* pattern, FILE* input, FILE* output, 
                         int show_line_numbers, int ignore_case) {
    char buffer[4096];
    int line_num = 1;
    int found = 0;
    
    char pattern_lower[256];
    if (ignore_case) {
        strcpy(pattern_lower, pattern);
        for (int i = 0; pattern_lower[i]; i++) {
            pattern_lower[i] = tolower(pattern_lower[i]);
        }
    }
    
    while (fgets(buffer, sizeof(buffer), input)) {
        char search_buffer[4096];
        strcpy(search_buffer, buffer);
        
        if (ignore_case) {
            for (int i = 0; search_buffer[i]; i++) {
                search_buffer[i] = tolower(search_buffer[i]);
            }
        }
        
        if (strstr(search_buffer, ignore_case ? pattern_lower : pattern)) {
            if (show_line_numbers) {
                fprintf(output, "%d:%s", line_num, buffer);
            } else {
                fprintf(output, "%s", buffer);
            }
            found = 1;
        }
        
        line_num++;
    }
    
    return found ? 0 : 1;
}


static int cmd_cat_file(ShellContext* ctx, const char* filename, FILE* input, FILE* output, int show_line_numbers) {
    
    FileEntry* file = find_child(ctx->fs->current_dir, filename);
    if (!file) {
        fprintf(output, "cat: '%s': 没有那个文件或目录\n", filename);
        return 1;
    }
    
    if (file->type != FILE_TYPE) {
        fprintf(output, "cat: '%s': 是一个目录\n", filename);
        return 1;
    }
    
    
    char* content = read_real_file(file->real_path);
    if (!content) {
        fprintf(output, "cat: 无法读取文件 '%s'\n", filename);
        return 1;
    }
    
    
    if (show_line_numbers) {
        int line_num = 1;
        char* line_start = content;
        char* current = content;
        
        while (*current) {
            if (*current == '\n') {
                fprintf(output, "%6d  ", line_num++);
                fwrite(line_start, 1, current - line_start, output);
                fprintf(output, "\n");
                line_start = current + 1;
            }
            current++;
        }
        
        if (line_start < current) {
            fprintf(output, "%6d  ", line_num);
            fwrite(line_start, 1, current - line_start, output);
            fprintf(output, "\n");
        }
    } else {
        fprintf(output, "%s", content);
    }
    
    free(content);
    return 0;
}
static int execute_command_with_pipe(ShellContext* ctx, int argc, char** argv, int input_fd, int output_fd) {
    if (!ctx || argc < 1) return 0;
    
    char* command_name = argv[0];
    CommandEntry* cmd = NULL;
    
    for (int i = 0; command_table[i].name != NULL; i++) {
        if (strcmp(command_name, command_table[i].name) == 0) {
            cmd = &command_table[i];
            break;
        }
    }
    
    if (!cmd) {
        printf("%s: 未找到命令\n", command_name);
        return 0;
    }
    
   
    FILE* input = NULL;
    FILE* output = NULL;
    
    if (input_fd != 0) {
        input = fdopen(input_fd, "r");
    } else {
        input = stdin;
    }
    
    if (output_fd != 1) {
        output = fdopen(output_fd, "w");
    } else {
        output = stdout;
    }
    
    if (!input || !output) {
        printf("错误: 无法创建输入输出流\n");
        return 0;
    }
    
    
    int result = cmd->handler(ctx, argc, argv, input, output);
    
    
    fflush(output);
    
    
    if (input_fd != 0) fclose(input);
    if (output_fd != 1) fclose(output);
    
    return result;
}
int execute_pipeline(ShellContext* ctx, const char* input) {
    if (!ctx || !input) return 0;
    
    // 将输入复制到可修改的缓冲区
    char input_copy[MAX_INPUT];
    strcpy(input_copy, input);
    
    // 分割命令
    char* commands[MAX_PIPE_COMMANDS];
    int cmd_count = 0;
    
    char* token = strtok(input_copy, "|");
    while (token && cmd_count < MAX_PIPE_COMMANDS) {
        // 去除首尾空格
        char* start = token;
        char* end = start + strlen(start) - 1;
        
        // 跳过开头的空格
        while (*start == ' ' || *start == '\t') {
            start++;
        }
        
        // 跳过结尾的空格
        while (end > start && (*end == ' ' || *end == '\t' || *end == '\n')) {
            *end = '\0';
            end--;
        }
        
        
        if (start <= end) {
            commands[cmd_count] = malloc(strlen(start) + 1);
            if (commands[cmd_count]) {
                strcpy(commands[cmd_count], start);
                cmd_count++;
            }
        }
        
        token = strtok(NULL, "|");
    }
    
    if (cmd_count < 2) {
        fprintf(stderr, "错误: 管道需要至少两个命令\n");
        
        
        for (int i = 0; i < cmd_count; i++) {
            free(commands[i]);
        }
        return 0;
    }
    
    
    for (int i = 0; i < cmd_count; i++) {
        
        char* args[MAX_ARGS];
        int argc = 0;
        
        char cmd_copy[MAX_INPUT];
        strcpy(cmd_copy, commands[i]);
        
        char* arg = strtok(cmd_copy, " \t\n");
        while (arg && argc < MAX_ARGS - 1) {
            args[argc++] = arg;
            arg = strtok(NULL, " \t\n");
        }
        args[argc] = NULL;
        
        if (argc > 0) {
            
            int found = 0;
            for (int j = 0; command_table[j].name != NULL; j++) {
                if (strcmp(args[0], command_table[j].name) == 0) {
                    command_table[j].handler(ctx, argc, args, stdin, stdout);
                    found = 1;
                    break;
                }
            }
            
            if (!found) {
                fprintf(stderr, "%s: 未找到命令\n", args[0]);
            }
        }
    }
    
    
    for (int i = 0; i < cmd_count; i++) {
        free(commands[i]);
    }
    
    return 1;
}
static int parse_arguments(const char* input, char* args[MAX_ARGS]) {
    if (!input || !args) return 0;
    
    char input_copy[MAX_INPUT];
    strcpy(input_copy, input);
    
    int argc = 0;
    char* token = strtok(input_copy, " \t\n");
    
    while (token && argc < MAX_ARGS - 1) {
        args[argc++] = token;
        token = strtok(NULL, " \t\n");
    }
    
    args[argc] = NULL;
    return argc;
}
static int parse_pipeline(const char* input, ParsedCommand* commands, int max_commands) {
    if (!input || !commands) return 0;
    
    char input_copy[MAX_INPUT];
    strcpy(input_copy, input);
    
    int command_count = 0;
    char* token = strtok(input_copy, "|");
    
    while (token && command_count < max_commands) {
        
        char* start = token;
        char* end = token + strlen(token) - 1;
        
        while (*start == ' ' || *start == '\t') start++;
        while (end > start && (*end == ' ' || *end == '\t' || *end == '\n')) {
            *end = '\0';
            end--;
        }
        
        if (strlen(start) > 0) {
            
            char* args[MAX_ARGS];
            int argc = parse_arguments(start, args);
            
            if (argc > 0) {
                commands[command_count].argc = argc;
                for (int i = 0; i < argc; i++) {
                    commands[command_count].argv[i] = args[i];
                }
                commands[command_count].argv[argc] = NULL;
                command_count++;
            }
        }
        
        token = strtok(NULL, "|");
    }
    
    return command_count;
}

void sync_real_directories(FileSystem* fs) {
    if (!fs || !fs->root) return;
    
    char search_path[MAX_PATH_LEN];
    sprintf(search_path, "%s\\*", BUILDS_DIR);
    
    WIN32_FIND_DATA find_data;
    HANDLE hFind = FindFirstFile(search_path, &find_data);
    
    if (hFind == INVALID_HANDLE_VALUE) {
        return;
    }
    
    do {
        if (strcmp(find_data.cFileName, ".") == 0 || strcmp(find_data.cFileName, "..") == 0) {
            continue;
        }
        
        if (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            char dir_name[MAX_NAME_LEN];
            strcpy(dir_name, find_data.cFileName);
            
            
            FileEntry* existing = find_child(fs->root, dir_name);
            if (!existing) {
                
                FileEntry* new_dir = create_entry(dir_name, DIR_TYPE);
                if (new_dir) {
                    char real_path[MAX_PATH_LEN];
                    sprintf(real_path, "%s\\%s", BUILDS_DIR, dir_name);
                    strcpy(new_dir->real_path, real_path);
                    new_dir->parent = fs->root;
                    
                    add_child(fs->root, new_dir);
                    fs->dir_count++;
                    
                    printf("同步目录: /%s\n", dir_name);
                    
                    
                    sync_subdirectories(new_dir, real_path);
                }
            }
        }
    } while (FindNextFile(hFind, &find_data) != 0);
    
    FindClose(hFind);
}

static void sync_subdirectories(FileEntry* parent_dir, const char* real_path) {
    char search_path[MAX_PATH_LEN];
    sprintf(search_path, "%s\\*", real_path);
    
    WIN32_FIND_DATA find_data;
    HANDLE hFind = FindFirstFile(search_path, &find_data);
    
    if (hFind == INVALID_HANDLE_VALUE) {
        return;
    }
    
    do {
        if (strcmp(find_data.cFileName, ".") == 0 || strcmp(find_data.cFileName, "..") == 0) {
            continue;
        }
        
        if (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            char dir_name[MAX_NAME_LEN];
            strcpy(dir_name, find_data.cFileName);
            
            
            FileEntry* existing = find_child(parent_dir, dir_name);
            if (!existing) {
               
                FileEntry* new_dir = create_entry(dir_name, DIR_TYPE);
                if (new_dir) {
                    char child_real_path[MAX_PATH_LEN];
                    sprintf(child_real_path, "%s\\%s", real_path, dir_name);
                    strcpy(new_dir->real_path, child_real_path);
                    new_dir->parent = parent_dir;
                    
                    add_child(parent_dir, new_dir);
                    
                    
                    sync_subdirectories(new_dir, child_real_path);
                }
            }
        }
    } while (FindNextFile(hFind, &find_data) != 0);
    
    FindClose(hFind);
}
static void print_formatted_time(FILETIME file_time) {
    FILETIME local_ft;
    SYSTEMTIME sys_time;
    
    
    if (FileTimeToLocalFileTime(&file_time, &local_ft) && 
        FileTimeToSystemTime(&local_ft, &sys_time)) {
       
        if (sys_time.wYear >= 1945 && sys_time.wYear <= 2100) {
            printf("%04d-%02d-%02d %02d:%02d:%02d",
                   sys_time.wYear, sys_time.wMonth, sys_time.wDay,
                   sys_time.wHour, sys_time.wMinute, sys_time.wSecond);
        } else {
            
            time_t now = time(NULL);
            struct tm* tm_info = localtime(&now);
            printf("%04d-%02d-%02d %02d:%02d:%02d",
                   tm_info->tm_year + 1900, tm_info->tm_mon + 1, tm_info->tm_mday,
                   tm_info->tm_hour, tm_info->tm_min, tm_info->tm_sec);
        }
    } else {
        
        time_t now = time(NULL);
        struct tm* tm_info = localtime(&now);
        printf("%04d-%02d-%02d %02d:%02d:%02d",
               tm_info->tm_year + 1900, tm_info->tm_mon + 1, tm_info->tm_mday,
               tm_info->tm_hour, tm_info->tm_min, tm_info->tm_sec);
    }
}
static void create_etc_hosts_file(FileEntry* etc_dir) {
    if (!etc_dir || etc_dir->type != DIR_TYPE) return;
    
    char hosts_path[MAX_PATH_LEN];
    sprintf(hosts_path, "%s\\hosts", etc_dir->real_path);
    
    printf("创建文件: /etc/hosts\n");
    HANDLE hFile = CreateFile(hosts_path, GENERIC_WRITE, 0, NULL, 
                            CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        printf("无法创建/etc/hosts文件: Windows错误 %lu\n", GetLastError());
        return;
    }
    
    const char* hosts_content = 
        "# LGD OS 1.1.0 Hosts文件\n"
        "# sse_107(C)2025-2026 保留所有权利\n"
        "\n"
        "127.0.0.1       localhost\n"
        "127.0.1.1       lgd-os\n"
        "\n"
        "# 以下为虚拟地址\n"
        "192.168.1.100   server1\n"
        "192.168.1.101   server2\n"
        "192.168.1.102   server3\n"
        "\n"
        "# IPv6地址\n"
        "::1             localhost ip6-localhost ip6-loopback\n"
        "ff02::1         ip6-allnodes\n"
        "ff02::2         ip6-allrouters\n";
    
    DWORD bytesWritten;
    if (!WriteFile(hFile, hosts_content, strlen(hosts_content), &bytesWritten, NULL)) {
        printf("无法写入/etc/hosts文件: Windows错误 %lu\n", GetLastError());
    } else {
        printf("  -> 已创建/etc/hosts文件\n");
    }
    
    CloseHandle(hFile);
    
    
    FileEntry* hosts = create_entry("hosts", FILE_TYPE);
    if (hosts) {
        strcpy(hosts->real_path, hosts_path);
        add_child(etc_dir, hosts);
    }
}
static void create_etc_passwd_file(FileEntry* etc_dir) {
    if (!etc_dir || etc_dir->type != DIR_TYPE) return;
    
    char passwd_path[MAX_PATH_LEN];
    sprintf(passwd_path, "%s\\passwd", etc_dir->real_path);
    
    printf("创建文件: /etc/passwd\n");
    HANDLE hFile = CreateFile(passwd_path, GENERIC_WRITE, 0, NULL, 
                            CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        printf("无法创建/etc/passwd文件: Windows错误 %lu\n", GetLastError());
        return;
    }
    
    const char* passwd_content = 
        "root:x:0:0:root:/root:/bin/bash\n"
        "user:x:1000:1000:user:/home/user:/bin/bash\n"
        "admin:x:1001:1001:Administrator:/home/admin:/bin/bash\n"
        "guest:x:1002:1002:Guest:/home/guest:/bin/bash\n";
    
    DWORD bytesWritten;
    if (!WriteFile(hFile, passwd_content, strlen(passwd_content), &bytesWritten, NULL)) {
        printf("无法写入/etc/passwd文件: Windows错误 %lu\n", GetLastError());
    } else {
        printf("  -> 已创建/etc/passwd文件\n");
    }
    
    CloseHandle(hFile);
    
    
    FileEntry* passwd = create_entry("passwd", FILE_TYPE);
    if (passwd) {
        strcpy(passwd->real_path, passwd_path);
        add_child(etc_dir, passwd);
    }
}
static int create_real_directory(const char* relative_path) {
    if (!relative_path) return 0;
    
    char full_path[MAX_PATH_LEN];
    sprintf(full_path, "%s\\%s", BUILDS_DIR, relative_path);
    
    
    DWORD attr = GetFileAttributes(full_path);
    if (attr != INVALID_FILE_ATTRIBUTES) {
        if (attr & FILE_ATTRIBUTE_DIRECTORY) {
            return 1;  
        } else {
            printf("错误: %s 已存在但不是目录\n", full_path);
            return 0;
        }
    }
    
    
    if (CreateDirectory(full_path, NULL)) {
        printf("创建目录: /%s\n", relative_path);
        return 1;
    } else {
        DWORD error = GetLastError();
        if (error == ERROR_ALREADY_EXISTS) {
            return 1;
        } else if (error == ERROR_PATH_NOT_FOUND) {
            
            char parent_path[MAX_PATH_LEN];
            char* last_slash = strrchr(relative_path, '\\');
            if (last_slash) {
                int len = last_slash - relative_path;
                strncpy(parent_path, relative_path, len);
                parent_path[len] = '\0';
                
                if (create_real_directory(parent_path)) {
                    
                    if (CreateDirectory(full_path, NULL)) {
                        printf("创建目录: /%s\n", relative_path);
                        return 1;
                    }
                }
            }
        }
        
        printf("无法创建目录 /%s: Windows错误 %lu\n", relative_path, error);
        return 0;
    }
}

static char* read_real_file(const char* filepath) {
    HANDLE hFile = CreateFile(filepath, GENERIC_READ, FILE_SHARE_READ, NULL, 
                             OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        return NULL;
    }
    
    DWORD fileSize = GetFileSize(hFile, NULL);
    if (fileSize == INVALID_FILE_SIZE) {
        CloseHandle(hFile);
        return NULL;
    }
    
    char* content = malloc(fileSize + 1);
    if (!content) {
        CloseHandle(hFile);
        return NULL;
    }
    
    DWORD bytesRead;
    if (!ReadFile(hFile, content, fileSize, &bytesRead, NULL)) {
        free(content);
        CloseHandle(hFile);
        return NULL;
    }
    
    content[fileSize] = '\0';
    CloseHandle(hFile);
    
    return content;
}
static int edit_real_file(FileEntry* file) {
    if (!file || file->type != FILE_TYPE) return 0;
    
    printf("正在编辑文件: %s\n", file->name);
    printf("真实路径: %s\n", file->real_path);
    printf("----------------------------------------\n");
    
    
    char* content = read_real_file(file->real_path);
    if (content) {
        printf("当前内容:\n%s\n", content);
        free(content);
    } else {
        printf("(空文件或无法读取)\n");
    }
    
    printf("----------------------------------------\n\n");
    printf("输入新内容 (输入单独一行 . 结束，输入 .! 放弃保存):\n");
    
    char** lines = NULL;
    int line_count = 0;
    int capacity = 10;
    
    lines = malloc(capacity * sizeof(char*));
    if (!lines) {
        printf("内存不足，无法编辑文件\n");
        return 0;
    }
    
    char buffer[1024];
    int save_changes = 1;
    
    while (1) {
        printf("[%d] ", line_count + 1);
        
        if (!fgets(buffer, sizeof(buffer), stdin)) {
            printf("读取输入失败\n");
            save_changes = 0;
            break;
        }
        
        buffer[strcspn(buffer, "\n")] = 0;
        
        if (strcmp(buffer, ".") == 0) {
            break;
        }
        
        if (strcmp(buffer, ".!") == 0) {
            printf("放弃保存，保留原内容\n");
            save_changes = 0;
            break;
        }
        
        if (line_count >= capacity) {
            capacity *= 2;
            char** new_lines = realloc(lines, capacity * sizeof(char*));
            if (!new_lines) {
                printf("内存不足，编辑被中断\n");
                save_changes = 0;
                break;
            }
            lines = new_lines;
        }
        
        lines[line_count] = malloc(strlen(buffer) + 2);
        if (!lines[line_count]) {
            printf("内存不足，无法保存此行\n");
            save_changes = 0;
            break;
        }
        
        strcpy(lines[line_count], buffer);
        strcat(lines[line_count], "\n");
        line_count++;
    }
    
    if (save_changes) {
        HANDLE hFile = CreateFile(file->real_path, GENERIC_WRITE, 0, NULL, 
                                 CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile == INVALID_HANDLE_VALUE) {
            printf("无法打开文件进行写入: Windows错误 %lu\n", GetLastError());
            save_changes = 0;
        } else {
            DWORD bytes_written;
            for (int i = 0; i < line_count; i++) {
                WriteFile(hFile, lines[i], strlen(lines[i]), &bytes_written, NULL);
            }
            CloseHandle(hFile);
            
            printf("文件已保存 (%d 行)\n", line_count);
            
            
            file->modify_time = time(NULL);
        }
    }
    
    for (int i = 0; i < line_count; i++) {
        free(lines[i]);
    }
    free(lines);
    
    return save_changes;
}
static int ensure_builds_directory() {
    DWORD file_attr = GetFileAttributes(BUILDS_DIR);
    if (file_attr == INVALID_FILE_ATTRIBUTES) {
        if (CreateDirectory(BUILDS_DIR, NULL)) {
            printf("创建 %s 目录\n", BUILDS_DIR);
            return 1;
        } else {
            printf("无法创建 %s 目录: Windows错误 %lu\n", BUILDS_DIR, GetLastError());
            return 0;
        }
    } else if (file_attr & FILE_ATTRIBUTE_DIRECTORY) {
        return 1;
    } else {
        printf("错误: %s 已存在但不是目录\n", BUILDS_DIR);
        return 0;
    }
}
static CommandEntry command_table[] = {
	{"ll", "ls -l 的别名", "ll [选项]", "ll -a", cmd_ll},
    {"ls", "列出目录内容", 
     "ls [选项] [目录]\n   选项:\n     -l        详细列表\n     -a        显示所有文件\n     -la/-al   详细列表显示所有文件", 
     "ls\nls -l\nls | cat\nls | grep txt", 
     cmd_ls},
    
    {"cat", "查看和编辑文本文件", 
     "cat [选项] <文件名>\n   选项:\n     -e        编辑文件内容\n     -n        显示行号\n     -c        显示文件信息", 
     "cat file.txt\ncat file.txt | grep hello\nls | cat -n", 
     cmd_cat},
    
    {"grep", "搜索文本", 
     "grep [选项] <模式> [文件...]\n   选项:\n     -n        显示行号\n     -i        忽略大小写", 
     "grep hello file.txt\nls | grep txt\ncat file.txt | grep -n error", 
     cmd_grep},
    
    {"wc", "统计字符、单词、行数", 
     "wc [选项] [文件...]\n   选项:\n     -l        只统计行数\n     -w        只统计单词数\n     -c        只统计字符数", 
     "wc file.txt\nls | wc -l\ncat file.txt | wc", 
     cmd_wc},
    
    {"head", "显示文件开头", 
     "head [-n 行数] [文件...]", 
     "head file.txt\nhead -n 5 file.txt\nls | head -5", 
     cmd_head},
    
    {"tail", "显示文件末尾", 
     "tail [-n 行数] [文件...]", 
     "tail file.txt\ntail -n 5 file.txt\nls | tail -5", 
     cmd_tail},
    
    {"cd", "改变当前工作目录", 
     "cd [目录路径]", 
     "cd /home\ncd ..\ncd -", 
     cmd_cd},
    
    {"pwd", "显示当前工作目录", 
     "pwd", 
     "pwd", 
     cmd_pwd},
    
    {"mkdir", "创建目录", 
     "mkdir <目录名>", 
     "mkdir test", 
     cmd_mkdir},
    
    {"rmdir", "删除空目录", 
     "rmdir <目录名>", 
     "rmdir test", 
     cmd_rmdir},
    
    {"rm", "删除文件", 
     "rm <文件名>", 
     "rm file.txt", 
     cmd_rm},
    
    {"mv", "移动或重命名文件", 
     "mv <源> <目标>", 
     "mv old.txt new.txt", 
     cmd_mv},
    
    {"touch", "创建空文件", 
     "touch <文件>", 
     "touch newfile.txt", 
     cmd_touch},
    
    {"edit", "编辑文件", 
     "edit <文件名>", 
     "edit file.txt", 
     cmd_edit},
    
    {"info", "显示文件信息", 
     "info <文件/目录名>", 
     "info file.txt", 
     cmd_info},
    
    {"clear", "清屏", 
     "clear", 
     "clear", 
     cmd_clear},
    
    {"cls", "清屏", 
     "cls", 
     "cls", 
     cmd_clear},
    
    {"exit", "退出系统", 
     "exit", 
     "exit", 
     cmd_exit},
    
    {"quit", "退出系统", 
     "quit", 
     "quit", 
     cmd_exit},
    
    {"help", "显示帮助信息", 
     "help [命令名] 或 help all", 
     "help\nhelp ls\nhelp all", 
     cmd_help},
    
    {NULL, NULL, NULL, NULL, NULL}  
};
int cmd_touch(ShellContext* ctx, int argc, char** argv, FILE* input, FILE* output) {
    if (argc < 2) {
        printf("touch: 缺少操作数\n");
        printf("用法: touch <文件名> [内容]\n");
        return 1;
    }
    
    for (int i = 1; i < argc; i++) {
        char file_name[MAX_NAME_LEN];
        strcpy(file_name, argv[i]);
        
        
        char current_real_path[MAX_PATH_LEN];
        if (ctx->fs->current_dir == ctx->fs->root) {
            strcpy(current_real_path, BUILDS_DIR);
        } else {
            strcpy(current_real_path, ctx->fs->current_dir->real_path);
        }
        
        
        char full_real_path[MAX_PATH_LEN];
        sprintf(full_real_path, "%s\\%s", current_real_path, file_name);
        
        
        DWORD attr = GetFileAttributes(full_real_path);
        if (attr != INVALID_FILE_ATTRIBUTES) {
            
            HANDLE hFile = CreateFile(full_real_path, GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 
                                      FILE_ATTRIBUTE_NORMAL, NULL);
            if (hFile != INVALID_HANDLE_VALUE) {
                SetFileTime(hFile, NULL, NULL, NULL);
                CloseHandle(hFile);
                printf("已更新文件时间: %s\n", file_name);
            }
            continue;
        }
        
        
        HANDLE hFile = CreateFile(full_real_path, GENERIC_WRITE, 0, NULL, CREATE_NEW, 
                                  FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile == INVALID_HANDLE_VALUE) {
            printf("touch: 无法创建文件 '%s': Windows错误 %lu\n", file_name, GetLastError());
            continue;
        }
        
        
        if (i + 1 < argc && argv[i + 1][0] != '-') {
            DWORD bytes_written;
            WriteFile(hFile, argv[i + 1], strlen(argv[i + 1]), &bytes_written, NULL);
            i++;
        }
        
        CloseHandle(hFile);
        
        
        FileEntry* file = create_entry(file_name, FILE_TYPE);
        if (!file) {
            printf("touch: 内存不足\n");
            DeleteFile(full_real_path);
            continue;
        }
        
        strcpy(file->real_path, full_real_path);
        file->parent = ctx->fs->current_dir;
        
        add_child(ctx->fs->current_dir, file);
        ctx->fs->file_count++;
        
        printf("已创建文件: %s \n", file_name, full_real_path);
    }
    
    return 0;
}
int cmd_edit(ShellContext* ctx, int argc, char** argv, FILE* input, FILE* output) {
    if (argc < 2) {
        fprintf(output, "edit: 缺少文件名\n");
        return 1;
    }
    
    
    char* new_argv[] = {"cat", "-e", argv[1], NULL};
    return cmd_cat(ctx, 3, new_argv, input, output);
}
int cmd_cat(ShellContext* ctx, int argc, char** argv, FILE* input, FILE* output) {
    int show_line_numbers = 0;
    int edit_mode = 0;
    int show_info = 0;
    char* filename = NULL;
    
    
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-n") == 0) {
            show_line_numbers = 1;
        } else if (strcmp(argv[i], "-e") == 0) {
            edit_mode = 1;
        } else if (strcmp(argv[i], "-c") == 0) {
            show_info = 1;
        } else if (argv[i][0] == '-') {
            fprintf(output, "cat: 无效选项 '%s'\n", argv[i]);
            return 1;
        } else {
            filename = argv[i];
        }
    }
    
    if (edit_mode) {
        
        if (!filename) {
            fprintf(output, "cat: 编辑模式需要文件名\n");
            return 1;
        }
        return cmd_cat_edit(ctx, filename, input, output);
    }
    
    if (show_info) {
        
        if (!filename) {
            fprintf(output, "cat: 显示文件信息需要文件名\n");
            return 1;
        }
        return cmd_cat_info(ctx, filename, input, output);
    }
    
   
    if (!filename) {
        return cmd_cat_stdin(input, output, show_line_numbers);
    }
    
    
    return cmd_cat_file(ctx, filename, input, output, show_line_numbers);
}
static int edit_file_content(FileEntry* file) {
    if (!file || file->type != FILE_TYPE) return 0;
    
    printf("正在编辑文件: %s\n", file->name);
    printf("当前内容 (共 %zu 字节):\n", file->size);
    printf("----------------------------------------\n");
    if (file->content && file->size > 0) {
        printf("%s\n", file->content);
    } else {
        printf("(空文件)\n");
    }
    printf("----------------------------------------\n\n");
    
    printf("输入新内容 (输入单独一行 . 结束):\n");
    
    char buffer[MAX_INPUT];
    char* content = NULL;
    size_t content_size = 0;
    size_t capacity = 1024;
    
    
    content = malloc(capacity);
    if (!content) {
        printf("内存不足，无法编辑文件\n");
        return 0;
    }
    content[0] = '\0';
    
    while (1) {
        if (fgets(buffer, sizeof(buffer), stdin) == NULL) {
            break;
        }
        
        
        buffer[strcspn(buffer, "\n")] = 0;
        
        
        if (strcmp(buffer, ".") == 0) {
            break;
        }
        
        
        size_t line_len = strlen(buffer);
        size_t new_size = content_size + line_len + 2; 
        
        
        if (new_size >= capacity) {
            capacity = capacity * 2;
            char* new_content = realloc(content, capacity);
            if (!new_content) {
                printf("内存不足，编辑被中断\n");
                free(content);
                return 0;
            }
            content = new_content;
        }
        
        
        if (content_size > 0) {
            strcat(content + content_size, "\n");
            content_size++;
        }
        strcat(content + content_size, buffer);
        content_size += line_len;
    }
    
   
    if (file->content) {
        free(file->content);
    }
    
    
    file->content = content;
    file->size = content_size;
    file->modify_time = time(NULL);
    
    printf("文件已保存 (%zu 字节)\n", content_size);
    return 1;
}
static int remove_child(FileEntry* parent, const char* name) {
    if (!parent || parent->type != DIR_TYPE) return 0;
    
    FileEntry* prev = NULL;
    FileEntry* current = parent->children;
    
    while (current) {
        if (strcmp(current->name, name) == 0) {
            if (prev) prev->next = current->next;
            else parent->children = current->next;
            
            free_entry(current);
            return 1;
        }
        prev = current;
        current = current->next;
    }
    return 0;
}
int cmd_rm(ShellContext* ctx, int argc, char** argv, FILE* input, FILE* output){
    if (argc < 2) {
        printf("rm: 缺少操作数\n");
        printf("用法: rm <文件名>\n");
        return 1;
    }
    
    for (int i = 1; i < argc; i++) {
        FileEntry* file = find_child(ctx->fs->current_dir, argv[i]);
        if (!file) {
            printf("rm: 无法删除 '%s': 没有那个文件或目录\n", argv[i]);
            continue;
        }
        
        if (file->type != FILE_TYPE) {
            printf("rm: 无法删除 '%s': 是一个目录\n", argv[i]);
            continue;
        }
        
        
        if (!DeleteFile(file->real_path)) {
            printf("rm: 无法删除 '%s': Windows错误 %lu\n", argv[i], GetLastError());
            continue;
        }
        
        if (remove_child(ctx->fs->current_dir, argv[i])) {
            ctx->fs->file_count--;
            printf("已删除文件: %s (真实路径: %s)\n", argv[i], file->real_path);
        } else {
            printf("rm: 无法删除 '%s': 未知错误\n", argv[i]);
        }
    }
    
    return 0;
}
static FileEntry* create_entry(const char* name, EntryType type) {
    FileEntry* entry = malloc(sizeof(FileEntry));
    if (!entry) return NULL;
    
    memset(entry, 0, sizeof(FileEntry));
    strcpy(entry->name, name);
    strcpy(entry->real_path, "");
    entry->type = type;
    entry->content = NULL;  
    entry->size = 0;        
    entry->create_time = time(NULL);
    entry->modify_time = entry->create_time;
    entry->parent = NULL;
    entry->children = NULL;
    entry->next = NULL;
    
    return entry;
}
static void free_entry(FileEntry* entry) {
    if (!entry) return;
    
    if (entry->content) free(entry->content);
    
    FileEntry* child = entry->children;
    while (child) {
        FileEntry* next = child->next;
        free_entry(child);
        child = next;
    }
    
    free(entry);
}

static FileEntry* find_child(FileEntry* parent, const char* name) {
    if (!parent || parent->type != DIR_TYPE) return NULL;
    
    FileEntry* child = parent->children;
    while (child) {
        if (strcmp(child->name, name) == 0) return child;
        child = child->next;
    }
    return NULL;
}

static void add_child(FileEntry* parent, FileEntry* child) {
    if (!parent || parent->type != DIR_TYPE) return;
    
    child->parent = parent;
    child->next = parent->children;
    parent->children = child;
}

static void fs_push_history(FileSystem* fs, const char* path) {
    if (fs->history_count >= MAX_HISTORY) {
        for (int i = 0; i < MAX_HISTORY - 1; i++) {
            strcpy(fs->dir_history[i], fs->dir_history[i + 1]);
        }
        fs->history_count--;
    }
    strcpy(fs->dir_history[fs->history_count++], path);
}

static FileEntry* fs_find_entry_by_path(FileSystem* fs, const char* path) {
    if (!path || strlen(path) == 0) return fs->current_dir;
    if (strcmp(path, "/") == 0) return fs->root;
    if (strcmp(path, ".") == 0) return fs->current_dir;
    if (strcmp(path, "..") == 0) return fs->current_dir->parent ? fs->current_dir->parent : fs->root;
    
    FileEntry* current = fs->root;
    char temp_path[MAX_PATH_LEN];
    strcpy(temp_path, path);
    
    if (temp_path[0] == '/') {
        current = fs->root;
        memmove(temp_path, temp_path + 1, strlen(temp_path));
    } else {
        current = fs->current_dir;
    }
    
    char* token = strtok(temp_path, "/");
    while (token && current) {
        current = find_child(current, token);
        token = strtok(NULL, "/");
    }
    
    return current;
}

FileSystem* fs_init() {
    FileSystem* fs = malloc(sizeof(FileSystem));
    if (!fs) return NULL;
    
    memset(fs, 0, sizeof(FileSystem));
    fs->root = create_entry("", DIR_TYPE);
    if (!fs->root) {
        free(fs);
        return NULL;
    }
    strcpy(fs->root->name, "/");
    
    fs->current_dir = fs->root;
    fs->file_count = 0;
    fs->dir_count = 1;
    
    return fs;
}

void fs_destroy(FileSystem* fs) {
    if (!fs) return;
    free_entry(fs->root);
    free(fs);
}

int fs_change_dir(FileSystem* fs, const char* path) {
    if (!path || strlen(path) == 0) {
        strcpy(fs->prev_dir, fs_get_current_path(fs));
        fs->current_dir = fs->root;
        fs_push_history(fs, "/");
        return 1;
    }
    
    if (strcmp(path, "/") == 0) {
        strcpy(fs->prev_dir, fs_get_current_path(fs));
        fs->current_dir = fs->root;
        fs_push_history(fs, "/");
        return 1;
    }
    
    if (strcmp(path, ".") == 0) return 1;
    
    if (strcmp(path, "..") == 0) {
        strcpy(fs->prev_dir, fs_get_current_path(fs));
        if (fs->current_dir->parent) {
            fs->current_dir = fs->current_dir->parent;
        } else {
            fs->current_dir = fs->root;
        }
        fs_push_history(fs, fs_get_current_path(fs));
        return 1;
    }
    
    if (strcmp(path, "-") == 0) {
        if (strlen(fs->prev_dir) == 0) {
            printf("cd: 没有上一个目录\n");
            return 0;
        }
        char current[MAX_PATH_LEN];
        strcpy(current, fs_get_current_path(fs));
        
        FileEntry* target = fs_find_entry_by_path(fs, fs->prev_dir);
        if (!target) {
            printf("cd: 上一个目录不存在: %s\n", fs->prev_dir);
            return 0;
        }
        
        if (target->type != DIR_TYPE) {
            printf("cd: 上一个路径不是目录: %s\n", fs->prev_dir);
            return 0;
        }
        
        fs->current_dir = target;
        strcpy(fs->prev_dir, current);
        fs_push_history(fs, fs_get_current_path(fs));
        printf("%s\n", fs_get_current_path(fs));
        return 1;
    }
    
    if (strcmp(path, "/..") == 0) {
        if (fs->history_count < 2) {
            printf("cd: 没有历史目录\n");
            return 0;
        }
        char current[MAX_PATH_LEN];
        strcpy(current, fs_get_current_path(fs));
        
        strcpy(fs->prev_dir, current);
        strcpy(current, fs->dir_history[fs->history_count - 2]);
        
        FileEntry* target = fs_find_entry_by_path(fs, current);
        if (!target) {
            printf("cd: 历史目录不存在: %s\n", current);
            return 0;
        }
        
        if (target->type != DIR_TYPE) {
            printf("cd: 历史路径不是目录: %s\n", current);
            return 0;
        }
        
        fs->current_dir = target;
        fs->history_count--; 
        fs_push_history(fs, fs_get_current_path(fs));
        printf("%s\n", fs_get_current_path(fs));
        return 1;
    }
    
    if (strcmp(path, "~") == 0) {
        FileEntry* home = find_child(fs->root, "home");
        if (home && home->type == DIR_TYPE) {
            strcpy(fs->prev_dir, fs_get_current_path(fs));
            fs->current_dir = home;
            fs_push_history(fs, fs_get_current_path(fs));
            return 1;
        } else {
            strcpy(fs->prev_dir, fs_get_current_path(fs));
            fs->current_dir = fs->root;
            fs_push_history(fs, "/");
            return 1;
        }
    }
    
    FileEntry* target = fs_find_entry_by_path(fs, path);
    if (!target) {
        printf("cd: %s: 没有那个文件或目录\n", path);
        return 0;
    }
    
    if (target->type != DIR_TYPE) {
        printf("cd: %s: 不是一个目录\n", path);
        return 0;
    }
    
    strcpy(fs->prev_dir, fs_get_current_path(fs));
    fs->current_dir = target;
    fs_push_history(fs, fs_get_current_path(fs));
    return 1;
}


static void create_user_initial_files(FileEntry* user_dir) {
    if (!user_dir) return;
    
    printf("正在为用户创建初始文件...\n");
    
    
    DWORD attr = GetFileAttributes(user_dir->real_path);
    if (attr == INVALID_FILE_ATTRIBUTES) {
        printf("用户目录不存在，正在创建: %s\n", user_dir->real_path);
        if (!CreateDirectory(user_dir->real_path, NULL)) {
            printf("无法创建用户目录: Windows错误 %lu\n", GetLastError());
            return;
        }
    }
    
    
    char bashrc_path[MAX_PATH_LEN];
    sprintf(bashrc_path, "%s\\.bashrc", user_dir->real_path);
    
    printf("创建文件: .bashrc\n");
    HANDLE hFile = CreateFile(bashrc_path, GENERIC_WRITE, 0, NULL, 
                             CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile != INVALID_HANDLE_VALUE) {
        const char* bashrc_content = 
            "# LGD OS 1.1.0 Bash配置\n"
            "alias ll='ls -l'\n"
            "alias la='ls -la'\n"
            "alias l='ls -CF'\n"
            "alias cls='clear'\n"
            "alias c='clear'\n"
            "alias ..='cd ..'\n"
            "alias ...='cd ../..'\n"
            "alias ....='cd ../../..'\n\n"
            "# 欢迎信息\n"
            "echo \"欢迎使用 LGD OS 1.1.0\"\n"
            "echo \"当前目录: $(pwd)\"\n"
            "echo \"输入 'help' 查看可用命令\"\n";
        
        DWORD bytesWritten;
        WriteFile(hFile, bashrc_content, strlen(bashrc_content), &bytesWritten, NULL);
        CloseHandle(hFile);
        
        
        FileEntry* bashrc = create_entry(".bashrc", FILE_TYPE);
        if (bashrc) {
            strcpy(bashrc->real_path, bashrc_path);
            add_child(user_dir, bashrc);
            printf("  -> 已创建.bashrc文件\n");
        }
    } else {
        printf("无法创建.bashrc文件: %s\n", bashrc_path);
    }
    
    
    char readme_path[MAX_PATH_LEN];
    sprintf(readme_path, "%s\\README.txt", user_dir->real_path);
    
    printf("创建文件: README.txt\n");
    hFile = CreateFile(readme_path, GENERIC_WRITE, 0, NULL, 
                      CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile != INVALID_HANDLE_VALUE) {
        const char* readme_content = 
            "========================================\n"
            "   欢迎使用 LGD OS 1.1.0\n"
            "========================================\n\n"
            "目录结构:\n"
            "  /               - 根目录 (对应 builds 目录)\n"
            "  /home/user      - 你的家目录\n"
            "  /etc            - 系统配置文件\n"
            "  /tmp            - 临时文件\n"
            "  /usr            - 用户程序\n"
            "  /var            - 可变数据\n"
            "  /bin            - 二进制可执行文件\n\n"
            "基本命令:\n"
            "  ls              - 列出目录内容\n"
            "  cd <目录>       - 切换目录\n"
            "  pwd             - 显示当前目录\n"
            "  mkdir <目录>    - 创建目录\n"
            "  touch <文件>    - 创建文件\n"
            "  cat <文件>      - 查看文件内容\n"
            "  edit <文件>     - 编辑文本文件\n"
            "  rm <文件>       - 删除文件\n"
            "  mv <原> <新>    - 移动/重命名文件\n"
            "  help            - 查看帮助\n"
            "  exit            - 退出系统\n\n"
            "提示:\n"
            "1. 所有文件都保存在 'builds' 目录中\n"
            "2. 你可以用Windows资源管理器查看这个目录\n"
            "3. 文件操作是实时的，立即在文件系统中生效\n\n"
            "系统信息:\n"
            "版本: 1.1.0\n"
            "存储: exe同一目录下的'builds'中\n\n"
            "祝你使用愉快！\n"
            "========================================\n";
        
        DWORD bytesWritten;
        WriteFile(hFile, readme_content, strlen(readme_content), &bytesWritten, NULL);
        CloseHandle(hFile);
        
        
        FileEntry* readme = create_entry("README.txt", FILE_TYPE);
        if (readme) {
            strcpy(readme->real_path, readme_path);
            add_child(user_dir, readme);
            printf("  -> 已创建README.txt文件\n");
        }
    } else {
        printf("无法创建README.txt文件: %s (错误: %lu)\n", readme_path, GetLastError());
    }
    
    
    char bash_profile_path[MAX_PATH_LEN];
    sprintf(bash_profile_path, "%s\\.bash_profile", user_dir->real_path);
    
    printf("创建文件: .bash_profile\n");
    hFile = CreateFile(bash_profile_path, GENERIC_WRITE, 0, NULL, 
                      CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile != INVALID_HANDLE_VALUE) {
        const char* profile_content = 
            "# ~/.bash_profile\n"
            "# 登录时执行\n\n"
            "if [ -f ~/.bashrc ]; then\n"
            "    . ~/.bashrc\n"
            "fi\n\n"
            "echo \"欢迎回来, $(whoami)!\"\n";
        
        DWORD bytesWritten;
        WriteFile(hFile, profile_content, strlen(profile_content), &bytesWritten, NULL);
        CloseHandle(hFile);
        
        FileEntry* bash_profile = create_entry(".bash_profile", FILE_TYPE);
        if (bash_profile) {
            strcpy(bash_profile->real_path, bash_profile_path);
            add_child(user_dir, bash_profile);
            printf("  -> 已创建.bash_profile文件\n");
        }
    }
    
    
    char docs_path[MAX_PATH_LEN];
    sprintf(docs_path, "%s\\Documents", user_dir->real_path);
    
    printf("创建目录: Documents\n");
    if (CreateDirectory(docs_path, NULL)) {
        FileEntry* docs = create_entry("Documents", DIR_TYPE);
        if (docs) {
            strcpy(docs->real_path, docs_path);
            add_child(user_dir, docs);
            printf("  -> 已创建Documents目录\n");
        }
    } else if (GetLastError() == ERROR_ALREADY_EXISTS) {
        printf("  -> Documents目录已存在\n");
    }
    
    
    char downloads_path[MAX_PATH_LEN];
    sprintf(downloads_path, "%s\\Downloads", user_dir->real_path);
    
    printf("创建目录: Downloads\n");
    if (CreateDirectory(downloads_path, NULL)) {
        FileEntry* downloads = create_entry("Downloads", DIR_TYPE);
        if (downloads) {
            strcpy(downloads->real_path, downloads_path);
            add_child(user_dir, downloads);
            printf("  -> 已创建Downloads目录\n");
        }
    } else if (GetLastError() == ERROR_ALREADY_EXISTS) {
        printf("  -> Downloads目录已存在\n");
    }
    
    
    char desktop_path[MAX_PATH_LEN];
    sprintf(desktop_path, "%s\\Desktop", user_dir->real_path);
    
    printf("创建目录: Desktop\n");
    if (CreateDirectory(desktop_path, NULL)) {
        FileEntry* desktop = create_entry("Desktop", DIR_TYPE);
        if (desktop) {
            strcpy(desktop->real_path, desktop_path);
            add_child(user_dir, desktop);
            printf("  -> 已创建Desktop目录\n");
        }
    } else if (GetLastError() == ERROR_ALREADY_EXISTS) {
        printf("  -> Desktop目录已存在\n");
    }
    
    
    char welcome_path[MAX_PATH_LEN];
    sprintf(welcome_path, "%s\\welcome.sh", user_dir->real_path);
    
    printf("创建文件: welcome.sh\n");
    hFile = CreateFile(welcome_path, GENERIC_WRITE, 0, NULL, 
                      CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile != INVALID_HANDLE_VALUE) {
        const char* welcome_content = 
            "#!/bin/bash\n"
            "# 欢迎脚本\n"
            "echo \"========================================\"\n"
            "echo \"      LGD OS 1.1.0 欢迎脚本\"\n"
            "echo \"========================================\"\n"
            "date\n"
            "echo \"用户: $(whoami)\"\n"
            "echo \"主机: $(hostname)\"\n"
            "echo \"目录: $(pwd)\"\n"
            "echo \"========================================\"\n"
            "ls -la\n";
        
        DWORD bytesWritten;
        WriteFile(hFile, welcome_content, strlen(welcome_content), &bytesWritten, NULL);
        CloseHandle(hFile);
        
        FileEntry* welcome = create_entry("welcome.sh", FILE_TYPE);
        if (welcome) {
            strcpy(welcome->real_path, welcome_path);
            add_child(user_dir, welcome);
            printf("  -> 已创建welcome.sh文件\n");
        }
    }
    
    printf("用户初始文件创建完成！\n");
}
char* fs_get_current_path(FileSystem* fs) {
    static char path[MAX_PATH_LEN];
    
    if (fs->current_dir == fs->root) {
        strcpy(path, "/");
        return path;
    }
    
    FileEntry* current = fs->current_dir;
    char* ptr = path + MAX_PATH_LEN - 1;
    *ptr = '\0';
    
    while (current != fs->root) {
        int len = strlen(current->name);
        ptr -= len;
        strncpy(ptr, current->name, len);
        
        ptr--;
        *ptr = '/';
        
        current = current->parent;
    }
    
    if (ptr[0] != '/') {
        ptr--;
        *ptr = '/';
    }
    
    return ptr;
}

void setup_sample_filesystem(FileSystem* fs) {
    if (!fs || !fs->root) return;
    
    
    
    
    if (!ensure_builds_directory()) {
        printf("错误: 无法访问builds目录\n");
        return;
    }
    
    
    create_directory_if_not_exists_silent("home");
    create_directory_if_not_exists_silent("etc");
    create_directory_if_not_exists_silent("var");
    create_directory_if_not_exists_silent("tmp");
    create_directory_if_not_exists_silent("usr");
    create_directory_if_not_exists_silent("bin");
    create_directory_if_not_exists_silent("sbin");
    create_directory_if_not_exists_silent("lib");
    create_directory_if_not_exists_silent("dev");
    create_directory_if_not_exists_silent("proc");
    create_directory_if_not_exists_silent("sys");
    create_directory_if_not_exists_silent("root");
    create_directory_if_not_exists_silent("mnt");
    create_directory_if_not_exists_silent("opt");
    create_directory_if_not_exists_silent("srv");
    create_directory_if_not_exists_silent("run");
    create_directory_if_not_exists_silent("media");
    create_directory_if_not_exists_silent("boot");
    
    
    create_directory_if_not_exists_silent("home\\user");
    
    
    create_directory_if_not_exists_silent("var\\log");
    
    
    create_directory_if_not_exists_silent("usr\\bin");
    
    
    create_directory_if_not_exists_silent("usr\\lib");
    
   
    strcpy(fs->root->real_path, BUILDS_DIR);
    
    
    sync_existing_directories_silent(fs);
}

int cmd_cd(ShellContext* ctx, int argc, char** argv, FILE* input, FILE* output) {
    if (argc > 1) {
        if (!fs_change_dir(ctx->fs, argv[1])) {
            return 1;
        }
    } else {
        fs_change_dir(ctx->fs, "~");
    }
    return 0;
}
static int show_real_file_info(FileEntry* file) {
    if (!file || file->type != FILE_TYPE) {
        printf("show_real_file_info: 无效的文件\n");
        return 0;
    }
    
    DWORD attr = GetFileAttributes(file->real_path);
    if (attr == INVALID_FILE_ATTRIBUTES) {
        printf("无法获取文件信息: %s\n", file->real_path);
        return 0;
    }
    
    HANDLE hFile = CreateFile(file->real_path, GENERIC_READ, FILE_SHARE_READ, NULL, 
                             OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        printf("无法打开文件: %s\n", file->real_path);
        return 0;
    }
    
    DWORD fileSize = GetFileSize(hFile, NULL);
    
    FILETIME createTime, accessTime, writeTime;
    GetFileTime(hFile, &createTime, &accessTime, &writeTime);
    
    CloseHandle(hFile);
    
    
    SYSTEMTIME sysCreateTime, sysWriteTime;
    FileTimeToSystemTime(&createTime, &sysCreateTime);
    FileTimeToSystemTime(&writeTime, &sysWriteTime);
    
    printf("文件: %s\n", file->name);
    printf("真实路径: %s\n", file->real_path);
    printf("大小: %lu 字节\n", fileSize);
    printf("创建时间: %04d-%02d-%02d %02d:%02d:%02d\n", 
           sysCreateTime.wYear, sysCreateTime.wMonth, sysCreateTime.wDay,
           sysCreateTime.wHour, sysCreateTime.wMinute, sysCreateTime.wSecond);
    printf("修改时间: %04d-%02d-%02d %02d:%02d:%02d\n", 
           sysWriteTime.wYear, sysWriteTime.wMonth, sysWriteTime.wDay,
           sysWriteTime.wHour, sysWriteTime.wMinute, sysWriteTime.wSecond);
    
    return 1;
}

static int display_real_file_content(FileEntry* file, int show_line_numbers) {
    if (!file || file->type != FILE_TYPE) {
        printf("display_real_file_content: 无效的文件\n");
        return 0;
    }
    
    char* content = read_real_file(file->real_path);
    if (!content) {
        printf("文件为空或无法读取: %s\n", file->real_path);
        return 0;
    }
    
    printf("文件: %s (真实路径: %s)\n", file->name, file->real_path);
    printf("----------------------------------------\n");
    
    if (show_line_numbers) {
        int line_num = 1;
        char* line_start = content;
        char* current = content;
        
        while (*current) {
            if (*current == '\n') {
                printf("%4d: ", line_num++);
                fwrite(line_start, 1, current - line_start, stdout);
                printf("\n");
                line_start = current + 1;
            }
            current++;
        }
        
        if (line_start < current) {
            printf("%4d: ", line_num);
            fwrite(line_start, 1, current - line_start, stdout);
            printf("\n");
        }
    } else {
        printf("%s", content);
        if (content[strlen(content) - 1] != '\n') {
            printf("\n");
        }
    }
    
    printf("----------------------------------------\n");
    
    free(content);
    return 1;
}
void fs_list_dir_output(FileSystem* fs, int detailed, int show_all, FILE* output) {
    if (!fs || !fs->current_dir || !output) return;
    
    char search_path[MAX_PATH_LEN];
    if (fs->current_dir == fs->root) {
        sprintf(search_path, "%s\\*", BUILDS_DIR);
    } else {
        sprintf(search_path, "%s\\*", fs->current_dir->real_path);
    }
    
    WIN32_FIND_DATA find_data;
    HANDLE hFind = FindFirstFile(search_path, &find_data);
    
    if (hFind == INVALID_HANDLE_VALUE) {
        fprintf(output, "无法列出目录: %s\n", search_path);
        return;
    }
    
    int total_files = 0;
    int total_dirs = 0;
    
    if (detailed) {
        char display_path[MAX_PATH_LEN];
        if (fs->current_dir == fs->root) {
            strcpy(display_path, "/");
        } else {
            char* relative_path = fs->current_dir->real_path + strlen(BUILDS_DIR) + 1;
            sprintf(display_path, "/%s", relative_path);
        }
        fprintf(output, "%s\n", display_path);
        fprintf(output, "----------------------------------------\n");
    }
    
    do {
        if (strcmp(find_data.cFileName, ".") == 0 || strcmp(find_data.cFileName, "..") == 0) {
            continue;
        }
        
        if (!show_all && find_data.cFileName[0] == '.') {
            continue;
        }
        
        if (detailed) {
            if (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                fprintf(output, "[DIR]  %-30s ", find_data.cFileName);
            } else {
                fprintf(output, "[FILE] %-30s %8lu bytes ", 
                       find_data.cFileName, find_data.nFileSizeLow);
            }
            
            
            time_t now = time(NULL);
            struct tm* tm_info = localtime(&now);
            fprintf(output, "%04d-%02d-%02d %02d:%02d:%02d\n",
                   tm_info->tm_year + 1900, tm_info->tm_mon + 1, tm_info->tm_mday,
                   tm_info->tm_hour, tm_info->tm_min, tm_info->tm_sec);
        } else {
            if (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                fprintf(output, "\033[1;34m%s\033[0m  ", find_data.cFileName);
            } else {
                fprintf(output, "%s  ", find_data.cFileName);
            }
        }
        
        if (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            total_dirs++;
        } else {
            total_files++;
        }
    } while (FindNextFile(hFind, &find_data) != 0);
    
    FindClose(hFind);
    
    if (detailed) {
        fprintf(output, "----------------------------------------\n");
        fprintf(output, "总计: %d 个目录, %d 个文件\n", total_dirs, total_files);
    } else {
        fprintf(output, "\n");
    }
}

int cmd_ll(ShellContext* ctx, int argc, char** argv, FILE* input, FILE* output) {
    
    char* new_argv[MAX_ARGS];
    int new_argc = 0;
    
    new_argv[new_argc++] = "ls";
    new_argv[new_argc++] = "-l";
    
    for (int i = 1; i < argc && i < MAX_ARGS - 1; i++) {
        new_argv[new_argc++] = argv[i];
    }
    
    new_argv[new_argc] = NULL;
    
    return cmd_ls(ctx, new_argc, new_argv, input, output);
}
int cmd_ls(ShellContext* ctx, int argc, char** argv, FILE* input, FILE* output) {
    int detailed = 0;
    int show_all = 0;
    char* target_dir = NULL;
    
    
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-l") == 0) {
            detailed = 1;
        } else if (strcmp(argv[i], "-a") == 0) {
            show_all = 1;
        } else if (strcmp(argv[i], "-la") == 0 || strcmp(argv[i], "-al") == 0) {
            detailed = 1;
            show_all = 1;
        } else if (argv[i][0] == '-') {
            fprintf(output, "ls: 无效选项 '%s'\n", argv[i]);
            return 1;
        } else {
            target_dir = argv[i];
        }
    }
    
    
    char saved_path[MAX_PATH_LEN];
    if (target_dir) {
        strcpy(saved_path, fs_get_current_path(ctx->fs));
        if (!fs_change_dir(ctx->fs, target_dir)) {
            fprintf(output, "ls: 无法访问 '%s': 没有那个文件或目录\n", target_dir);
            return 1;
        }
    }
    
    
    fs_list_dir_output(ctx->fs, detailed, show_all, output);
    
    
    if (target_dir) {
        fs_change_dir(ctx->fs, saved_path);
    }
    
    return 0;
}
int cmd_pwd(ShellContext* ctx, int argc, char** argv, FILE* input, FILE* output) {
    printf("%s\n", fs_get_current_path(ctx->fs));
    return 0;
}

int cmd_save(ShellContext* ctx, int argc, char** argv, FILE* input, FILE* output) {
    fprintf(output, "注意: 从版本1.1.0开始，不再需要手动保存文件系统状态\n");
    fprintf(output, "所有文件都直接保存在 %s 目录中\n", BUILDS_DIR);
    fprintf(output, "你可以在Windows资源管理器中查看该目录\n");
    return 0;
}
void save_file_system_recursive(FILE* fp, FileEntry* entry, const char* base_path) {
    if (!entry) return;
    
    char full_path[MAX_PATH_LEN];
    if (strcmp(base_path, "/") == 0 && strcmp(entry->name, "/") == 0) {
        strcpy(full_path, "/");
    } else if (strcmp(base_path, "/") == 0) {
        sprintf(full_path, "/%s", entry->name);
    } else {
        sprintf(full_path, "%s/%s", base_path, entry->name);
    }
    
    if (entry->type == DIR_TYPE) {
       
        fprintf(fp, "D %s %ld %ld\n", 
                full_path, 
                (long)entry->create_time, 
                (long)entry->modify_time);
        
        
        FileEntry* child = entry->children;
        while (child) {
            save_file_system_recursive(fp, child, full_path);
            child = child->next;
        }
    } else if (entry->type == FILE_TYPE) {
        
        fprintf(fp, "F %s %ld %ld %zu ", 
                full_path, 
                (long)entry->create_time, 
                (long)entry->modify_time, 
                entry->size);
        
        
        if (entry->content && entry->size > 0) {
            for (size_t i = 0; i < entry->size; i++) {
                char c = entry->content[i];
                if (c == '\n') {
                    fprintf(fp, "\\n");
                } else if (c == '\r') {
                    fprintf(fp, "\\r");
                } else if (c == '\\') {
                    fprintf(fp, "\\\\");
                } else if (c == ' ' || c == '\t') {
                    fprintf(fp, "\\x%02x", (unsigned char)c);
                } else if (c < 32 || c > 126) {
                    fprintf(fp, "\\x%02x", (unsigned char)c);
                } else {
                    fprintf(fp, "%c", c);
                }
            }
        }
        fprintf(fp, "\n");
    }
}
int cmd_load(ShellContext* ctx, int argc, char** argv, FILE* input, FILE* output) {
    fprintf(output, "注意: 从版本1.1.0开始，不再需要手动加载文件系统状态\n");
    fprintf(output, "每次启动时都会自动从 %s 目录加载现有文件\n", BUILDS_DIR);
    return 0;
}
static void ensure_directory_path(FileSystem* fs, const char* path) {
    if (!path || strlen(path) <= 1) return;
    
    char temp_path[MAX_PATH_LEN];
    char* current_dir = fs_get_current_path(fs);
    char saved_current_dir[MAX_PATH_LEN];
    strcpy(saved_current_dir, current_dir);
    
    
    fs_change_dir(fs, "/");
    
    
    char* token;
    char copy_path[MAX_PATH_LEN];
    strcpy(copy_path, path);
    
    
    if (copy_path[0] == '/') {
        token = strtok(copy_path + 1, "/");
    } else {
        token = strtok(copy_path, "/");
    }
    
    while (token) {
        
        FileEntry* entry = find_child(fs->current_dir, token);
        
        if (!entry) {
            
            FileEntry* dir = create_entry(token, DIR_TYPE);
            if (dir) {
                add_child(fs->current_dir, dir);
                fs->dir_count++;
                fs->current_dir = dir;
            }
        } else if (entry->type == DIR_TYPE) {
            
            fs->current_dir = entry;
        } else {
            
            break;
        }
        
        token = strtok(NULL, "/");
    }
    
    
    fs_change_dir(fs, saved_current_dir);
}
int cmd_history(ShellContext* ctx, int argc, char** argv) {
    FileSystem* fs = ctx->fs;
    printf("目录历史记录:\n");
    for (int i = 0; i < fs->history_count; i++) {
        printf("%2d. %s\n", i + 1, fs->dir_history[i]);
    }
    return 0;
}
static void load_file_system_entry(FileSystem* fs, const char* line) {
    char type[2];
    char path[MAX_PATH_LEN];
    long create_time, modify_time;
    size_t size = 0;
    
    if (sscanf(line, "%1s %s %ld %ld", type, path, &create_time, &modify_time) < 4) {
        return;
    }
    
    
    if (strcmp(path, "/") == 0) {
        return;  
    }
    
    char* last_slash = strrchr(path, '/');
    if (!last_slash) {
        return;
    }
    
    char parent_path[MAX_PATH_LEN];
    char entry_name[MAX_NAME_LEN];
    
    if (last_slash == path) {  
        strcpy(parent_path, "/");
        strcpy(entry_name, last_slash + 1);
    } else {
        strncpy(parent_path, path, last_slash - path);
        parent_path[last_slash - path] = '\0';
        strcpy(entry_name, last_slash + 1);
    }
    
    
    FileEntry* parent = NULL;
    if (strcmp(parent_path, "/") == 0) {
        parent = fs->root;
    } else {
       
        char saved_path[MAX_PATH_LEN];
        strcpy(saved_path, fs_get_current_path(fs));
        
        if (fs_change_dir(fs, parent_path)) {
            parent = fs->current_dir;
        }
        
        
        fs_change_dir(fs, saved_path);
    }
    
    if (!parent || parent->type != DIR_TYPE) {
        return;
    }
    
    
    FileEntry* existing = find_child(parent, entry_name);
    if (existing) {
        
        existing->create_time = (time_t)create_time;
        existing->modify_time = (time_t)modify_time;
        return;
    }
    
    if (type[0] == 'D') {
        
        FileEntry* dir = create_entry(entry_name, DIR_TYPE);
        if (dir) {
            dir->create_time = (time_t)create_time;
            dir->modify_time = (time_t)modify_time;
            add_child(parent, dir);
            fs->dir_count++;
        }
    } else if (type[0] == 'F') {
       
        char* content_start = strchr(line, ' ');
        for (int i = 0; i < 4; i++) {
            if (!content_start) break;
            content_start = strchr(content_start + 1, ' ');
        }
        
        
        FileEntry* file = create_entry(entry_name, FILE_TYPE);
        if (file) {
            file->create_time = (time_t)create_time;
            file->modify_time = (time_t)modify_time;
            
            if (content_start) {
                content_start++;
                char* decoded_content = decode_content(content_start, &size);
                if (decoded_content) {
                    file->content = decoded_content;
                    file->size = size;
                }
            }
            
            add_child(parent, file);
            fs->file_count++;
        }
    }
}
int cmd_mkdir(ShellContext* ctx, int argc, char** argv, FILE* input, FILE* output) {
    if (argc < 2) {
        printf("mkdir: 缺少操作数\n");
        printf("用法: mkdir <目录名>\n");
        return 1;
    }
    
    for (int i = 1; i < argc; i++) {
        char dir_name[MAX_NAME_LEN];
        strcpy(dir_name, argv[i]);
        
        
        char current_real_path[MAX_PATH_LEN];
        if (ctx->fs->current_dir == ctx->fs->root) {
            strcpy(current_real_path, BUILDS_DIR);
        } else {
            strcpy(current_real_path, ctx->fs->current_dir->real_path);
        }
        
        
        char full_real_path[MAX_PATH_LEN];
        sprintf(full_real_path, "%s\\%s", current_real_path, dir_name);
        
        
        DWORD attr = GetFileAttributes(full_real_path);
        if (attr != INVALID_FILE_ATTRIBUTES) {
            printf("mkdir: 无法创建目录 '%s': 文件或目录已存在\n", dir_name);
            continue;
        }
        
        
        if (!CreateDirectory(full_real_path, NULL)) {
            printf("mkdir: 无法创建目录 '%s': Windows错误 %lu\n", dir_name, GetLastError());
            continue;
        }
        
        
        FileEntry* dir = create_entry(dir_name, DIR_TYPE);
        if (!dir) {
            printf("mkdir: 内存不足\n");
            RemoveDirectory(full_real_path);  
            continue;
        }
        
        strcpy(dir->real_path, full_real_path);
        dir->parent = ctx->fs->current_dir;
        
        add_child(ctx->fs->current_dir, dir);
        ctx->fs->dir_count++;
        
        printf("已创建目录: %s (真实路径: %s)\n", dir_name, full_real_path);
    }
    
    return 0;
}
int cmd_rmdir(ShellContext* ctx, int argc, char** argv, FILE* input, FILE* output) {
    if (argc < 2) {
        printf("rmdir: 缺少操作数\n");
        printf("用法: rmdir <目录名>\n");
        return 1;
    }
    
    for (int i = 1; i < argc; i++) {
        FileEntry* dir = find_child(ctx->fs->current_dir, argv[i]);
        if (!dir) {
            printf("rmdir: 无法删除 '%s': 没有那个文件或目录\n", argv[i]);
            continue;
        }
        
        if (dir->type != DIR_TYPE) {
            printf("rmdir: 无法删除 '%s': 不是目录\n", argv[i]);
            continue;
        }
        
        if (dir->children) {
            printf("rmdir: 无法删除 '%s': 目录非空\n", argv[i]);
            continue;
        }
        
        
        if (!RemoveDirectory(dir->real_path)) {
            printf("rmdir: 无法删除 '%s': Windows错误 %lu\n", argv[i], GetLastError());
            continue;
        }
        
        if (remove_child(ctx->fs->current_dir, argv[i])) {
            ctx->fs->dir_count--;
            printf("已删除目录: %s (真实路径: %s)\n", argv[i], dir->real_path);
        } else {
            printf("rmdir: 无法删除 '%s': 未知错误\n", argv[i]);
        }
    }
    
    return 0;
}
int cmd_mv(ShellContext* ctx, int argc, char** argv, FILE* input, FILE* output) {
    if (argc < 3) {
        printf("mv: 缺少操作数\n");
        printf("用法: mv <原名称> <新名称>\n");
        return 1;
    }
    
    char* old_name = argv[1];
    char* new_name = argv[2];
    
    
    FileEntry* entry = find_child(ctx->fs->current_dir, old_name);
    if (!entry) {
        printf("mv: 无法移动 '%s': 没有那个文件或目录\n", old_name);
        return 1;
    }
    
   
    FileEntry* existing = find_child(ctx->fs->current_dir, new_name);
    if (existing) {
        printf("mv: 无法重命名: '%s' 已存在\n", new_name);
        return 1;
    }
    
    
    strcpy(entry->name, new_name);
    entry->modify_time = time(NULL);
    
    printf("已将 '%s' 重命名为 '%s'\n", old_name, new_name);
    return 0;
}
int cmd_info(ShellContext* ctx, int argc, char** argv, FILE* input, FILE* output) {
    if (argc < 2) {
        printf("info: 缺少操作数\n");
        printf("用法: info <文件/目录名>\n");
        return 1;
    }
    
    
    char real_path[MAX_PATH_LEN];
    if (ctx->fs->current_dir == ctx->fs->root) {
        sprintf(real_path, "%s\\%s", BUILDS_DIR, argv[1]);
    } else {
        sprintf(real_path, "%s\\%s", ctx->fs->current_dir->real_path, argv[1]);
    }
    
    DWORD attr = GetFileAttributes(real_path);
    if (attr == INVALID_FILE_ATTRIBUTES) {
        printf("info: 无法获取 '%s' 的信息: 没有那个文件或目录\n", argv[1]);
        return 1;
    }
    
    char create_time[32], modify_time[32];
    time_t now = time(NULL);
    strftime(create_time, sizeof(create_time), "%Y-%m-%d %H:%M:%S", localtime(&now));
    strftime(modify_time, sizeof(modify_time), "%Y-%m-%d %H:%M:%S", localtime(&now));
    
    printf("名称: %s\n", argv[1]);
    printf("真实路径: %s\n", real_path);
    
    if (attr & FILE_ATTRIBUTE_DIRECTORY) {
        printf("类型: 目录\n");
        
        
        char search_path[MAX_PATH_LEN];
        sprintf(search_path, "%s\\*", real_path);
        
        WIN32_FIND_DATA find_data;
        HANDLE hFind = FindFirstFile(search_path, &find_data);
        
        int file_count = 0, dir_count = 0;
        if (hFind != INVALID_HANDLE_VALUE) {
            do {
                if (strcmp(find_data.cFileName, ".") == 0 || strcmp(find_data.cFileName, "..") == 0) {
                    continue;
                }
                
                if (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                    dir_count++;
                } else {
                    file_count++;
                }
            } while (FindNextFile(hFind, &find_data) != 0);
            
            FindClose(hFind);
        }
        
        printf("包含: %d 个文件, %d 个子目录\n", file_count, dir_count);
    } else {
        printf("类型: 文件\n");
        
        
        HANDLE hFile = CreateFile(real_path, GENERIC_READ, FILE_SHARE_READ, NULL, 
                                 OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile != INVALID_HANDLE_VALUE) {
            DWORD fileSize = GetFileSize(hFile, NULL);
            CloseHandle(hFile);
            printf("大小: %lu 字节\n", fileSize);
            
            if (argc > 2 && strcmp(argv[2], "-c") == 0) {
                char* content = read_real_file(real_path);
                if (content) {
                    printf("内容预览:\n%s\n", content);
                    free(content);
                } else {
                    printf("内容预览: (无法读取或为空)\n");
                }
            }
        } else {
            printf("大小: 无法获取\n");
        }
    }
    
    printf("创建时间: %s\n", create_time);
    printf("修改时间: %s\n", modify_time);
    
    return 0;
}
char* decode_content(const char* encoded, size_t* out_size) {
    size_t len = strlen(encoded);
    char* decoded = malloc(len + 1);
    if (!decoded) return NULL;
    
    size_t i = 0, j = 0;
    while (i < len) {
        if (encoded[i] == '\\') {
            if (i + 1 < len) {
                if (encoded[i + 1] == 'n') {
                    decoded[j++] = '\n';
                    i += 2;
                } else if (encoded[i + 1] == 'r') {
                    decoded[j++] = '\r';
                    i += 2;
                } else if (encoded[i + 1] == '\\') {
                    decoded[j++] = '\\';
                    i += 2;
                } else if (i + 3 < len && encoded[i + 1] == 'x') {
                    
                    char hex[3] = {encoded[i + 2], encoded[i + 3], '\0'};
                    int value;
                    sscanf(hex, "%2x", &value);
                    decoded[j++] = (char)value;
                    i += 4;
                } else {
                    
                    decoded[j++] = encoded[i++];
                }
            } else {
                decoded[j++] = encoded[i++];
            }
        } else {
            decoded[j++] = encoded[i++];
        }
    }
    decoded[j] = '\0';
    
    if (out_size) *out_size = j;
    return decoded;
}
int cmd_help(ShellContext* ctx, int argc, char** argv, FILE* input, FILE* output) {
    if (argc == 1) {
        fprintf(output, "LGD OS 1.1.0 命令帮助\n");
        fprintf(output, "=====================\n\n");
        
        fprintf(output, "可用的命令组:\n");
        fprintf(output, "  1. 目录操作命令: cd, pwd, mkdir, rmdir\n");
        fprintf(output, "  2. 文件操作命令: ls, ll, rm, mv, info, cat, edit, touch\n");
        fprintf(output, "  3. 文本处理命令: grep, wc, head, tail\n");
        fprintf(output, "  4. 系统操作命令: save, load, history, clear, exit\n");
        fprintf(output, "  5. 帮助命令: help\n\n");
        
        fprintf(output, "管道功能:\n");
        fprintf(output, "  使用 | 连接多个命令，将前一个命令的输出作为后一个命令的输入\n");
        fprintf(output, "  示例: ls | pwd\n");
        fprintf(output, "输入 'help <命令名>' 获取特定命令的详细帮助\n");
        fprintf(output, "示例: help cd, help ls, help mkdir\n\n");
        fprintf(output, "输入 'help all' 显示所有命令的完整帮助信息\n");
        
        return 0;
    }
    
    if (strcmp(argv[1], "all") == 0) {
        printf("LGD OS 1.1.0 完整命令手册\n");
        printf("========================\n\n");
        
        int cmd_count = 0;
        for (int i = 0; command_table[i].name != NULL; i++) {
            CommandEntry* cmd = &command_table[i];
            
            if (strcmp(cmd->name, "cls") == 0 || strcmp(cmd->name, "quit") == 0) {
                continue;
            }
            
            printf("%d. %s 命令 - %s\n", ++cmd_count, cmd->name, cmd->description);
            printf("   用法: %s\n", cmd->usage);
            printf("   示例:\n%s\n\n", cmd->example);
        }
        
        return 0;
    }
    
    char* command_name = argv[1];
    CommandEntry* found_cmd = NULL;
    
    for (int i = 0; command_table[i].name != NULL; i++) {
        if (strcmp(command_name, command_table[i].name) == 0) {
            found_cmd = &command_table[i];
            break;
        }
    }
    
    if (found_cmd) {
        printf("%s 命令 - %s\n", found_cmd->name, found_cmd->description);
        printf("========================\n\n");
        printf("用法:\n%s\n\n", found_cmd->usage);
        printf("示例:\n%s\n", found_cmd->example);
    } else {
        printf("未知命令: %s\n", command_name);
        printf("输入 'help' 查看所有可用命令\n");
        return 1;
    }
    
    return 0;
}

int cmd_clear(ShellContext* ctx, int argc, char** argv, FILE* input, FILE* output) {
    system("cls");
    return 0;
}

int cmd_exit(ShellContext* ctx, int argc, char** argv, FILE* input, FILE* output) {
    ctx->running = 0;
    return 0;
}

void shell_show_prompt(ShellContext* ctx) {
    char* path = fs_get_current_path(ctx->fs);
    
    printf("\033[1;32m%s@%s\033[0m", ctx->username, ctx->hostname);
    printf(":\033[1;34m%s\033[0m$ ", path);
    fflush(stdout);
}

void shell_execute_command(ShellContext* ctx, const char* input) {
    if (!ctx || !input) return;
    
    
    int has_pipe = 0;
    for (int i = 0; input[i]; i++) {
        if (input[i] == '|') {
            has_pipe = 1;
            break;
        }
    }
    
    if (has_pipe) {
        
        execute_pipeline(ctx, input);
    } else {
        
        char* args[MAX_ARGS];
        int argc = 0;
        
        char input_copy[MAX_INPUT];
        strcpy(input_copy, input);
        
        char* token = strtok(input_copy, " \t\n");
        while (token && argc < MAX_ARGS - 1) {
            args[argc++] = token;
            token = strtok(NULL, " \t\n");
        }
        args[argc] = NULL;
        
        if (argc == 0) return;
        
        char* command_name = args[0];
        CommandEntry* cmd = NULL;
        
        for (int i = 0; command_table[i].name != NULL; i++) {
            if (strcmp(command_name, command_table[i].name) == 0) {
                cmd = &command_table[i];
                break;
            }
        }
        
        if (cmd) {
            
            cmd->handler(ctx, argc, args, stdin, stdout);
        } else {
            fprintf(stderr, "%s: 未找到命令\n", command_name);
        }
    }
}

void shell_run(ShellContext* ctx) {
    char input[MAX_INPUT];
    
    printf("输入 'help' 查看可用命令\n\n");
    
    while (ctx->running) {
        shell_show_prompt(ctx);
        
        if (fgets(input, sizeof(input), stdin) == NULL) {
            break;
        }
        
        input[strcspn(input, "\n")] = 0;
        
        if (strlen(input) > 0) {
            shell_execute_command(ctx, input);
        }
    }
}

int main() {
    printf("========================================\n");
    printf("   LGD OS 1.1.0   builds 20262251840    \n");
    printf("     sse_107(C)2025-2026 保留所有权利   \n");
    printf("========================================\n");
    
    
    
    DWORD attr = GetFileAttributes(BUILDS_DIR);
    int builds_exists = (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY));
    
    if (!builds_exists) {
        printf("正在初始化系统...\n");
        if (!CreateDirectory(BUILDS_DIR, NULL)) {
            printf("错误: 无法创建 %s 目录\n", BUILDS_DIR);
            return 1;
        }
    }
    
    
    FileSystem* fs = fs_init();
    if (!fs) {
        printf("错误: 无法初始化文件系统\n");
        return 1;
    }
    
    
    strcpy(fs->root->real_path, BUILDS_DIR);
    
    ShellContext ctx;
    ctx.fs = fs;
    ctx.running = 1;
    
    const char* env_username = getenv("USERNAME");
    if (env_username) {
        strcpy(ctx.username, env_username);
    } else {
        strcpy(ctx.username, "user");
    }

    const char* env_hostname = getenv("COMPUTERNAME");
    if (env_hostname) {
        strcpy(ctx.hostname, env_hostname);
    } else {
        strcpy(ctx.hostname, "lgd-os");
    }
    
    
    setup_sample_filesystem(fs);
    
    
    if (!fs_change_dir(fs, "/home/user")) {
        fs_change_dir(fs, "/");
    }
    
    
    char cwd[MAX_PATH_LEN];
    GetCurrentDirectory(MAX_PATH_LEN, cwd);
    
    printf("\n系统就绪\n");
    printf("用户: %s\n", ctx.username);
    printf("主机: %s\n", ctx.hostname);
    printf("当前目录: %s\n", fs_get_current_path(fs));
    printf("存储目录: %s\n", BUILDS_DIR);
    
    printf("\n输入 'help' 查看命令\n");
    
    shell_run(&ctx);
    
    //printf("\n正在保存系统状态...\n");
    //char* save_argv[] = {"save", NULL};
    //cmd_save(&ctx, 1, save_argv);
    //1.0.0远古保存方式已绝版 
    printf("正在关闭系统...\n");
    fs_destroy(fs);
    
    printf("\nLGD OS 1.1.0 已安全关闭。\n");
    printf("你可以在 '%s' 目录中找到创建的所有文件和目录\n", BUILDS_DIR);
    printf("Goodbye!\n");
    
    return 0;
}
