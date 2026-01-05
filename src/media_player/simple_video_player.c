/**
 * @file simple_video_player.c
 * @brief 简单的MPlayer全屏视频播放器实现
 * 
 * 实现方案（参考video.c，但添加FIFO控制和性能优化）：
 * 1. 使用MPlayer全屏播放视频（-vo fbdev2）
 * 2. 使用fork+execlp启动（参考video.c）
 * 3. 使用FIFO多线程控制（恢复FIFO，用于控制功能）
 * 4. 添加性能优化参数（-framedrop等）
 */

#include "simple_video_player.h"
#include "../file_scanner/file_scanner.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <signal.h>
#include <pthread.h>
#include <errno.h>

// 播放器状态
static bool is_playing = false;
static bool is_paused = false;  // 暂停状态
static pid_t mplayer_pid = -1;
static int mplayer_control_pipe = -1;
static pthread_mutex_t player_mutex = PTHREAD_MUTEX_INITIALIZER;

// 当前播放文件索引（使用全局变量）
extern int current_video_index;

// 播放速度（1.0为正常速度）
static float playback_speed = 1.0f;

/**
 * @brief 判断文件是否为视频文件
 */
static bool is_video_file(const char *file_path) {
    const char *ext = strrchr(file_path, '.');
    if (ext == NULL) return false;
    
    return (strcasecmp(ext, ".mp4") == 0 ||
            strcasecmp(ext, ".avi") == 0 ||
            strcasecmp(ext, ".mkv") == 0 ||
            strcasecmp(ext, ".mov") == 0 ||
            strcasecmp(ext, ".flv") == 0 ||
            strcasecmp(ext, ".wmv") == 0);
}

/**
 * @brief 发送MPlayer控制命令
 */
static void send_mplayer_command(const char *cmd) {
    if (cmd != NULL && mplayer_control_pipe != -1) {
        char full_cmd[256];
        snprintf(full_cmd, sizeof(full_cmd), "%s\n", cmd);
        (void)write(mplayer_control_pipe, full_cmd, strlen(full_cmd));
        fsync(mplayer_control_pipe);
    }
}

/**
 * @brief 启动MPlayer进程（参考video.c，但添加FIFO控制和性能优化参数）
 */
