#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <windows.h>
#include <ctype.h>

#define MAX_NAME_LEN 256
#define MAX_PATH_LEN 1024
#define MAX_INPUT 256
#define MAX_ARGS 32
#define MAX_HISTORY 50
#define SAVE_FILE "lgdos_save.txt"

typedef enum {
    FILE_TYPE,
    DIR_TYPE
} EntryType;

typedef struct FileEntry {
    char name[MAX_NAME_LEN];
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

FileSystem* fs_init(void);
void fs_destroy(FileSystem* fs);
char* fs_get_current_path(FileSystem* fs);
int fs_change_dir(FileSystem* fs, const char* path);
void fs_list_dir(FileSystem* fs, int detailed, int show_all);
void setup_sample_filesystem(FileSystem* fs);

int cmd_cd(ShellContext* ctx, int argc, char** argv);
int cmd_ls(ShellContext* ctx, int argc, char** argv);
int cmd_ll(ShellContext* ctx, int argc, char** argv);
int cmd_pwd(ShellContext* ctx, int argc, char** argv);
int cmd_save(ShellContext* ctx, int argc, char** argv);
int cmd_load(ShellContext* ctx, int argc, char** argv);
int cmd_mkdir(ShellContext* ctx, int argc, char** argv);
int cmd_rmdir(ShellContext* ctx, int argc, char** argv);
int cmd_rm(ShellContext* ctx, int argc, char** argv);
int cmd_mv(ShellContext* ctx, int argc, char** argv);
int cmd_info(ShellContext* ctx, int argc, char** argv);
int cmd_history(ShellContext* ctx, int argc, char** argv);
int cmd_help(ShellContext* ctx, int argc, char** argv);
int cmd_clear(ShellContext* ctx, int argc, char** argv);
int cmd_exit(ShellContext* ctx, int argc, char** argv);

void shell_show_prompt(ShellContext* ctx);
void shell_execute_command(ShellContext* ctx, const char* input);
void shell_run(ShellContext* ctx);
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
int cmd_rm(ShellContext* ctx, int argc, char** argv) {
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
        
        if (remove_child(ctx->fs->current_dir, argv[i])) {
            ctx->fs->file_count--;
            printf("已删除文件: %s\n", argv[i]);
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
    entry->type = type;
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

void fs_list_dir(FileSystem* fs, int detailed, int show_all) {
    if (!fs || !fs->current_dir) return;
    
    FileEntry* child = fs->current_dir->children;
    
    if (detailed) {
        int total_blocks = 0;
        FileEntry* temp = child;
        while (temp) {
            if (!show_all && temp->name[0] == '.') {
                temp = temp->next;
                continue;
            }
            total_blocks += 1;
            temp = temp->next;
        }
        printf("总用量 %d\n", total_blocks);
        
        while (child) {
            if (!show_all && child->name[0] == '.') {
                child = child->next;
                continue;
            }
            
            char time_buf[32];
            strftime(time_buf, sizeof(time_buf), "%b %d %H:%M", localtime(&child->modify_time));
            
            if (child->type == DIR_TYPE) {
                printf("drwxr-xr-x 1 root root %6zu %s \033[1;34m%s\033[0m\n",
                       child->size, time_buf, child->name);
            } else {
                printf("-rw-r--r-- 1 root root %6zu %s %s\n",
                       child->size, time_buf, child->name);
            }
            child = child->next;
        }
    } else {
        int count = 0;
        while (child) {
            if (!show_all && child->name[0] == '.') {
                child = child->next;
                continue;
            }
            
            if (child->type == DIR_TYPE) {
                printf("\033[1;34m%s\033[0m  ", child->name);
            } else {
                printf("%s  ", child->name);
            }
            
            count++;
            if (count % 5 == 0) printf("\n");
            child = child->next;
        }
        if (count % 5 != 0) printf("\n");
    }
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
    
    FileEntry* home = find_child(fs->root, "home");
    if (!home) {
        home = create_entry("home", DIR_TYPE);
        if (home) {
            add_child(fs->root, home);
            fs->dir_count++;
        }
    }
    
    FileEntry* etc = find_child(fs->root, "etc");
    if (!etc) {
        etc = create_entry("etc", DIR_TYPE);
        if (etc) {
            add_child(fs->root, etc);
            fs->dir_count++;
        }
    }
    
    FileEntry* tmp = find_child(fs->root, "tmp");
    if (!tmp) {
        tmp = create_entry("tmp", DIR_TYPE);
        if (tmp) {
            add_child(fs->root, tmp);
            fs->dir_count++;
        }
    }
    
    FileEntry* bin = find_child(fs->root, "bin");
    if (!bin) {
        bin = create_entry("bin", DIR_TYPE);
        if (bin) {
            add_child(fs->root, bin);
            fs->dir_count++;
        }
    }
    
    FileEntry* var = find_child(fs->root, "var");
    if (!var) {
        var = create_entry("var", DIR_TYPE);
        if (var) {
            add_child(fs->root, var);
            fs->dir_count++;
        }
    }
    
    FileEntry* usr = find_child(fs->root, "usr");
    if (!usr) {
        usr = create_entry("usr", DIR_TYPE);
        if (usr) {
            add_child(fs->root, usr);
            fs->dir_count++;
        }
    }
    
    
    if (home) {
        FileEntry* user = find_child(home, "user");
        if (!user) {
            user = create_entry("user", DIR_TYPE);
            if (user) {
                add_child(home, user);
                fs->dir_count++;
            }
        }
        
        
        if (user && !find_child(user, ".bashrc")) {
            FileEntry* bashrc = create_entry(".bashrc", FILE_TYPE);
            if (bashrc) {
                bashrc->content = strdup("alias ll='ls -l'\nalias la='ls -la'\nalias l='ls -CF'\n");
                bashrc->size = strlen(bashrc->content);
                add_child(user, bashrc);
                fs->file_count++;
            }
            
            FileEntry* readme = create_entry("README.txt", FILE_TYPE);
            if (readme) {
                readme->content = strdup("欢迎使用 LGD OS 1.0.0\n");
                readme->size = strlen(readme->content);
                add_child(user, readme);
                fs->file_count++;
            }
            
            FileEntry* docs = create_entry("Documents", DIR_TYPE);
            if (docs) {
                add_child(user, docs);
                fs->dir_count++;
            }
            
            FileEntry* downloads = create_entry("Downloads", DIR_TYPE);
            if (downloads) {
                add_child(user, downloads);
                fs->dir_count++;
            }
        }
    }
    
    
    if (etc && !find_child(etc, "passwd")) {
        FileEntry* passwd = create_entry("passwd", FILE_TYPE);
        if (passwd) {
            passwd->content = strdup("root:x:0:0:root:/root:/bin/bash\nuser:x:1000:1000:user:/home/user:/bin/bash");
            passwd->size = strlen(passwd->content);
            add_child(etc, passwd);
            fs->file_count++;
        }
    }
}

int cmd_cd(ShellContext* ctx, int argc, char** argv) {
    if (argc > 1) {
        if (!fs_change_dir(ctx->fs, argv[1])) {
            return 1;
        }
    } else {
        fs_change_dir(ctx->fs, "~");
    }
    return 0;
}

int cmd_ls(ShellContext* ctx, int argc, char** argv) {
    int detailed = 0;
    int show_all = 0;
    
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-l") == 0) {
            detailed = 1;
        } else if (strcmp(argv[i], "-a") == 0) {
            show_all = 1;
        } else if (strcmp(argv[i], "-la") == 0 || strcmp(argv[i], "-al") == 0) {
            detailed = 1;
            show_all = 1;
        } else if (argv[i][0] == '-') {
            printf("ls: 无效选项 -- '%s'\n", argv[i] + 1);
            printf("用法: ls [-l] [-a] [-la]\n");
            return 1;
        } else {
            char saved_path[MAX_PATH_LEN];
            strcpy(saved_path, fs_get_current_path(ctx->fs));
            if (fs_change_dir(ctx->fs, argv[i])) {
                fs_list_dir(ctx->fs, detailed, show_all);
                fs_change_dir(ctx->fs, saved_path);
            } else {
                printf("ls: 无法访问 '%s': 没有那个文件或目录\n", argv[i]);
            }
            return 0;
        }
    }
    
    fs_list_dir(ctx->fs, detailed, show_all);
    return 0;
}

int cmd_ll(ShellContext* ctx, int argc, char** argv) {
    char* new_argv[MAX_ARGS + 1];
    new_argv[0] = "ls";
    new_argv[1] = "-l";
    
    for (int i = 1; i < argc && i < MAX_ARGS; i++) {
        new_argv[i + 1] = argv[i];
    }
    
    return cmd_ls(ctx, argc + 1, new_argv);
}

int cmd_pwd(ShellContext* ctx, int argc, char** argv) {
    printf("%s\n", fs_get_current_path(ctx->fs));
    return 0;
}

int cmd_save(ShellContext* ctx, int argc, char** argv) {
    if (!ctx) {
        printf("save: 错误: 上下文为空\n");
        return 1;
    }
    
    char filename[MAX_PATH_LEN] = SAVE_FILE;
    if (argc > 1) {
        strcpy(filename, argv[1]);
    }
    
    FILE* fp = fopen(filename, "w");
    if (!fp) {
        printf("无法保存到文件: %s\n", filename);
        return 1;
    }
    
    
    fprintf(fp, "# LGD OS 1.0.0 Save File\n");
    fprintf(fp, "# Created: %s", ctime(&(time_t){time(NULL)}));
    fprintf(fp, "# Format: D <path> <creation_time> <modify_time>\n");
    fprintf(fp, "#         F <path> <creation_time> <modify_time> <size> <content>\n\n");
    
    
    fprintf(fp, "current_dir: %s\n", fs_get_current_path(ctx->fs));
    
    
    save_file_system_recursive(fp, ctx->fs->root, "/");
    
    fclose(fp);
    printf("文件系统状态已保存到: %s\n", filename);
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
int cmd_load(ShellContext* ctx, int argc, char** argv) {
    if (!ctx) {
        printf("load: 错误: 上下文为空\n");
        return 1;
    }
    
    char filename[MAX_PATH_LEN] = SAVE_FILE;
    if (argc > 1) {
        strcpy(filename, argv[1]);
    }
    
    FILE* fp = fopen(filename, "r");
    if (!fp) {
        printf("无法加载文件: %s\n", filename);
        return 1;
    }
    
    char line[MAX_PATH_LEN * 2];
    int loaded = 0;
    
    
    char saved_current_dir[MAX_PATH_LEN];
    strcpy(saved_current_dir, fs_get_current_path(ctx->fs));
    
    
    FileEntry* old_root = ctx->fs->root;
    
    
    ctx->fs->root = create_entry("", DIR_TYPE);
    strcpy(ctx->fs->root->name, "/");
    ctx->fs->current_dir = ctx->fs->root;
    ctx->fs->file_count = 0;
    ctx->fs->dir_count = 1;
    
    
    free_entry(old_root);
    
    while (fgets(line, sizeof(line), fp)) {
        line[strcspn(line, "\n")] = 0;
        
        if (line[0] == '#' || line[0] == '\0') {
            continue;
        }
        
        if (strncmp(line, "current_dir: ", 13) == 0) {
            char* path = line + 13;
            if (fs_change_dir(ctx->fs, path)) {
                loaded = 1;
            } else {
                printf("警告: 无法切换到目录 %s\n", path);
            }
        } else if (line[0] == 'D' || line[0] == 'F') {
            load_file_system_entry(ctx->fs, line);
        }
    }
    
    fclose(fp);
    
    if (!loaded) {
        printf("存档文件为空或格式错误\n");
        return 1;
    }
    
    printf("从 %s 加载成功\n", filename);
    printf("当前目录: %s\n", fs_get_current_path(ctx->fs));
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
int cmd_mkdir(ShellContext* ctx, int argc, char** argv) {
    if (argc < 2) {
        printf("mkdir: 缺少操作数\n");
        printf("用法: mkdir <目录名>\n");
        return 1;
    }
    
    for (int i = 1; i < argc; i++) {
        
        FileEntry* existing = find_child(ctx->fs->current_dir, argv[i]);
        if (existing) {
            printf("mkdir: 无法创建目录 '%s': 文件已存在\n", argv[i]);
            continue;
        }
        
        
        FileEntry* dir = create_entry(argv[i], DIR_TYPE);
        if (!dir) {
            printf("mkdir: 无法创建目录 '%s': 内存不足\n", argv[i]);
            continue;
        }
        
        add_child(ctx->fs->current_dir, dir);
        ctx->fs->dir_count++;
        
        printf("已创建目录: %s\n", argv[i]);
    }
    
    return 0;
}
int cmd_rmdir(ShellContext* ctx, int argc, char** argv) {
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
        
        if (remove_child(ctx->fs->current_dir, argv[i])) {
            ctx->fs->dir_count--;
            printf("已删除目录: %s\n", argv[i]);
        } else {
            printf("rmdir: 无法删除 '%s': 未知错误\n", argv[i]);
        }
    }
    
    return 0;
}
int cmd_mv(ShellContext* ctx, int argc, char** argv) {
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
int cmd_info(ShellContext* ctx, int argc, char** argv) {
    if (argc < 2) {
        printf("info: 缺少操作数\n");
        printf("用法: info <文件/目录名>\n");
        return 1;
    }
    
    FileEntry* entry = find_child(ctx->fs->current_dir, argv[1]);
    if (!entry) {
        printf("info: 无法获取 '%s' 的信息: 没有那个文件或目录\n", argv[1]);
        return 1;
    }
    
    char create_time[32], modify_time[32];
    strftime(create_time, sizeof(create_time), "%Y-%m-%d %H:%M:%S", 
             localtime(&entry->create_time));
    strftime(modify_time, sizeof(modify_time), "%Y-%m-%d %H:%M:%S", 
             localtime(&entry->modify_time));
    
    printf("名称: %s\n", entry->name);
    printf("类型: %s\n", entry->type == DIR_TYPE ? "目录" : "文件");
    printf("创建时间: %s\n", create_time);
    printf("修改时间: %s\n", modify_time);
    
    if (entry->type == FILE_TYPE) {
        printf("大小: %zu 字节\n", entry->size);
        if (entry->content && argc > 2 && strcmp(argv[2], "-c") == 0) {
            printf("内容预览:\n%s\n", entry->content);
        }
    } else {
        
        int file_count = 0, dir_count = 0;
        FileEntry* child = entry->children;
        while (child) {
            if (child->type == DIR_TYPE) dir_count++;
            else file_count++;
            child = child->next;
        }
        printf("包含: %d 个文件, %d 个子目录\n", file_count, dir_count);
    }
    
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
int cmd_help(ShellContext* ctx, int argc, char** argv) {
    if (argc == 1) {
        printf("LGD OS 1.0.0 命令帮助\n");
        printf("=====================\n\n");
        printf("可用的命令组:\n");
        printf("  1. 目录操作命令: cd, pwd, mkdir, rmdir\n");
        printf("  2. 文件操作命令: ls, ll, rm, mv, info\n");
        printf("  3. 系统操作命令: save, load, history, clear, exit\n");
        printf("  4. 帮助命令: help\n\n");
        printf("输入 'help <命令名>' 获取特定命令的详细帮助\n");
        printf("示例: help cd, help ls, help mkdir\n\n");
        printf("输入 'help all' 显示所有命令的完整帮助信息\n");
        return 0;
    }
    
    if (strcmp(argv[1], "all") == 0) {
        printf("LGD OS 1.0.0 所有命令详细帮助\n");
        printf("=============================\n\n");
        
        printf("1. CD 命令 - 改变当前工作目录\n");
        printf("   用法: cd [目录路径]\n");
        printf("   参数:\n");
        printf("     .        当前目录\n");
        printf("     ..       父目录\n");
        printf("     /        根目录\n");
        printf("     ~        家目录 (/home/user)\n");
        printf("     -        返回上一个目录\n");
        printf("     /..      返回历史目录\n");
        printf("     [path]   任意有效目录路径\n");
        printf("   示例:\n");
        printf("     cd /home             # 切换到/home目录\n");
        printf("     cd ..                # 返回上级目录\n");
        printf("     cd -                 # 返回上一个目录\n");
        printf("     cd /home/user/docs   # 切换到深层目录\n");
        printf("   \n");
        
        printf("2. LS 命令 - 列出目录内容\n");
        printf("   用法: ls [选项] [目录]\n");
        printf("   选项:\n");
        printf("     -l        详细列表（显示权限、大小、时间等）\n");
        printf("     -a        显示所有文件（包括隐藏文件）\n");
        printf("     -la/-al   详细列表显示所有文件\n");
        printf("   参数:\n");
        printf("     [目录]    可选，指定要列出的目录\n");
        printf("   示例:\n");
        printf("     ls                 # 列出当前目录内容\n");
        printf("     ls -l              # 详细列表\n");
        printf("     ls -a              # 显示所有文件\n");
        printf("     ls -la             # 详细列表显示所有文件\n");
        printf("     ls /home           # 列出/home目录内容\n");
        printf("     ls -l /home        # 详细列出/home目录\n");
        printf("   \n");
        
        printf("3. LL 命令 - ls -l 的别名\n");
        printf("   用法: ll [目录]\n");
        printf("   参数:\n");
        printf("     [目录]    可选，指定要列出的目录\n");
        printf("   示例:\n");
        printf("     ll                 # 详细列出当前目录\n");
        printf("     ll /home           # 详细列出/home目录\n");
        printf("   \n");
        
        printf("4. PWD 命令 - 显示当前工作目录的完整路径\n");
        printf("   用法: pwd\n");
        printf("   示例:\n");
        printf("     pwd                # 输出: /home/user\n");
        printf("   \n");
        
        printf("5. MKDIR 命令 - 创建新目录\n");
        printf("   用法: mkdir <目录名> [目录名2 ...]\n");
        printf("   参数:\n");
        printf("     <目录名>   要创建的目录名称（可多个）\n");
        printf("   示例:\n");
        printf("     mkdir docs         # 创建docs目录\n");
        printf("     mkdir a b c        # 创建a, b, c三个目录\n");
        printf("     mkdir /home/user/newdir  # 指定完整路径创建目录\n");
        printf("   \n");
        
        printf("6. RMDIR 命令 - 删除空目录\n");
        printf("   用法: rmdir <目录名> [目录名2 ...]\n");
        printf("   参数:\n");
        printf("     <目录名>   要删除的目录名称（可多个）\n");
        printf("   注意: 只能删除空目录\n");
        printf("   示例:\n");
        printf("     rmdir docs         # 删除docs目录（必须为空）\n");
        printf("     rmdir a b c        # 删除a, b, c三个目录\n");
        printf("   \n");
        
        printf("7. RM 命令 - 删除文件\n");
        printf("   用法: rm <文件名> [文件名2 ...]\n");
        printf("   参数:\n");
        printf("     <文件名>   要删除的文件名称（可多个）\n");
        printf("   注意: 不能删除目录，只能删除文件\n");
        printf("   示例:\n");
        printf("     rm file.txt        # 删除file.txt文件\n");
        printf("     rm a.txt b.txt     # 删除多个文件\n");
        printf("   \n");
        
        printf("8. MV 命令 - 重命名文件或目录\n");
        printf("   用法: mv <原名称> <新名称>\n");
        printf("   参数:\n");
        printf("     <原名称>   原始文件/目录名\n");
        printf("     <新名称>   新的文件/目录名\n");
        printf("   示例:\n");
        printf("     mv old.txt new.txt    # 文件重命名\n");
        printf("     mv olddir newdir      # 目录重命名\n");
        printf("   \n");
        
        printf("9. INFO 命令 - 显示文件或目录的详细信息\n");
        printf("   用法: info <名称> [-c]\n");
        printf("   参数:\n");
        printf("     <名称>     要查看的文件/目录名\n");
        printf("     -c        如果查看文件，显示内容预览\n");
        printf("   示例:\n");
        printf("     info file.txt        # 显示文件信息\n");
        printf("     info docs            # 显示目录信息\n");
        printf("     info file.txt -c     # 显示文件信息和内容\n");
        printf("   \n");
        
        printf("10. SAVE 命令 - 保存当前文件系统状态到文件\n");
        printf("   用法: save [文件名]\n");
        printf("   参数:\n");
        printf("     [文件名]   可选，保存文件名（默认: lgdos_save.txt）\n");
        printf("   注意: 保存文件为纯文本格式，包含创建时间、修改时间等信息\n");
        printf("   示例:\n");
        printf("     save                 # 保存到默认文件\n");
        printf("     save backup.txt      # 保存到backup.txt\n");
        printf("   \n");
        
        printf("11. LOAD 命令 - 从文件加载文件系统状态\n");
        printf("   用法: load [文件名]\n");
        printf("   参数:\n");
        printf("     [文件名]   可选，要加载的文件名（默认: lgdos_save.txt）\n");
        printf("   注意: 加载时会清空当前文件系统\n");
        printf("   示例:\n");
        printf("     load                 # 从默认文件加载\n");
        printf("     load backup.txt      # 从backup.txt加载\n");
        printf("   \n");
        
        printf("12. HISTORY 命令 - 显示目录切换历史\n");
        printf("   用法: history\n");
        printf("   示例:\n");
        printf("     history              # 显示最近访问的目录\n");
        printf("   \n");
        
        printf("13. CLEAR/CLS 命令 - 清屏\n");
        printf("   用法: clear 或 cls\n");
        printf("   示例:\n");
        printf("     clear               # 清空屏幕\n");
        printf("     cls                 # 清空屏幕\n");
        printf("   \n");
        
        printf("14. EXIT/QUIT 命令 - 退出系统\n");
        printf("   用法: exit 或 quit\n");
        printf("   注意: 退出前会自动保存文件系统状态\n");
        printf("   示例:\n");
        printf("     exit                # 退出系统\n");
        printf("     quit                # 退出系统\n");
        printf("   \n");
        
        printf("15. HELP 命令 - 显示帮助信息\n");
        printf("   用法: help [命令名] 或 help all\n");
        printf("   参数:\n");
        printf("     [命令名]   可选，获取特定命令的帮助\n");
        printf("     all        显示所有命令的完整帮助\n");
        printf("   示例:\n");
        printf("     help                # 显示简要帮助\n");
        printf("     help cd             # 显示cd命令的详细帮助\n");
        printf("     help all            # 显示所有命令的详细帮助\n");
        
        return 0;
    }
    
    char* command = argv[1];
    
    if (strcmp(command, "cd") == 0) {
        printf("CD 命令 - 改变当前工作目录\n");
        printf("==========================\n\n");
        printf("用法: cd [目录路径]\n\n");
        printf("参数:\n");
        printf("  .        当前目录\n");
        printf("  ..       父目录\n");
        printf("  /        根目录\n");
        printf("  ~        家目录 (/home/user)\n");
        printf("  -        返回上一个目录\n");
        printf("  /..      返回历史目录\n");
        printf("  [path]   任意有效目录路径\n\n");
        printf("示例:\n");
        printf("  cd /home              # 切换到/home目录\n");
        printf("  cd ..                 # 返回上级目录\n");
        printf("  cd -                  # 返回上一个目录\n");
        printf("  cd /home/user/docs    # 切换到深层目录\n");
        printf("  cd                    # 无参数时回到家目录\n");
        
    } else if (strcmp(command, "ls") == 0) {
        printf("LS 命令 - 列出目录内容\n");
        printf("======================\n\n");
        printf("用法: ls [选项] [目录]\n\n");
        printf("选项:\n");
        printf("  -l        详细列表（显示权限、大小、时间等）\n");
        printf("  -a        显示所有文件（包括隐藏文件）\n");
        printf("  -la/-al   详细列表显示所有文件\n\n");
        printf("参数:\n");
        printf("  [目录]    可选，指定要列出的目录\n\n");
        printf("示例:\n");
        printf("  ls                    # 列出当前目录内容\n");
        printf("  ls -l                 # 详细列表\n");
        printf("  ls -a                 # 显示所有文件\n");
        printf("  ls -la                # 详细列表显示所有文件\n");
        printf("  ls /home              # 列出/home目录内容\n");
        printf("  ls -l /home           # 详细列出/home目录\n");
        
    } else if (strcmp(command, "ll") == 0) {
        printf("LL 命令 - ls -l 的别名\n");
        printf("======================\n\n");
        printf("用法: ll [目录]\n\n");
        printf("参数:\n");
        printf("  [目录]    可选，指定要列出的目录\n\n");
        printf("示例:\n");
        printf("  ll                    # 详细列出当前目录\n");
        printf("  ll /home              # 详细列出/home目录\n");
        
    } else if (strcmp(command, "pwd") == 0) {
        printf("PWD 命令 - 显示当前工作目录的完整路径\n");
        printf("===================================\n\n");
        printf("用法: pwd\n\n");
        printf("示例:\n");
        printf("  pwd                   # 输出: /home/user\n");
        
    } else if (strcmp(command, "mkdir") == 0) {
        printf("MKDIR 命令 - 创建新目录\n");
        printf("=======================\n\n");
        printf("用法: mkdir <目录名> [目录名2 ...]\n\n");
        printf("参数:\n");
        printf("  <目录名>   要创建的目录名称（可多个）\n\n");
        printf("示例:\n");
        printf("  mkdir docs            # 创建docs目录\n");
        printf("  mkdir a b c           # 创建a, b, c三个目录\n");
        printf("  mkdir /home/user/newdir  # 指定完整路径创建目录\n");
        
    } else if (strcmp(command, "rmdir") == 0) {
        printf("RMDIR 命令 - 删除空目录\n");
        printf("=======================\n\n");
        printf("用法: rmdir <目录名> [目录名2 ...]\n\n");
        printf("参数:\n");
        printf("  <目录名>   要删除的目录名称（可多个）\n\n");
        printf("注意:\n");
        printf("  - 只能删除空目录\n");
        printf("  - 目录不存在时会显示错误\n\n");
        printf("示例:\n");
        printf("  rmdir docs            # 删除docs目录（必须为空）\n");
        printf("  rmdir a b c           # 删除a, b, c三个目录\n");
        
    } else if (strcmp(command, "rm") == 0) {
        printf("RM 命令 - 删除文件\n");
        printf("==================\n\n");
        printf("用法: rm <文件名> [文件名2 ...]\n\n");
        printf("参数:\n");
        printf("  <文件名>   要删除的文件名称（可多个）\n\n");
        printf("注意:\n");
        printf("  - 不能删除目录，只能删除文件\n");
        printf("  - 文件不存在时会显示错误\n\n");
        printf("示例:\n");
        printf("  rm file.txt           # 删除file.txt文件\n");
        printf("  rm a.txt b.txt        # 删除多个文件\n");
        
    } else if (strcmp(command, "mv") == 0) {
        printf("MV 命令 - 重命名文件或目录\n");
        printf("=========================\n\n");
        printf("用法: mv <原名称> <新名称>\n\n");
        printf("参数:\n");
        printf("  <原名称>   原始文件/目录名\n");
        printf("  <新名称>   新的文件/目录名\n\n");
        printf("注意:\n");
        printf("  - 新名称已存在时会显示错误\n");
        printf("  - 原名称不存在时会显示错误\n\n");
        printf("示例:\n");
        printf("  mv old.txt new.txt    # 文件重命名\n");
        printf("  mv olddir newdir      # 目录重命名\n");
        
    } else if (strcmp(command, "info") == 0) {
        printf("INFO 命令 - 显示文件或目录的详细信息\n");
        printf("===================================\n\n");
        printf("用法: info <名称> [-c]\n\n");
        printf("参数:\n");
        printf("  <名称>     要查看的文件/目录名\n");
        printf("  -c        如果查看文件，显示内容预览\n\n");
        printf("显示的信息包括:\n");
        printf("  - 名称、类型、创建时间、修改时间\n");
        printf("  - 对于文件: 大小（字节）\n");
        printf("  - 对于目录: 包含的文件和子目录数量\n\n");
        printf("示例:\n");
        printf("  info file.txt         # 显示文件信息\n");
        printf("  info docs             # 显示目录信息\n");
        printf("  info file.txt -c      # 显示文件信息和内容\n");
        
    } else if (strcmp(command, "save") == 0) {
        printf("SAVE 命令 - 保存当前文件系统状态到文件\n");
        printf("====================================\n\n");
        printf("用法: save [文件名]\n\n");
        printf("参数:\n");
        printf("  [文件名]   可选，保存文件名（默认: lgdos_save.txt）\n\n");
        printf("注意:\n");
        printf("  - 保存文件为纯文本格式\n");
        printf("  - 包含目录结构、文件内容、创建时间、修改时间\n");
        printf("  - 保存当前目录位置\n\n");
        printf("示例:\n");
        printf("  save                  # 保存到默认文件\n");
        printf("  save backup.txt       # 保存到backup.txt\n");
        
    } else if (strcmp(command, "load") == 0) {
        printf("LOAD 命令 - 从文件加载文件系统状态\n");
        printf("=================================\n\n");
        printf("用法: load [文件名]\n\n");
        printf("参数:\n");
        printf("  [文件名]   可选，要加载的文件名（默认: lgdos_save.txt）\n\n");
        printf("注意:\n");
        printf("  - 加载时会清空当前文件系统\n");
        printf("  - 恢复保存时的目录结构、文件内容和时间信息\n");
        printf("  - 自动切换到保存时的目录位置\n\n");
        printf("示例:\n");
        printf("  load                  # 从默认文件加载\n");
        printf("  load backup.txt       # 从backup.txt加载\n");
        
    } else if (strcmp(command, "history") == 0) {
        printf("HISTORY 命令 - 显示目录切换历史\n");
        printf("==============================\n\n");
        printf("用法: history\n\n");
        printf("示例:\n");
        printf("  history               # 显示最近访问的目录\n");
        
    } else if (strcmp(command, "clear") == 0 || strcmp(command, "cls") == 0) {
        printf("CLEAR/CLS 命令 - 清屏\n");
        printf("=====================\n\n");
        printf("用法: clear 或 cls\n\n");
        printf("示例:\n");
        printf("  clear                 # 清空屏幕\n");
        printf("  cls                   # 清空屏幕\n");
        
    } else if (strcmp(command, "exit") == 0 || strcmp(command, "quit") == 0) {
        printf("EXIT/QUIT 命令 - 退出系统\n");
        printf("=======================\n\n");
        printf("用法: exit 或 quit\n\n");
        printf("注意:\n");
        printf("  - 退出前会自动保存文件系统状态\n");
        printf("  - 使用'save'命令的默认文件名\n\n");
        printf("示例:\n");
        printf("  exit                  # 退出系统\n");
        printf("  quit                  # 退出系统\n");
        
    } else if (strcmp(command, "help") == 0) {
        printf("HELP 命令 - 显示帮助信息\n");
        printf("=======================\n\n");
        printf("用法: help [命令名] 或 help all\n\n");
        printf("参数:\n");
        printf("  [命令名]   可选，获取特定命令的帮助\n");
        printf("  all        显示所有命令的完整帮助\n\n");
        printf("示例:\n");
        printf("  help                  # 显示简要帮助\n");
        printf("  help cd               # 显示cd命令的详细帮助\n");
        printf("  help all              # 显示所有命令的详细帮助\n");
        
    } else {
        printf("未知命令: %s\n", command);
        printf("输入 'help' 查看可用命令\n");
        return 1;
    }
    
    return 0;
}

int cmd_clear(ShellContext* ctx, int argc, char** argv) {
    system("cls");
    return 0;
}

int cmd_exit(ShellContext* ctx, int argc, char** argv) {
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
    
    if (strcmp(args[0], "cd") == 0) {
        cmd_cd(ctx, argc, args);
    }
    else if (strcmp(args[0], "ls") == 0) {
        cmd_ls(ctx, argc, args);
    }
    else if (strcmp(args[0], "ll") == 0) {
        cmd_ll(ctx, argc, args);
    }
    else if (strcmp(args[0], "pwd") == 0) {
        cmd_pwd(ctx, argc, args);
    }
    else if (strcmp(args[0], "mkdir") == 0) {
        cmd_mkdir(ctx, argc, args);
    }
    else if (strcmp(args[0], "rmdir") == 0) {
        cmd_rmdir(ctx, argc, args);
    }
    else if (strcmp(args[0], "rm") == 0) {
        cmd_rm(ctx, argc, args);
    }
    else if (strcmp(args[0], "mv") == 0) {
        cmd_mv(ctx, argc, args);
    }
    else if (strcmp(args[0], "info") == 0) {
        cmd_info(ctx, argc, args);
    }
    else if (strcmp(args[0], "save") == 0) {
        cmd_save(ctx, argc, args);
    }
    else if (strcmp(args[0], "load") == 0) {
        cmd_load(ctx, argc, args);
    }
    else if (strcmp(args[0], "history") == 0) {
        cmd_history(ctx, argc, args);
    }
    else if (strcmp(args[0], "help") == 0) {
        cmd_help(ctx, argc, args);
    }
    else if (strcmp(args[0], "clear") == 0 || strcmp(args[0], "cls") == 0) {
        cmd_clear(ctx, argc, args);
    }
    else if (strcmp(args[0], "exit") == 0 || strcmp(args[0], "quit") == 0) {
        cmd_exit(ctx, argc, args);
    }
    else {
        printf("%s: 未找到命令\n", args[0]);
        printf("输入 'help' 查看可用命令\n");
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
    printf("      LGD OS 1.0.0\n");
    printf("========================================\n");
    
    printf("正在检查存档文件...\n");
    
    
    FileSystem* fs = fs_init();
    if (!fs) {
        printf("错误: 无法初始化文件系统\n");
        return 1;
    }
    
    
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
    
    
    FILE* save_file = fopen(SAVE_FILE, "r");
    if (save_file) {
        printf("找到存档文件，正在加载...\n");
        fclose(save_file);
        
        
        char* load_argv[] = {"load", NULL};
        if (cmd_load(&ctx, 1, load_argv) != 0) {
            printf("加载失败，创建默认文件系统...\n");
            setup_sample_filesystem(fs);
        }
    } else {
        printf("无存档文件，创建默认文件系统...\n");
        setup_sample_filesystem(fs);
    }
    
    printf("\n系统启动完成！\n");
    printf("当前用户: %s\n", ctx.username);
    printf("主机名: %s\n", ctx.hostname);
    printf("当前目录: %s\n\n", fs_get_current_path(fs));
    
    
    shell_run(&ctx);
    
    
    printf("正在保存文件系统状态...\n");
    char* save_argv[] = {"save", NULL};
    cmd_save(&ctx, 1, save_argv);
    
    printf("正在关闭系统...\n");
    fs_destroy(fs);
    
    printf("LGD OS 1.0.0 已安全关闭。\n");
    
    return 0;
}
