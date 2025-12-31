/**
 * @file audio_player.c
 * @brief 独立的音频播放器模块实现（仅处理音频文件）
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <signal.h>
#include <errno.h>
#include <string.h>
#include <stdbool.h>
#include "audio_player.h"

// 音频播放器独立的状态变量
static pid_t audio_mplayer_pid = -1;
static int audio_mplayer_pipe = -1;
static float audio_current_speed = 1.0;
static bool audio_is_playing = false;
static bool audio_is_paused = false;

// UI更新回调函数指针
static void (*audio_status_update_callback)(const char *status) = NULL;

/**
 * @brief 初始化音频播放器
 */
void audio_player_init(void) {
    // 初始化状态
    audio_mplayer_pid = -1;
    audio_mplayer_pipe = -1;
    audio_current_speed = 1.0;
    audio_is_playing = false;
    audio_is_paused = false;
}

/**
 * @brief 设置状态更新回调函数
 */
void audio_player_set_status_callback(void (*callback)(const char *)) {
    audio_status_update_callback = callback;
}

/**
 * @brief 检查文件是否存在
 */
static void check_audio_file_exists(const char *path) {
    struct stat st;
    if (stat(path, &st) == -1) {
        printf("[音频播放器] 错误: 文件不存在 [%s] - %s\n", path, strerror(errno));
        if (audio_status_update_callback) {
            char err_msg[100];
            snprintf(err_msg, sizeof(err_msg), "错误: %s", strerror(errno));
            audio_status_update_callback(err_msg);
        }
        return;
    }
    
    printf("[音频播放器] 文件信息: %s\n", path);
    printf("[音频播放器] 大小: %ld 字节\n", st.st_size);
}

/**
 * @brief 播放音频文件
 */
bool audio_player_play(const char *file_path) {
    if (file_path == NULL) {
        return false;
    }
    
    // 检查文件是否存在
    check_audio_file_exists(file_path);
    
    // 如果已有播放器在运行，先停止并等待资源释放
    if (audio_is_playing || audio_player_is_running()) {
        audio_player_stop();
        // 等待进程完全退出
        usleep(200000);  // 200ms
    }
    
    int pipefd[2];
    
    if (pipe(pipefd) == -1) {
        perror("[音频播放器] pipe");
        return false;
    }

    audio_mplayer_pid = fork();
    if (audio_mplayer_pid < 0) {
        perror("[音频播放器] fork");
        close(pipefd[0]);
        close(pipefd[1]);
        return false;
    }

    if (audio_mplayer_pid == 0) { // 子进程
        close(pipefd[1]);
        // 使用管道文件描述符，mplayer会从标准输入读取命令
        dup2(pipefd[0], STDIN_FILENO);
        close(pipefd[0]);
        
        // 准备参数数组
        char *args[30];
        int arg_count = 0;
        
        // 第一个参数是程序路径，稍后设置
        args[arg_count++] = NULL;  // 占位符
        args[arg_count++] = "-slave";
        args[arg_count++] = "-quiet";
        // 音频播放不需要视频输出参数
        
        // 添加文件路径
        args[arg_count++] = (char *)file_path;
        args[arg_count] = NULL; // 结束标记
        
        // 尝试多个可能的mplayer路径
        const char *mplayer_paths[] = {
            "/bin/mplayer",
            "./mplayer",
            "/usr/bin/mplayer",
            "/system/bin/mplayer",
            "mplayer",
            NULL
        };
        
        // 打印执行命令
        printf("[音频播放器] 执行命令: ");
        for (int i = 1; args[i] != NULL; i++) {
            printf("%s ", args[i]);
        }
        printf("\n");
        
        // 尝试每个路径
        for (int i = 0; mplayer_paths[i] != NULL; i++) {
            args[0] = (char *)mplayer_paths[i];
            execvp(args[0], args);
            // 如果execvp成功，不会返回
        }
        
        // 所有路径都失败了
        printf("[音频播放器] 错误: 无法找到mplayer，尝试过的路径:\n");
        for (int i = 0; mplayer_paths[i] != NULL; i++) {
            printf("  - %s\n", mplayer_paths[i]);
        }
        perror("[音频播放器] execvp");
        exit(1);
    } else { // 父进程
        close(pipefd[0]);
        audio_mplayer_pipe = pipefd[1];
        fcntl(audio_mplayer_pipe, F_SETFL, O_NONBLOCK);
        audio_is_playing = true;
        audio_is_paused = false;
        
        // 更新状态显示
        if (audio_status_update_callback) {
            audio_status_update_callback("状态: 播放音频");
        }
        
        printf("[音频播放器] 正在播放: %s (PID: %d)\n", file_path, audio_mplayer_pid);
        return true;
    }
}