static bool start_mplayer(const char *file_path) {
    // 先停止之前的MPlayer进程（如果有，参考video.c的video_stop）
    if (mplayer_pid != -1) {
        kill(mplayer_pid, SIGTERM);
        waitpid(mplayer_pid, NULL, 0);
        mplayer_pid = -1;
    }
    
    // 创建FIFO管道（用于多线程控制）
    (void)system("rm -f /tmp/mplayer_fifo");
    if (mkfifo("/tmp/mplayer_fifo", 0666) == -1 && errno != EEXIST) {
        perror("mkfifo");
        return false;
    }
    
    // 使用fork+execlp方式启动MPlayer（参考video.c，但添加-slave和性能优化参数）
    mplayer_pid = fork();
    if (mplayer_pid == 0) {
        // 子进程：执行MPlayer（参考video.c，但添加-slave和性能优化）
        // 设置framebuffer环境（参考video.c）
        putenv("SDL_VIDEODRIVER=fbcon");
        putenv("SDL_FBDEV=/dev/fb0");
        
        // 优先尝试mplayer，结合参考代码的简单方式和已验证可用的格式
        // 开发板信息：虚拟分辨率800x1440，色深32位，实际显示800x480
        // 已验证：fbdev + bgra格式能正常播放视频
        // 方案：使用参考代码的参数顺序和简单方式，但使用fbdev + bgra格式（已验证可用）
        // 注意：保留-slave和FIFO用于触控功能，但退出时使用参考代码的简单方式
        execlp("mplayer", "mplayer",
              "-vo", "fbdev2",        // 使用fbdev（已验证可用，参考video.c使用fbdev2但改为fbdev）
              "-fs",
              "-zoom",
              "-quiet",
              "-slave",              // 保留slave模式用于FIFO控制（触控功能需要）
              "-input", "file=/tmp/mplayer_fifo",  // FIFO控制（触控功能需要）
              "-x", "800",           // 强制输出宽度为800（参考video.c）
              "-y", "480",           // 强制输出高度为480（参考video.c）
              "-vf", "format=bgra",  // 转换为BGRA格式（32位，已验证可用）
              "-lavdopts", "skiploopfilter=all",  // 快速解码（参考video.c）
              "-framedrop",          // 抽帧减少卡顿（性能优化）
              "-autosync", "30",     // 自动同步（性能优化）
              "-cache", "32768",     // 缓存大小（性能优化）
              "-cache-min", "50",    // 最小缓存（性能优化）
              "-ao", "oss",          // 音频输出
              file_path,
              NULL);
              
        // 如果所有方案都失败，直接退出
        perror("Failed to play video with mplayer (all methods failed)");
        exit(1);
    } else if (mplayer_pid < 0) {
        // fork失败
        perror("fork");
        return false;
    }
    
    // 父进程：等待MPlayer启动
    usleep(1500000);  // 1.5秒，等待MPlayer启动
    
    // 验证进程是否还在运行
    int status;
    pid_t result = waitpid(mplayer_pid, &status, WNOHANG);
    if (result == mplayer_pid) {
        // 进程已退出
        printf("错误: MPlayer启动后立即退出\n");
        mplayer_pid = -1;
        return false;
    } else if (result == -1) {
        // 错误
        perror("waitpid");
        mplayer_pid = -1;
        return false;
    }
    
    // 打开FIFO用于发送控制命令
    mplayer_control_pipe = open("/tmp/mplayer_fifo", O_WRONLY | O_NONBLOCK);
    if (mplayer_control_pipe == -1) {
        printf("警告: 无法打开MPlayer控制FIFO\n");
        // 尝试再次打开
        usleep(500000);  // 500ms
        mplayer_control_pipe = open("/tmp/mplayer_fifo", O_WRONLY | O_NONBLOCK);
        if (mplayer_control_pipe == -1) {
            printf("错误: 无法打开MPlayer控制FIFO\n");
            // 即使FIFO打开失败，如果进程在运行，也认为启动成功
        }
    }
    
    printf("[视频播放] MPlayer已启动，PID: %d\n", mplayer_pid);
    return true;
}

/**
 * @brief 停止MPlayer进程（参考video.c，但先尝试FIFO命令）
 */
static void stop_mplayer(void) {
    if (mplayer_pid != -1) {
        // 先尝试通过FIFO发送quit命令
        if (mplayer_control_pipe != -1) {
            send_mplayer_command("quit");
            close(mplayer_control_pipe);
            mplayer_control_pipe = -1;
            usleep(300000);  // 等待300ms
        }
        
        // 使用SIGTERM终止进程（参考video.c）
        kill(mplayer_pid, SIGTERM);
        // 等待进程退出（参考video.c的waitpid）
        waitpid(mplayer_pid, NULL, 0);
        mplayer_pid = -1;
    }
    
    // 清理FIFO
    (void)system("rm -f /tmp/mplayer_fifo");
}

/**
 * @brief 强制停止MPlayer进程（参考video.c，但先尝试FIFO命令）
 */
static void force_stop_mplayer(void) {
    printf("[视频播放] 开始强制停止MPlayer\n");
    
    // 先尝试通过FIFO发送quit命令
    if (mplayer_control_pipe != -1) {
        send_mplayer_command("quit");
        close(mplayer_control_pipe);
        mplayer_control_pipe = -1;
        usleep(200000);  // 等待200ms
    }
    
    // 使用SIGTERM终止进程（参考video.c）
    if (mplayer_pid != -1) {
        kill(mplayer_pid, SIGTERM);
        waitpid(mplayer_pid, NULL, 0);
        mplayer_pid = -1;
    }
    
    // 清理FIFO
    (void)system("rm -f /tmp/mplayer_fifo");
    
    printf("[视频播放] MPlayer已完全停止\n");
}

/**
 * @brief 初始化视频播放器
 */
void simple_video_init(void) {
    is_playing = false;
    is_paused = false;
    mplayer_pid = -1;
    mplayer_control_pipe = -1;
    playback_speed = 1.0f;
}

