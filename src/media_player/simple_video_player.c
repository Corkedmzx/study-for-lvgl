/**
 * @file simple_video_player.c
 * @brief 简单的MPlayer全屏视频播放器实现
 * 
 * 实现方案：
 * 1. 使用MPlayer全屏播放视频（-vo fbdev）
 * 2. 通过管道发送控制命令
 * 3. 触屏手势控制（在独立线程中处理）
 * 4. 播放线程和控制线程分离
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
#include <sys/mman.h>  // For mmap
#include <linux/fb.h>  // For framebuffer
#include <signal.h>
#include <pthread.h>
#include <errno.h>
#include <unistd.h>  // for access

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
    if (cmd != NULL) {
        // 如果FIFO还没打开，尝试打开
        if (mplayer_control_pipe == -1) {
            mplayer_control_pipe = open("/tmp/mplayer_fifo", O_WRONLY | O_NONBLOCK);
            if (mplayer_control_pipe == -1) {
                return;  // 无法打开FIFO
            }
        }
        
        if (mplayer_control_pipe != -1) {
            (void)write(mplayer_control_pipe, cmd, strlen(cmd));
            (void)write(mplayer_control_pipe, "\n", 1);
            fsync(mplayer_control_pipe);
        }
    }
}

/**
 * @brief 启动MPlayer进程（全屏播放）
 */
