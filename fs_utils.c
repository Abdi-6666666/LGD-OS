// fs_utils.c
#include "declarations.h"
#include <io.h>
#include <time.h>

// 确保builds目录存在
int ensure_builds_directory(void) {
    // 检查目录是否存在
    if (_access(BUILDS_DIR, 0) != 0) {
        // 目录不存在，创建它
        if (_mkdir(BUILDS_DIR) != 0) {
            return 0;
        }
    }
    return 1;
}

// 创建目录（如果不存在）- 静默版本
int create_directory_if_not_exists_silent(const char *path) {
    if (_access(path, 0) != 0) {
        if (_mkdir(path) != 0) {
            return 0;  // 创建失败
        }
    }
    return 1;  // 目录已存在或创建成功
}

// 创建目录（如果不存在）
int create_directory_if_not_exists(const char *path) {
    int result = create_directory_if_not_exists_silent(path);
    if (result) {
        printf("已创建目录: %s\n", path);
    }
    return result;
}

// 同步现有目录
void sync_existing_directories_silent(void) {
    // Windows版本，遍历builds目录
    struct _finddata_t fileinfo;
    intptr_t handle;

    if ((handle = _findfirst(BUILDS_DIR "\\*.*", &fileinfo)) == -1L) {
        return;
    }

    do {
        if (strcmp(fileinfo.name, ".") == 0 || strcmp(fileinfo.name, "..") == 0) {
            continue;
        }

        if (fileinfo.attrib & _A_SUBDIR) {
            // 这是一个目录
        }
    } while (_findnext(handle, &fileinfo) == 0);

    _findclose(handle);
}

// 创建Linux风格目录结构
void create_linux_directory_structure(void) {
    if (!ensure_builds_directory()) {
        printf("错误: 无法访问builds目录\n");
        return;
    }

    // 保存当前目录
    char original_dir[MAX_PATH_LEN];
    if (_getcwd(original_dir, MAX_PATH_LEN) == NULL) {
        strcpy(original_dir, ".");
    }

    // 进入builds目录
    if (_chdir(BUILDS_DIR) != 0) {
        printf("错误: 无法进入builds目录\n");
        return;
    }

    printf("正在创建Linux风格目录结构...\n");

    // 创建标准Linux目录
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

    // 创建子目录
    create_directory_if_not_exists_silent("home\\user");
    create_directory_if_not_exists_silent("var\\log");
    create_directory_if_not_exists_silent("usr\\bin");
    create_directory_if_not_exists_silent("usr\\lib");
    create_directory_if_not_exists_silent("usr\\local");
    create_directory_if_not_exists_silent("var\\tmp");
    create_directory_if_not_exists_silent("var\\cache");
    create_directory_if_not_exists_silent("opt\\local");

    // 创建示例文件
    FILE *fp = fopen("etc\\version", "w");
    if (fp) {
        fprintf(fp, "%s\n", LGD_OS_VERSION);
        fprintf(fp, "这是一个模拟的Linux文件系统\n");
        fprintf(fp, "运行在Windows平台上\n");
        fclose(fp);
    }

    fp = fopen("home\\user\\README.txt", "w");
    if (fp) {
        fprintf(fp, "欢迎使用 LGD-OS Shell\n");
        fprintf(fp, "这是一个在Windows上运行的Shell环境\n");
        fprintf(fp, "所有操作都在 builds\\ 目录下进行\n");
        fclose(fp);
    }

    fp = fopen("var\\log\\system.log", "w");
    if (fp) {
        fprintf(fp, "系统启动日志\n");
        time_t now = time(NULL);
        fprintf(fp, "启动时间: %s", ctime(&now));
        fclose(fp);
    }

    // 返回到原始目录
    _chdir(original_dir);

    printf("Linux风格目录结构创建完成！\n");
    printf("所有操作将在 builds\\ 目录下进行\n\n");
}