/**
 * @brief 播放视频文件（全屏）
 * @param file_path 视频文件路径
 * @return 成功返回true，失败返回false
 */
bool simple_video_play(const char *file_path) {
    pthread_mutex_lock(&player_mutex);
    
    // 如果正在播放，先停止并等待完全退出
    if (is_playing) {
        stop_mplayer();
        is_playing = false;
        is_paused = false;
        // 等待MPlayer完全退出和framebuffer恢复
        usleep(500000);  // 500ms
    }
    
    if (file_path == NULL || !is_video_file(file_path)) {
        pthread_mutex_unlock(&player_mutex);
        return false;
    }
    
    // 启动MPlayer
    if (start_mplayer(file_path)) {
        is_playing = true;
        is_paused = false;
        playback_speed = 1.0f;
        pthread_mutex_unlock(&player_mutex);
        return true;
    }
    
    pthread_mutex_unlock(&player_mutex);
    return false;
}

/**
 * @brief 停止播放（参考video.c，但先尝试FIFO命令）
 */
void simple_video_stop(void) {
    pthread_mutex_lock(&player_mutex);
    
    if (is_playing) {
        // 先尝试通过FIFO发送quit命令
        if (mplayer_control_pipe != -1) {
            send_mplayer_command("quit");
            close(mplayer_control_pipe);
            mplayer_control_pipe = -1;
            usleep(200000);  // 等待200ms
        }
        
        // 使用SIGTERM终止进程（参考video.c）
        if (mplayer_pid != -1) {
            kill(mplayer_pid, SIGTERM);
            waitpid(mplayer_pid, NULL, 0);
            mplayer_pid = -1;
        }
        
        // 清理FIFO
        (void)system("rm -f /tmp/mplayer_fifo");
        
        is_playing = false;
        is_paused = false;
    }
    
    pthread_mutex_unlock(&player_mutex);
}

/**
 * @brief 强制停止播放（单线程退出逻辑，不使用互斥锁）
 */
void simple_video_force_stop(void) {
    // 不使用互斥锁，直接停止（单线程退出逻辑）
    force_stop_mplayer();
    
    // 更新状态（不使用互斥锁）
    is_playing = false;
    is_paused = false;
}

/**
 * @brief 暂停/恢复播放
 */
void simple_video_toggle_pause(void) {
    pthread_mutex_lock(&player_mutex);
    
    if (is_playing && mplayer_control_pipe != -1) {
        send_mplayer_command("pause");
        is_paused = !is_paused;  // 切换暂停状态
        printf("视频%s\n", is_paused ? "已暂停" : "已恢复播放");
    }
    
    pthread_mutex_unlock(&player_mutex);
}

/**
 * @brief 上一首
 */
void simple_video_prev(void) {
    pthread_mutex_lock(&player_mutex);
    
    if (!is_playing) {
        pthread_mutex_unlock(&player_mutex);
        return;
    }
    
    // 查找上一个视频文件
    extern char **video_files;
    extern int video_count;
    
    if (video_files == NULL || video_count == 0) {
        pthread_mutex_unlock(&player_mutex);
        return;
    }
    
    extern int current_video_index;
    int start_index = (current_video_index >= 0) ? current_video_index : 0;
    int index = start_index;
    
    do {
        index--;
        if (index < 0) {
            index = video_count - 1;
        }
        
        if (index == start_index) {
            // 已经循环一圈，没有找到其他视频
            pthread_mutex_unlock(&player_mutex);
            return;
        }
        
        const char *prev_file = video_files[index];
        if (prev_file != NULL && is_video_file(prev_file)) {
            // 停止当前播放
            if (mplayer_control_pipe != -1) {
                send_mplayer_command("quit");
                close(mplayer_control_pipe);
                mplayer_control_pipe = -1;
            }
            
            if (mplayer_pid != -1) {
                kill(mplayer_pid, SIGTERM);
                waitpid(mplayer_pid, NULL, 0);
                mplayer_pid = -1;
            }
            
            (void)system("rm -f /tmp/mplayer_fifo");
            usleep(300000);
            
            // 播放新视频
            printf("切换到视频: %s\n", prev_file);
            current_video_index = index;
            if (start_mplayer(prev_file)) {
                is_playing = true;
                is_paused = false;
            } else {
                is_playing = false;
                is_paused = false;
            }
            break;
        }
    } while (index != start_index);
    
    pthread_mutex_unlock(&player_mutex);
}