static bool start_mplayer(const char *file_path) {
    // 创建FIFO管道（参考MPlayer_text.cpp）
    (void)system("rm -f /tmp/mplayer_fifo");
    if (mkfifo("/tmp/mplayer_fifo", 0666) == -1 && errno != EEXIST) {
        perror("mkfifo");
        return false;
    }
    
    mplayer_pid = fork();
    if (mplayer_pid < 0) {
        perror("fork");
        return false;
    }
    
    if (mplayer_pid == 0) {  // 子进程
        // 不重定向stderr，保留错误信息以便调试
        // 如果需要静默模式，可以取消下面的注释
        // int null_fd = open("/dev/null", O_WRONLY);
        // if (null_fd != -1) {
        //     dup2(null_fd, STDERR_FILENO);
        //     close(null_fd);
        // }
        
        // MPlayer参数（参考MPlayer_text.cpp）
        const char *mplayer_paths[] = {
            "/bin/mplayer",
            "./mplayer",
            "/usr/bin/mplayer",
            "/system/bin/mplayer",
            "mplayer",
            NULL
        };
        
        char *args[35];  // 增加参数数组大小以容纳更多优化参数
        int arg_count = 0;
        args[arg_count++] = NULL;  // 占位符
        args[arg_count++] = "-slave";
        args[arg_count++] = "-quiet";
        args[arg_count++] = "-input";
        args[arg_count++] = "file=/tmp/mplayer_fifo";
        args[arg_count++] = "-vo";
        args[arg_count++] = "fbdev";  // 全屏输出到framebuffer
        args[arg_count++] = "-vf";
        args[arg_count++] = "scale=800:480";  // 缩放视频到屏幕尺寸
        args[arg_count++] = "-zoom";  // 缩放以适应屏幕
        args[arg_count++] = "-fs";  // 全屏模式
        // 性能优化参数（根据MPlayer错误提示添加）
        // 注意：移除 -framedrop 可以减少丢帧，但可能导致播放卡顿
        // 如果系统性能足够，可以注释掉下面这行来减少丢帧
        args[arg_count++] = "-framedrop";  // 丢帧以保持同步，解决视频输出慢的问题
        args[arg_count++] = "-autosync";
        args[arg_count++] = "30";  // 自动同步，30是推荐值，解决音频驱动问题
        args[arg_count++] = "-cache";
        args[arg_count++] = "32768";  // 增加缓存到32MB（从16MB增加），减少因I/O导致的丢帧，提高流畅度
        args[arg_count++] = "-cache-min";  // 最小缓存百分比
        args[arg_count++] = "50";  // 缓存到50%才开始播放，减少卡顿
        // 注意：MPlayer内部已经使用了高效的I/O机制（包括预读和缓存）
        // 对于本地文件，MPlayer会自动优化I/O，mmap的效果有限
        args[arg_count++] = "-lavdopts";
        // 解码优化参数：
        // fast: 快速解码模式（当前使用）
        // threads=2: 使用2个线程进行解码（如果MPlayer版本不支持，可以移除threads参数）
        // skiploopfilter=all: 跳过循环滤波（可提高性能，但可能降低画质，作为注释保留）
        // 注意：如果遇到"threads option must be an integer"错误，请移除threads参数，只使用"fast"
        args[arg_count++] = "fast";  // 只使用快速解码模式（如果threads不支持，使用这个）
        // args[arg_count++] = "fast:threads=2";  // 如果MPlayer支持threads参数，可以使用这行替代上面一行
        // 音频输出优化：优先尝试OSS（通常比ALSA更快），如果失败会自动回退
        args[arg_count++] = "-ao";
        args[arg_count++] = "oss";  // 使用OSS音频输出（如果系统支持）
        args[arg_count++] = (char *)file_path;
        args[arg_count] = NULL;
        
        // 检查视频文件是否存在
        struct stat st;
        if (stat(file_path, &st) != 0) {
            exit(1);
        }
        
        // 尝试执行MPlayer
        for (int i = 0; mplayer_paths[i] != NULL; i++) {
            args[0] = (char *)mplayer_paths[i];
            execvp(args[0], args);
        }
        
        exit(1);
    } else {  // 父进程
        // 等待MPlayer启动
        usleep(1000000);  // 1秒，等待FIFO创建和MPlayer启动
        
        // 打开FIFO用于发送控制命令
        mplayer_control_pipe = open("/tmp/mplayer_fifo", O_WRONLY | O_NONBLOCK);
        if (mplayer_control_pipe == -1) {
            printf("警告: 无法打开MPlayer控制FIFO\n");
            // 继续执行，可能MPlayer还在启动中
        }
        
        // 检查进程是否还在运行（多次检查，确保MPlayer真正启动）
        int status;
        pid_t result;
        int check_count = 0;
        const int max_checks = 5;  // 最多检查5次
        
        while (check_count < max_checks) {
            result = waitpid(mplayer_pid, &status, WNOHANG);
        if (result == mplayer_pid) {
                // MPlayer退出，启动失败
            printf("错误: MPlayer启动失败，退出状态: %d\n", WEXITSTATUS(status));
                if (mplayer_control_pipe != -1) {
                    close(mplayer_control_pipe);
                }
                mplayer_control_pipe = -1;
                mplayer_pid = -1;
                return false;
            } else if (result == 0) {
                // 进程还在运行，检查是否能够写入FIFO
                if (mplayer_control_pipe != -1) {
                    // 可以写入FIFO，再等待一下确保MPlayer真正启动（不是立即退出）
                    usleep(500000);  // 500ms，确保MPlayer真正开始播放
                    // 再次检查进程是否还在运行
                    result = waitpid(mplayer_pid, &status, WNOHANG);
                    if (result == 0) {
                        // 进程还在运行，说明启动成功
                        printf("MPlayer已启动，PID: %d\n", mplayer_pid);
                        return true;
                    } else if (result == mplayer_pid) {
                        // MPlayer在启动后退出
                        printf("错误: MPlayer启动后立即退出，退出状态: %d\n", WEXITSTATUS(status));
                        if (mplayer_control_pipe != -1) {
                            close(mplayer_control_pipe);
                        }
                        mplayer_control_pipe = -1;
                        mplayer_pid = -1;
                        return false;
                    }
                } else {
                    // FIFO还未准备好，再等待一下
                    usleep(200000);  // 200ms
                    check_count++;
                    // 重新尝试打开FIFO
                    mplayer_control_pipe = open("/tmp/mplayer_fifo", O_WRONLY | O_NONBLOCK);
                }
            } else {
                // waitpid错误
                printf("警告: waitpid错误: %s\n", strerror(errno));
                break;
            }
        }
        
        // 如果检查了max_checks次后进程还在运行，再等待并检查一次
        usleep(500000);  // 500ms
        result = waitpid(mplayer_pid, &status, WNOHANG);
        if (result == 0) {
            // 进程还在运行，认为启动成功
            printf("MPlayer已启动，PID: %d\n", mplayer_pid);
            return true;
        } else if (result == mplayer_pid) {
            // MPlayer退出
            printf("错误: MPlayer启动后退出，退出状态: %d\n", WEXITSTATUS(status));
            if (mplayer_control_pipe != -1) {
                close(mplayer_control_pipe);
            }
            mplayer_control_pipe = -1;
            mplayer_pid = -1;
            return false;
        } else {
            printf("错误: MPlayer启动失败\n");
            if (mplayer_control_pipe != -1) {
                close(mplayer_control_pipe);
            }
            mplayer_control_pipe = -1;
            mplayer_pid = -1;
            return false;
        }
    }
}

