/**
 * @file media_player.c
 * @brief 媒体播放器模块实现（兼容层，委托给独立的音频和视频播放器）
 * 
 * 重构说明：
 * - 音频播放：委托给 audio_player 模块
 * - 视频播放：委托给 simple_video_player 模块
 * - 两者完全独立，互不干扰
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "media_player.h"
#include "simple_video_player.h"
#include "audio_player.h"

// 状态变量（用于兼容旧接口）
static bool is_playing = false;
static bool is_paused = false;
static bool is_video = false;

// UI更新回调函数指针（由UI模块设置）
static void (*status_update_callback)(const char *status) = NULL;

/**
 * @brief 设置状态更新回调函数
 */
void media_player_set_status_callback(void (*callback)(const char *)) {
    status_update_callback = callback;
    // 同时设置给音频播放器
    audio_player_set_status_callback(callback);
}

void check_file_exists(const char *path) {
    // 委托给音频播放器（如果需要的话，可以保留这个函数用于兼容）
    // 实际检查在 audio_player 和 simple_video_player 中完成
}

void start_mplayer(const char* file, bool video) {
    if (video) {
        // 视频文件：委托给 simple_video_player
        if (simple_video_play(file)) {
            is_playing = true;
            is_paused = false;
            is_video = true;
            if (status_update_callback) {
                status_update_callback("状态: 播放视频");
            }
        } else {
            printf("错误: 无法启动视频播放\n");
            if (status_update_callback) {
                status_update_callback("错误: 无法启动视频播放");
            }
        }
    } else {
        // 音频文件：委托给 audio_player
        if (audio_player_play(file)) {
        is_playing = true;
        is_paused = false;
            is_video = false;
            if (status_update_callback) {
                status_update_callback("状态: 播放音频");
            }
        } else {
            printf("错误: 无法启动音频播放\n");
        if (status_update_callback) {
                status_update_callback("错误: 无法启动音频播放");
            }
        }
    }
}

void send_command(const char* cmd) {
    if (cmd == NULL) {
        return;
    }
    
    // 根据当前播放类型，发送命令到对应的播放器
    if (is_video && is_playing) {
        // 视频播放器的命令发送在 simple_video_player 中处理
        // 视频播放器使用FIFO，命令发送在 simple_video_player 中
    } else if (!is_video && is_playing) {
        // 音频播放器：使用统一的命令发送接口
        audio_player_send_command(cmd);
    }
}

void stop_mplayer() {
    if (is_video && is_playing) {
        // 视频：委托给 simple_video_player
        simple_video_stop();
        is_playing = false;
        is_paused = false;
        is_video = false;
        if (status_update_callback) {
            status_update_callback("状态: 已停止");
        }
    } else if (!is_video && is_playing) {
        // 音频：委托给 audio_player
        audio_player_stop();
    is_playing = false;
    is_paused = false;
    is_video = false;
    if (status_update_callback) {
        status_update_callback("状态: 已停止");
        }
    }
}

bool get_is_playing(void) {
    // 从对应的播放器获取状态
    if (is_video) {
        return simple_video_is_playing();
    } else {
        return audio_player_is_playing();
    }
}

bool get_is_paused(void) {
    // 从对应的播放器获取状态
    if (is_video) {
        // 视频播放器可能没有暂停状态，返回 false
        return false;
    } else {
        return audio_player_is_paused();
    }
}

bool get_is_video(void) {
    return is_video;
}

void set_is_playing(bool playing) {
    is_playing = playing;
    // 同步到对应的播放器（如果需要）
}

void set_is_paused(bool paused) {
    is_paused = paused;
    // 同步到对应的播放器（如果需要）
}

void set_is_video(bool video) {
    is_video = video;
}

float get_current_speed(void) {
    // 从对应的播放器获取速度
    if (is_video) {
        // 视频播放器的速度管理在 simple_video_player 中
        return 1.0f;  // 默认值
    } else {
        return audio_player_get_speed();
    }
}

void set_current_speed(float speed) {
    // 设置到对应的播放器
    if (is_video) {
        // 视频播放器的速度设置在 simple_video_player 中
    } else {
        audio_player_set_speed(speed);
    }
}

int get_mplayer_pid(void) {
    // 返回对应播放器的进程ID
    if (is_video) {
        // 视频播放器的PID在 simple_video_player 中管理
        return -1;
    } else {
        return audio_player_get_pid();
    }
}

bool is_mplayer_running(void) {
    // 检查对应播放器是否在运行
    if (is_video) {
        return simple_video_is_playing();
    } else {
        return audio_player_is_running();
    }
}