/**
 * @brief 发送控制命令
 */
static void send_audio_command(const char* cmd) {
    if (audio_mplayer_pipe != -1) {
        dprintf(audio_mplayer_pipe, "%s\n", cmd);
    }
}

/**
 * @brief 停止播放
 */
void audio_player_stop(void) {
    // 先发送退出命令
    if (audio_mplayer_pipe != -1) {
        send_audio_command("quit");
        // 等待一小段时间让mplayer处理命令
        usleep(100000);  // 100ms
        close(audio_mplayer_pipe);
        audio_mplayer_pipe = -1;
    }
    
    // 等待进程退出，使用超时机制避免无限等待
    if (audio_mplayer_pid != -1) {
        int status;
        pid_t result;
        int wait_count = 0;
        const int max_wait = 10;  // 最多等待1秒（10 * 100ms）
        
        while (wait_count < max_wait) {
            result = waitpid(audio_mplayer_pid, &status, WNOHANG);
            if (result > 0) {
                // 进程已退出
                break;
            } else if (result == 0) {
                // 进程还在运行，继续等待
                usleep(100000);  // 100ms
                wait_count++;
            } else {
                // 错误
                perror("[音频播放器] waitpid");
                break;
            }
        }
        
        // 如果进程还在运行，强制终止
        if (wait_count >= max_wait) {
            printf("[音频播放器] 警告: mplayer进程未正常退出，强制终止\n");
            kill(audio_mplayer_pid, SIGTERM);
            usleep(100000);
            waitpid(audio_mplayer_pid, NULL, 0);
        }
        
        audio_mplayer_pid = -1;
    }
    
    // 确保所有状态都已清理
    audio_is_playing = false;
    audio_is_paused = false;
    
    // 更新状态显示
    if (audio_status_update_callback) {
        audio_status_update_callback("状态: 已停止");
    }
}

/**
 * @brief 暂停/恢复播放
 */
void audio_player_toggle_pause(void) {
    if (audio_is_playing && audio_mplayer_pipe != -1) {
        send_audio_command("pause");
        audio_is_paused = !audio_is_paused;
        
        if (audio_status_update_callback) {
            audio_status_update_callback(audio_is_paused ? "状态: 已暂停" : "状态: 播放音频");
        }
    }
}

/**
 * @brief 获取播放状态
 */
bool audio_player_is_playing(void) {
    return audio_is_playing;
}

/**
 * @brief 获取暂停状态
 */
bool audio_player_is_paused(void) {
    return audio_is_paused;
}

/**
 * @brief 获取当前播放速度
 */
float audio_player_get_speed(void) {
    return audio_current_speed;
}

/**
 * @brief 设置当前播放速度
 */
void audio_player_set_speed(float speed) {
    audio_current_speed = speed;
    
    if (audio_is_playing && audio_mplayer_pipe != -1) {
        char cmd[32];
        snprintf(cmd, sizeof(cmd), "speed_set %.2f", (double)speed);
        send_audio_command(cmd);
    }
}

/**
 * @brief 增加音量
 */
void audio_player_volume_up(void) {
    if (audio_is_playing && audio_mplayer_pipe != -1) {
        send_audio_command("volume +10");
    }
}

/**
 * @brief 减少音量
 */
void audio_player_volume_down(void) {
    if (audio_is_playing && audio_mplayer_pipe != -1) {
        send_audio_command("volume -10");
    }
}

/**
 * @brief 获取MPlayer进程ID
 */
int audio_player_get_pid(void) {
    return audio_mplayer_pid;
}

/**
 * @brief 检查MPlayer是否仍在运行
 */
bool audio_player_is_running(void) {
    if (audio_mplayer_pid == -1) {
        return false;
    }
    // 使用WNOHANG非阻塞检查
    int status;
    pid_t result = waitpid(audio_mplayer_pid, &status, WNOHANG);
    if (result > 0) {
        // 进程已结束
        audio_mplayer_pid = -1;
        audio_mplayer_pipe = -1;
        audio_is_playing = false;
        audio_is_paused = false;
        return false;
    }
    return true;
}

/**
 * @brief 发送控制命令到音频播放器
 */
void audio_player_send_command(const char *cmd) {
    if (cmd == NULL || !audio_is_playing || audio_mplayer_pipe == -1) {
        return;
    }
    send_audio_command(cmd);
}

/**
 * @brief 清理资源
 */
void audio_player_cleanup(void) {
    if (audio_is_playing) {
        audio_player_stop();
    }
}