/**
 * @brief 下一首
 */
void simple_video_next(void) {
    pthread_mutex_lock(&player_mutex);
    
    if (!is_playing) {
        pthread_mutex_unlock(&player_mutex);
        return;
    }
    
    // 查找下一个视频文件
    extern char **video_files;
    extern int video_count;
    
    if (video_files == NULL || video_count == 0) {
        pthread_mutex_unlock(&player_mutex);
        return;
    }
    
    extern int current_video_index;
    int start_index = (current_video_index >= 0) ? current_video_index : 0;
    int index = start_index;
    
    do {
        index++;
        if (index >= video_count) {
            index = 0;
        }
        
        if (index == start_index) {
            // 已经循环一圈，没有找到其他视频
            pthread_mutex_unlock(&player_mutex);
            return;
        }
        
        const char *next_file = video_files[index];
        if (next_file != NULL && is_video_file(next_file)) {
            // 停止当前播放
            if (mplayer_control_pipe != -1) {
                send_mplayer_command("quit");
                close(mplayer_control_pipe);
                mplayer_control_pipe = -1;
            }
            
            if (mplayer_pid != -1) {
                kill(mplayer_pid, SIGTERM);
                waitpid(mplayer_pid, NULL, 0);
                mplayer_pid = -1;
            }
            
            (void)system("rm -f /tmp/mplayer_fifo");
            usleep(300000);
            
            // 播放新视频
            printf("切换到视频: %s\n", next_file);
            current_video_index = index;
            if (start_mplayer(next_file)) {
                is_playing = true;
                is_paused = false;
            } else {
                is_playing = false;
                is_paused = false;
            }
            break;
        }
    } while (index != start_index);
    
    pthread_mutex_unlock(&player_mutex);
}

/**
 * @brief 加速播放
 */
void simple_video_speed_up(void) {
    pthread_mutex_lock(&player_mutex);
    
    if (is_playing && mplayer_control_pipe != -1) {
        playback_speed += 0.1f;
        if (playback_speed > 2.0f) {
            playback_speed = 2.0f;
        }
        
        char cmd[64];
        snprintf(cmd, sizeof(cmd), "speed_set %.1f", playback_speed);
        send_mplayer_command(cmd);
        printf("播放速度: %.1fx\n", playback_speed);
    }
    
    pthread_mutex_unlock(&player_mutex);
}

/**
 * @brief 减速播放
 */
void simple_video_speed_down(void) {
    pthread_mutex_lock(&player_mutex);
    
    if (is_playing && mplayer_control_pipe != -1) {
        playback_speed -= 0.1f;
        if (playback_speed < 0.5f) {
            playback_speed = 0.5f;
        }
        
        char cmd[64];
        snprintf(cmd, sizeof(cmd), "speed_set %.1f", playback_speed);
        send_mplayer_command(cmd);
        printf("播放速度: %.1fx\n", playback_speed);
    }
    
    pthread_mutex_unlock(&player_mutex);
}

/**
 * @brief 增加音量
 */
void simple_video_volume_up(void) {
    pthread_mutex_lock(&player_mutex);
    
    if (is_playing && mplayer_control_pipe != -1) {
        send_mplayer_command("volume +10");
    }
    
    pthread_mutex_unlock(&player_mutex);
}

/**
 * @brief 减少音量
 */
void simple_video_volume_down(void) {
    pthread_mutex_lock(&player_mutex);
    
    if (is_playing && mplayer_control_pipe != -1) {
        send_mplayer_command("volume -10");
    }
    
    pthread_mutex_unlock(&player_mutex);
}

/**
 * @brief 获取播放状态
 * @return 是否正在播放
 */
bool simple_video_is_playing(void) {
    bool result;
    pthread_mutex_lock(&player_mutex);
    result = is_playing;
    pthread_mutex_unlock(&player_mutex);
    return result;
}

/**
 * @brief 清理资源
 */
void simple_video_cleanup(void) {
    simple_video_force_stop();
}
