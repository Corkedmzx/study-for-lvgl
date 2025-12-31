/**
 * @file file_scanner.c
 * @brief 文件扫描模块实现
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <dirent.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdbool.h>
#include "file_scanner.h"

// 图片文件相关全局变量（非static，供其他模块访问）
char **image_files = NULL;
char **image_names = NULL;
int image_count = 0;

// 音频文件相关全局变量（非static，供其他模块访问）
char **audio_files = NULL;
char **audio_names = NULL;
int audio_count = 0;

// 视频文件相关全局变量（非static，供其他模块访问）
char **video_files = NULL;
char **video_names = NULL;
int video_count = 0;

int scan_image_directory(const char *dir_path) {
    DIR *dir;
    struct dirent *entry;
    struct stat st;
    char full_path[512];
    int count = 0;
    int capacity = 32;  // 初始容量
    
    // 先释放旧的数组（如果存在）
    free_image_arrays();
    
    // 分配初始内存
    image_files = (char **)malloc(capacity * sizeof(char *));
    image_names = (char **)malloc(capacity * sizeof(char *));
    if (image_files == NULL || image_names == NULL) {
        printf("错误: 内存分配失败\n");
        return -1;
    }
    
    // 打开目录
    dir = opendir(dir_path);
    if (dir == NULL) {
        printf("错误: 无法打开目录 %s: %s\n", dir_path, strerror(errno));
        free(image_files);
        free(image_names);
        image_files = NULL;
        image_names = NULL;
        return -1;
    }
    
    printf("开始扫描目录: %s\n", dir_path);
    
    // 遍历目录中的文件
    while ((entry = readdir(dir)) != NULL) {
        // 跳过 . 和 ..
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        
        // 构建完整路径
        snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, entry->d_name);
        
        // 检查文件类型
        if (stat(full_path, &st) != 0) {
            continue;  // 跳过无法访问的文件
        }
        
        // 只处理普通文件
        if (!S_ISREG(st.st_mode)) {
            continue;
        }
        
        // 检查文件扩展名
        const char *ext = strrchr(entry->d_name, '.');
        if (ext == NULL) {
            continue;
        }
        
        bool is_bmp = (strcasecmp(ext, ".bmp") == 0);
        bool is_gif = (strcasecmp(ext, ".gif") == 0);
        
        if (!is_bmp && !is_gif) {
            continue;  // 跳过非BMP/GIF文件
        }
        
        // 如果数组已满，扩容
        if (count >= capacity) {
            capacity *= 2;
            char **new_files = (char **)realloc(image_files, capacity * sizeof(char *));
            char **new_names = (char **)realloc(image_names, capacity * sizeof(char *));
            if (new_files == NULL || new_names == NULL) {
                printf("错误: 内存重新分配失败\n");
                // 释放已分配的内存
                for (int i = 0; i < count; i++) {
                    free(image_files[i]);
                    free(image_names[i]);
                }
                free(image_files);
                free(image_names);
                closedir(dir);
                return -1;
            }
            image_files = new_files;
            image_names = new_names;
        }
        
        // 分配并复制文件路径
        image_files[count] = (char *)malloc(strlen(full_path) + 1);
        if (image_files[count] == NULL) {
            printf("错误: 无法分配内存存储文件路径\n");
            continue;
        }
        strcpy(image_files[count], full_path);
        
        // 生成显示名称（使用文件名，不含路径和扩展名）
        char name_buf[256];
        char temp_buf[256];
        int name_len = ext - entry->d_name;
        // 为后缀留出空间：" (BMP)" 或 " (GIF)" 最多6字节
        const char *suffix = is_bmp ? " (BMP)" : " (GIF)";
        int suffix_len = strlen(suffix);
        int max_name_len = sizeof(name_buf) - suffix_len - 1;  // 保留结束符
        if (name_len > max_name_len) name_len = max_name_len;
        strncpy(temp_buf, entry->d_name, name_len);
        temp_buf[name_len] = '\0';
        
        // 添加类型标识（使用临时缓冲区避免覆盖）
        // 已确保有足够空间，抑制编译器误报警告
        #pragma GCC diagnostic push
        #pragma GCC diagnostic ignored "-Wformat-truncation"
        snprintf(name_buf, sizeof(name_buf), "%s%s", temp_buf, suffix);
        #pragma GCC diagnostic pop
        
        image_names[count] = (char *)malloc(strlen(name_buf) + 1);
        if (image_names[count] == NULL) {
            printf("错误: 无法分配内存存储文件名\n");
            free(image_files[count]);
            continue;
        }
        strcpy(image_names[count], name_buf);
        
        printf("找到图片文件[%d]: %s (%s)\n", count, full_path, name_buf);
        count++;
    }
    
    closedir(dir);
    
    // 添加NULL终止符
    if (count < capacity) {
        image_files[count] = NULL;
        image_names[count] = NULL;
    } else {
        // 如果数组已满，需要再分配一个位置用于NULL
        char **new_files = (char **)realloc(image_files, (capacity + 1) * sizeof(char *));
        char **new_names = (char **)realloc(image_names, (capacity + 1) * sizeof(char *));
        if (new_files != NULL && new_names != NULL) {
            image_files = new_files;
            image_names = new_names;
            image_files[count] = NULL;
            image_names[count] = NULL;
        }
    }
    
    image_count = count;
    printf("扫描完成，共找到 %d 个图片文件\n", count);
    
    return count;
}

int scan_audio_directory(const char *dir_path) {
    DIR *dir;
    struct dirent *entry;
    struct stat st;
    char full_path[512];
    int count = 0;
    int capacity = 32;  // 初始容量
    
    // 先释放旧的数组（如果存在）
    free_audio_arrays();
    
    // 分配初始内存
    audio_files = (char **)malloc(capacity * sizeof(char *));
    audio_names = (char **)malloc(capacity * sizeof(char *));
    if (audio_files == NULL || audio_names == NULL) {
        printf("错误: 内存分配失败\n");
        return -1;
    }
    
    // 打开目录
    dir = opendir(dir_path);
    if (dir == NULL) {
        printf("错误: 无法打开目录 %s: %s\n", dir_path, strerror(errno));
        free(audio_files);
        free(audio_names);
        audio_files = NULL;
        audio_names = NULL;
        return -1;
    }
    
    printf("开始扫描音频目录: %s\n", dir_path);
    
    // 遍历目录中的文件
    while ((entry = readdir(dir)) != NULL) {
        // 跳过 . 和 ..
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        
        // 构建完整路径
        snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, entry->d_name);
        
        // 检查文件类型
        if (stat(full_path, &st) != 0) {
            continue;  // 跳过无法访问的文件
        }
        
        // 只处理普通文件
        if (!S_ISREG(st.st_mode)) {
            continue;
        }
        
        // 检查文件扩展名
        const char *ext = strrchr(entry->d_name, '.');
        if (ext == NULL) {
            continue;
        }
        
        // 支持的音频格式
        bool is_audio = (strcasecmp(ext, ".mp3") == 0 ||
                         strcasecmp(ext, ".wav") == 0 ||
                         strcasecmp(ext, ".ogg") == 0 ||
                         strcasecmp(ext, ".flac") == 0 ||
                         strcasecmp(ext, ".aac") == 0 ||
                         strcasecmp(ext, ".m4a") == 0);
        
        if (!is_audio) {
            continue;  // 跳过非音频文件
        }
        
        // 如果数组已满，扩容
        if (count >= capacity) {
            capacity *= 2;
            char **new_files = (char **)realloc(audio_files, capacity * sizeof(char *));
            char **new_names = (char **)realloc(audio_names, capacity * sizeof(char *));
            if (new_files == NULL || new_names == NULL) {
                printf("错误: 内存重新分配失败\n");
                // 释放已分配的内存
                for (int i = 0; i < count; i++) {
                    free(audio_files[i]);
                    free(audio_names[i]);
                }
                free(audio_files);
                free(audio_names);
                closedir(dir);
                return -1;
            }
            audio_files = new_files;
            audio_names = new_names;
        }
        
        // 分配并复制文件路径
        audio_files[count] = (char *)malloc(strlen(full_path) + 1);
        if (audio_files[count] == NULL) {
            printf("错误: 无法分配内存存储文件路径\n");
            continue;
        }
        strcpy(audio_files[count], full_path);
        
        // 生成显示名称（使用文件名，不含路径和扩展名）
        char name_buf[256];
        char temp_buf[256];
        int name_len = ext - entry->d_name;
        const char *suffix = " (音频)";
        int suffix_len = strlen(suffix);
        int max_name_len = sizeof(name_buf) - suffix_len - 1;
        if (name_len > max_name_len) name_len = max_name_len;
        strncpy(temp_buf, entry->d_name, name_len);
        temp_buf[name_len] = '\0';
        
        #pragma GCC diagnostic push
        #pragma GCC diagnostic ignored "-Wformat-truncation"
        snprintf(name_buf, sizeof(name_buf), "%s%s", temp_buf, suffix);
        #pragma GCC diagnostic pop
        
        audio_names[count] = (char *)malloc(strlen(name_buf) + 1);
        if (audio_names[count] == NULL) {
            printf("错误: 无法分配内存存储文件名\n");
            free(audio_files[count]);
            continue;
        }
        strcpy(audio_names[count], name_buf);
        
        printf("找到音频文件[%d]: %s (%s)\n", count, full_path, name_buf);
        count++;
    }
    
    closedir(dir);
    
    // 添加NULL终止符
    if (count < capacity) {
        audio_files[count] = NULL;
        audio_names[count] = NULL;
    } else {
        char **new_files = (char **)realloc(audio_files, (capacity + 1) * sizeof(char *));
        char **new_names = (char **)realloc(audio_names, (capacity + 1) * sizeof(char *));
        if (new_files != NULL && new_names != NULL) {
            audio_files = new_files;
            audio_names = new_names;
            audio_files[count] = NULL;
            audio_names[count] = NULL;
        }
    }
    
    audio_count = count;
    printf("扫描完成，共找到 %d 个音频文件\n", count);
    
    return count;
}

int scan_video_directory(const char *dir_path) {
    DIR *dir;
    struct dirent *entry;
    struct stat st;
    char full_path[512];
    int count = 0;
    int capacity = 32;  // 初始容量
    
    // 先释放旧的数组（如果存在）
    free_video_arrays();
    
    // 分配初始内存
    video_files = (char **)malloc(capacity * sizeof(char *));
    video_names = (char **)malloc(capacity * sizeof(char *));
    if (video_files == NULL || video_names == NULL) {
        printf("错误: 内存分配失败\n");
        return -1;
    }
    
    // 打开目录
    dir = opendir(dir_path);
    if (dir == NULL) {
        printf("错误: 无法打开目录 %s: %s\n", dir_path, strerror(errno));
        free(video_files);
        free(video_names);
        video_files = NULL;
        video_names = NULL;
        return -1;
    }
    
    printf("开始扫描视频目录: %s\n", dir_path);
    
    // 遍历目录中的文件
    while ((entry = readdir(dir)) != NULL) {
        // 跳过 . 和 ..
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        
        // 构建完整路径
        snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, entry->d_name);
        
        // 检查文件类型
        if (stat(full_path, &st) != 0) {
            continue;  // 跳过无法访问的文件
        }
        
        // 只处理普通文件
        if (!S_ISREG(st.st_mode)) {
            continue;
        }
        
        // 检查文件扩展名
        const char *ext = strrchr(entry->d_name, '.');
        if (ext == NULL) {
            continue;
        }
        
        // 支持的视频格式
        bool is_video = (strcasecmp(ext, ".mp4") == 0 ||
                         strcasecmp(ext, ".avi") == 0 ||
                         strcasecmp(ext, ".mkv") == 0 ||
                         strcasecmp(ext, ".mov") == 0 ||
                         strcasecmp(ext, ".flv") == 0 ||
                         strcasecmp(ext, ".wmv") == 0);
        
        if (!is_video) {
            continue;  // 跳过非视频文件
        }
        
        // 如果数组已满，扩容
        if (count >= capacity) {
            capacity *= 2;
            char **new_files = (char **)realloc(video_files, capacity * sizeof(char *));
            char **new_names = (char **)realloc(video_names, capacity * sizeof(char *));
            if (new_files == NULL || new_names == NULL) {
                printf("错误: 内存重新分配失败\n");
                // 释放已分配的内存
                for (int i = 0; i < count; i++) {
                    free(video_files[i]);
                    free(video_names[i]);
                }
                free(video_files);
                free(video_names);
                closedir(dir);
                return -1;
            }
            video_files = new_files;
            video_names = new_names;
        }
        
        // 分配并复制文件路径
        video_files[count] = (char *)malloc(strlen(full_path) + 1);
        if (video_files[count] == NULL) {
            printf("错误: 无法分配内存存储文件路径\n");
            continue;
        }
        strcpy(video_files[count], full_path);
        
        // 生成显示名称（使用文件名，不含路径和扩展名）
        char name_buf[256];
        char temp_buf[256];
        int name_len = ext - entry->d_name;
        const char *suffix = " (视频)";
        int suffix_len = strlen(suffix);
        int max_name_len = sizeof(name_buf) - suffix_len - 1;
        if (name_len > max_name_len) name_len = max_name_len;
        strncpy(temp_buf, entry->d_name, name_len);
        temp_buf[name_len] = '\0';
        
        #pragma GCC diagnostic push
        #pragma GCC diagnostic ignored "-Wformat-truncation"
        snprintf(name_buf, sizeof(name_buf), "%s%s", temp_buf, suffix);
        #pragma GCC diagnostic pop
        
        video_names[count] = (char *)malloc(strlen(name_buf) + 1);
        if (video_names[count] == NULL) {
            printf("错误: 无法分配内存存储文件名\n");
            free(video_files[count]);
            continue;
        }
        strcpy(video_names[count], name_buf);
        
        printf("找到视频文件[%d]: %s (%s)\n", count, full_path, name_buf);
        count++;
    }
    
    closedir(dir);
    
    // 添加NULL终止符
    if (count < capacity) {
        video_files[count] = NULL;
        video_names[count] = NULL;
    } else {
        char **new_files = (char **)realloc(video_files, (capacity + 1) * sizeof(char *));
        char **new_names = (char **)realloc(video_names, (capacity + 1) * sizeof(char *));
        if (new_files != NULL && new_names != NULL) {
            video_files = new_files;
            video_names = new_names;
            video_files[count] = NULL;
            video_names[count] = NULL;
        }
    }
    
    video_count = count;
    printf("扫描完成，共找到 %d 个视频文件\n", count);
    
    return count;
}

void free_image_arrays(void) {
    if (image_files != NULL) {
        for (int i = 0; i < image_count; i++) {
            if (image_files[i] != NULL) {
                free(image_files[i]);
            }
        }
        free(image_files);
        image_files = NULL;
    }
    
    if (image_names != NULL) {
        for (int i = 0; i < image_count; i++) {
            if (image_names[i] != NULL) {
                free(image_names[i]);
            }
        }
        free(image_names);
        image_names = NULL;
    }
    
    image_count = 0;
}

void free_audio_arrays(void) {
    if (audio_files != NULL) {
        for (int i = 0; i < audio_count; i++) {
            if (audio_files[i] != NULL) {
                free(audio_files[i]);
            }
        }
        free(audio_files);
        audio_files = NULL;
    }
    
    if (audio_names != NULL) {
        for (int i = 0; i < audio_count; i++) {
            if (audio_names[i] != NULL) {
                free(audio_names[i]);
            }
        }
        free(audio_names);
        audio_names = NULL;
    }
    
    audio_count = 0;
}

void free_video_arrays(void) {
    if (video_files != NULL) {
        for (int i = 0; i < video_count; i++) {
            if (video_files[i] != NULL) {
                free(video_files[i]);
            }
        }
        free(video_files);
        video_files = NULL;
    }
    
    if (video_names != NULL) {
        for (int i = 0; i < video_count; i++) {
            if (video_names[i] != NULL) {
                free(video_names[i]);
            }
        }
        free(video_names);
        video_names = NULL;
    }
    
    video_count = 0;
}

char **get_image_files(void) {
    return image_files;
}

char **get_image_names(void) {
    return image_names;
}

int get_image_count(void) {
    return image_count;
}

char **get_audio_files(void) {
    return audio_files;
}

char **get_audio_names(void) {
    return audio_names;
}

int get_audio_count(void) {
    return audio_count;
}

char **get_video_files(void) {
    return video_files;
}

char **get_video_names(void) {
    return video_names;
}

int get_video_count(void) {
    return video_count;
}