/**
 * @brief 强制停止MPlayer进程（立即使用SIGKILL）
 */
static void force_stop_mplayer(void) {
    if (mplayer_pid != -1) {
        // 立即强制终止，不等待
        kill(mplayer_pid, SIGKILL);
        usleep(50000);  // 等待50ms确保进程终止
        waitpid(mplayer_pid, NULL, WNOHANG);  // 清理僵尸进程
        
        mplayer_pid = -1;
    }
    
    if (mplayer_control_pipe != -1) {
        close(mplayer_control_pipe);
        mplayer_control_pipe = -1;
    }
    
    // 清理FIFO
    (void)system("rm -f /tmp/mplayer_fifo");
    
    // 清理framebuffer，填充白色（确保framebuffer完全恢复）
    int fb_fd = open("/dev/fb0", O_RDWR);
    if (fb_fd != -1) {
        struct fb_var_screeninfo vinfo;
        struct fb_fix_screeninfo finfo;
        if (ioctl(fb_fd, FBIOGET_VSCREENINFO, &vinfo) == 0 &&
            ioctl(fb_fd, FBIOGET_FSCREENINFO, &finfo) == 0) {
            long screensize = finfo.smem_len;
            char *fbp = (char *)mmap(0, screensize, PROT_READ | PROT_WRITE, MAP_SHARED, fb_fd, 0);
            if ((intptr_t)fbp != -1) {
                // 填充白色（0xFFFFFFFF），这样LVGL可以正确显示
                memset(fbp, 0xFF, screensize);
                // 确保写入完成
                msync(fbp, screensize, MS_SYNC);
                munmap(fbp, screensize);
            }
        }
        close(fb_fd);
    }
}

/**
 * @brief 停止MPlayer进程
 */
static void stop_mplayer(void) {
    if (mplayer_control_pipe != -1) {
        send_mplayer_command("quit");
        close(mplayer_control_pipe);
        mplayer_control_pipe = -1;
    }
    
    if (mplayer_pid != -1) {
        int status;
        pid_t result;
        int wait_count = 0;
        const int max_wait = 20;  // 最多等待2秒
        
        while (wait_count < max_wait) {
            result = waitpid(mplayer_pid, &status, WNOHANG);
            if (result > 0) {
                break;
            } else if (result == 0) {
                usleep(100000);  // 100ms
                wait_count++;
            } else {
                break;
            }
        }
        
        // 如果进程还在运行，强制终止
        if (wait_count >= max_wait) {
            if (kill(mplayer_pid, SIGTERM) == 0) {
                usleep(200000);
                result = waitpid(mplayer_pid, &status, WNOHANG);
                if (result == 0) {
                    kill(mplayer_pid, SIGKILL);
                    usleep(100000);
                    waitpid(mplayer_pid, NULL, 0);
                }
            }
        }
        
        mplayer_pid = -1;
    }
    
    // 清理FIFO
    (void)system("rm -f /tmp/mplayer_fifo");
    
    // 清理framebuffer，填充白色（而不是黑色，以便LVGL可以正确显示）
    int fb_fd = open("/dev/fb0", O_RDWR);
    if (fb_fd != -1) {
        struct fb_var_screeninfo vinfo;
        struct fb_fix_screeninfo finfo;
        if (ioctl(fb_fd, FBIOGET_VSCREENINFO, &vinfo) == 0 &&
            ioctl(fb_fd, FBIOGET_FSCREENINFO, &finfo) == 0) {
            long screensize = finfo.smem_len;
            char *fbp = (char *)mmap(0, screensize, PROT_READ | PROT_WRITE, MAP_SHARED, fb_fd, 0);
            if ((intptr_t)fbp != -1) {
                // 填充白色（0xFFFFFFFF），这样LVGL可以正确显示
                memset(fbp, 0xFF, screensize);
                // 确保写入完成
                msync(fbp, screensize, MS_SYNC);
                munmap(fbp, screensize);
            }
        }
        close(fb_fd);
    }
}

/**
 * @brief 初始化视频播放器
 */
void simple_video_init(void) {
    // 初始化互斥锁（已在静态初始化中完成）
}

/**
 * @brief 播放视频文件
 */
bool simple_video_play(const char *file_path) {
    if (file_path == NULL) {
        return false;
    }
    
    pthread_mutex_lock(&player_mutex);
    
    // 如果正在播放，先停止并等待完全退出
    if (is_playing) {
        stop_mplayer();
        is_playing = false;
        // 等待MPlayer完全退出和framebuffer恢复
        usleep(500000);  // 500ms，确保MPlayer完全退出和framebuffer恢复
    }
    
    // 启动MPlayer
    if (!start_mplayer(file_path)) {
        is_playing = false;  // 确保状态正确
        is_paused = false;
        pthread_mutex_unlock(&player_mutex);
        return false;
    }
    
    is_playing = true;
    is_paused = false;  // 初始状态为播放中
    playback_speed = 1.0f;
    
    pthread_mutex_unlock(&player_mutex);
    return true;
}

/**
 * @brief 停止播放
 */
void simple_video_stop(void) {
    pthread_mutex_lock(&player_mutex);
    
    if (is_playing) {
        stop_mplayer();
        is_playing = false;
        is_paused = false;
    }
    
    pthread_mutex_unlock(&player_mutex);
}

/**
 * @brief 强制停止播放（立即使用SIGKILL终止mplayer进程）
 */
void simple_video_force_stop(void) {
    pthread_mutex_lock(&player_mutex);
    
    if (is_playing) {
        force_stop_mplayer();
        is_playing = false;
        is_paused = false;
    }
    
    pthread_mutex_unlock(&player_mutex);
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
            break;  // 没有找到其他视频文件
        }
        
        if (video_files[index] != NULL) {
            const char *next_file = video_files[index];
            // 先停止当前播放并重置状态
            // 使用force_stop确保framebuffer完全清理
            force_stop_mplayer();
            is_playing = false;
            is_paused = false;
            // 等待MPlayer完全退出和framebuffer恢复
            usleep(500000);  // 500ms，确保MPlayer完全退出和framebuffer恢复
            
            // 释放锁，让simple_video_play重新获取锁
            pthread_mutex_unlock(&player_mutex);
            
            // 播放新视频
            printf("切换到视频: %s\n", next_file);
            if (simple_video_play(next_file)) {
                current_video_index = index;
                printf("视频切换成功，新索引: %d\n", current_video_index);
            } else {
                printf("错误: 无法播放视频: %s\n", next_file);
            }
            return;  // 已经释放锁，直接返回
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
            break;  // 没有找到其他视频文件
        }
        
        if (video_files[index] != NULL) {
            const char *next_file = video_files[index];
            // 先停止当前播放并重置状态
            // 使用force_stop确保framebuffer完全清理
            force_stop_mplayer();
            is_playing = false;
            is_paused = false;
            // 等待MPlayer完全退出和framebuffer恢复
            usleep(500000);  // 500ms，确保MPlayer完全退出和framebuffer恢复
            
            // 释放锁，让simple_video_play重新获取锁
            pthread_mutex_unlock(&player_mutex);
            
            // 播放新视频
            printf("切换到视频: %s\n", next_file);
            if (simple_video_play(next_file)) {
                current_video_index = index;
                printf("视频切换成功，新索引: %d\n", current_video_index);
            } else {
                printf("错误: 无法播放视频: %s\n", next_file);
            }
            return;  // 已经释放锁，直接返回
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
        
        char cmd[32];
        snprintf(cmd, sizeof(cmd), "speed_set %.1f", (double)playback_speed);
        send_mplayer_command(cmd);
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
        
        char cmd[32];
        snprintf(cmd, sizeof(cmd), "speed_set %.1f", (double)playback_speed);
        send_mplayer_command(cmd);
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
    simple_video_stop();
}

