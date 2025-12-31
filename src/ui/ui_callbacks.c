/**
 * @file ui_callbacks.c
 * @brief UI回调函数实现
 */

#include "ui_callbacks.h"
#include "ui_screens.h"
#include "../common/common.h"
#include "../media_player/audio_player.h"
#include "../media_player/simple_video_player.h"
#include "../file_scanner/file_scanner.h"
#include "../ui/video_win.h"
#include "../ui/video_touch_control.h"
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <pthread.h>
#include "lvgl/lvgl.h"

// 从common.h中获取全局变量
extern lv_obj_t *status_label;
extern lv_obj_t *speed_label;
extern lv_obj_t *video_container;
extern bool should_exit;
extern int current_audio_index;
extern int current_video_index;

// 从file_scanner.h中获取函数
extern char **audio_files;
extern int audio_count;
extern char **video_files;
extern int video_count;

// 按钮操作互斥锁，防止并发冲突
static pthread_mutex_t button_mutex = PTHREAD_MUTEX_INITIALIZER;

/**
 * @brief 播放音频回调（只处理音频文件）
 * 确保可以多次响应，不会因为状态问题导致无响应
 */
void play_audio_cb(lv_event_t * e) {
    (void)e;
    
    // 检查事件类型，确保是点击事件
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    
    // 加锁保护，防止并发操作
    if (pthread_mutex_lock(&button_mutex) != 0) {
        return;
    }
    
    // 使用扫描到的音频文件
    if (audio_count == 0 || audio_files == NULL) {
        if (status_label) {
            lv_label_set_text(status_label, "状态: 未找到音频文件");
        }
        pthread_mutex_unlock(&button_mutex);
        return;
    }
    
    // 检查当前索引是否有效
    if (current_audio_index < 0 || current_audio_index >= audio_count) {
        current_audio_index = 0;
    }
    
    const char *file_path = audio_files[current_audio_index];
    if (file_path == NULL) {
        if (status_label) {
            lv_label_set_text(status_label, "状态: 文件路径无效");
        }
        pthread_mutex_unlock(&button_mutex);
        return;
    }
    
    // 音频文件使用独立的音频播放器
    // 检查音频播放器是否还在运行
    if (audio_player_is_running() && audio_player_is_playing()) {
            // 如果正在播放，切换暂停/播放
        audio_player_toggle_pause();
            
            if (status_label) {
            lv_label_set_text(status_label, audio_player_is_paused() ? "状态: 音频暂停" : "状态: 播放音频");
            }
        } else {
            // 如果未播放或进程已退出，重新启动播放
        if (audio_player_is_playing()) {
            audio_player_stop();  // 清理旧状态
            }
        audio_player_play(file_path);
        }
    
    pthread_mutex_unlock(&button_mutex);
}

/**
 * @brief 播放视频回调（只处理视频文件）
 */
void play_video_cb(lv_event_t * e) {
    (void)e;
    
    // 检查事件类型，确保是点击事件
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    
    // 加锁保护，防止并发操作
    if (pthread_mutex_lock(&button_mutex) != 0) {
        return;
    }
    
    // 使用扫描到的视频文件
    if (video_count == 0 || video_files == NULL) {
        if (status_label) {
            lv_label_set_text(status_label, "状态: 未找到视频文件");
        }
        pthread_mutex_unlock(&button_mutex);
        return;
    }
    
    // 检查当前索引是否有效
    if (current_video_index < 0 || current_video_index >= video_count) {
        current_video_index = 0;
    }
    
    const char *file_path = video_files[current_video_index];
    if (file_path == NULL) {
        if (status_label) {
            lv_label_set_text(status_label, "状态: 文件路径无效");
        }
        pthread_mutex_unlock(&button_mutex);
        return;
    }
    
    // 视频文件使用视频播放器
    if (simple_video_is_playing()) {
        simple_video_stop();
    }
    // 直接显示视频窗口（全屏播放）
    extern void video_win_show_with_file(const char *);
    video_win_show_with_file(file_path);
    
    pthread_mutex_unlock(&button_mutex);
}

/**
 * @brief 停止回调
 * 确保可以多次调用，稳定停止播放
 */
void stop_cb(lv_event_t * e) {
    (void)e;
    
    // 检查事件类型
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    
    // 加锁保护
    if (pthread_mutex_lock(&button_mutex) != 0) {
        return;
    }
    
    // 停止视频播放器
    if (simple_video_is_playing()) {
        simple_video_stop();
    }
    
    // 停止音频播放器
    if (audio_player_is_playing()) {
        audio_player_stop();
    }
    
    if (status_label) {
        lv_label_set_text(status_label, "状态: 已停止");
    }
    
    pthread_mutex_unlock(&button_mutex);
}

/**
 * @brief 减速回调
 * 确保可以多次响应
 */
void slower_cb(lv_event_t * e) {
    (void)e;
    
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    
    // 加锁保护
    if (pthread_mutex_lock(&button_mutex) != 0) {
        return;
    }
    
    // 检查是否正在播放且未暂停（视频或音频）
    bool playing = simple_video_is_playing() || audio_player_is_playing();
    bool paused = audio_player_is_paused();  // 视频播放器没有暂停状态
    
    if (playing && !paused) {
        float speed;
        if (simple_video_is_playing()) {
            // 视频播放器的速度控制
            simple_video_speed_down();
            speed = 1.0f;  // 视频播放器内部管理速度
        } else if (audio_player_is_playing()) {
            // 音频播放器的速度控制
            speed = audio_player_get_speed();
        if (speed > 0.25f) {
            speed -= 0.25f;
                audio_player_set_speed(speed);
            }
        }
            
            // 更新速度显示
        if (speed_label && audio_player_is_playing()) {
                char speed_str[32];
                snprintf(speed_str, sizeof(speed_str), "速度: %.2fx", (double)speed);
                lv_label_set_text(speed_label, speed_str);
        }
    }
    
    pthread_mutex_unlock(&button_mutex);
}

/**
 * @brief 加速回调
 * 确保可以多次响应
 */
void faster_cb(lv_event_t * e) {
    (void)e;
    
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    
    // 加锁保护
    if (pthread_mutex_lock(&button_mutex) != 0) {
        return;
    }
    
    // 检查是否正在播放且未暂停（视频或音频）
    bool playing = simple_video_is_playing() || audio_player_is_playing();
    bool paused = audio_player_is_paused();  // 视频播放器没有暂停状态
    
    if (playing && !paused) {
        float speed;
        if (simple_video_is_playing()) {
            // 视频播放器的速度控制
            simple_video_speed_up();
            speed = 1.0f;  // 视频播放器内部管理速度
        } else if (audio_player_is_playing()) {
            // 音频播放器的速度控制
            speed = audio_player_get_speed();
        if (speed < 5.0f) {
            speed += 0.25f;
                audio_player_set_speed(speed);
            }
        }
            
            // 更新速度显示
        if (speed_label && audio_player_is_playing()) {
                char speed_str[32];
                snprintf(speed_str, sizeof(speed_str), "速度: %.2fx", (double)speed);
                lv_label_set_text(speed_label, speed_str);
        }
    }
    
    pthread_mutex_unlock(&button_mutex);
}

/**
 * @brief 重置速度回调
 */
void reset_speed_cb(lv_event_t * e) {
    (void)e;
    
    // 加锁保护
    if (pthread_mutex_lock(&button_mutex) != 0) {
        return;
    }
    
    if (audio_player_is_playing() && !audio_player_is_paused()) {
        audio_player_set_speed(1.0);
        
        // 更新速度显示
        if (speed_label) {
            lv_label_set_text(speed_label, "速度: 1.00x");
        }
    }
    
    pthread_mutex_unlock(&button_mutex);
}

/**
 * @brief 音量减回调
 * 确保可以多次响应
 */
void volume_down_cb(lv_event_t * e) {
    (void)e;
    
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    
    // 加锁保护
    if (pthread_mutex_lock(&button_mutex) != 0) {
        return;
    }
    
    // 检查是否正在播放（视频或音频）
    if (simple_video_is_playing()) {
        simple_video_volume_down();
    } else if (audio_player_is_playing()) {
        audio_player_volume_down();
    }
    
    pthread_mutex_unlock(&button_mutex);
}

/**
 * @brief 音量加回调
 * 确保可以多次响应
 */
void volume_up_cb(lv_event_t * e) {
    (void)e;
    
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    
    // 加锁保护
    if (pthread_mutex_lock(&button_mutex) != 0) {
        return;
    }
    
    // 检查是否正在播放（视频或音频）
    if (simple_video_is_playing()) {
        simple_video_volume_up();
    } else if (audio_player_is_playing()) {
        audio_player_volume_up();
    }
    
    pthread_mutex_unlock(&button_mutex);
}

/**
 * @brief 上一首媒体回调（只处理音频）
 * 确保可以多次响应，稳定切换
 */
void prev_media_cb(lv_event_t * e) {
    (void)e;
    
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    
    // 加锁保护
    if (pthread_mutex_lock(&button_mutex) != 0) {
        return;
    }
    
    // 只处理音频文件
    if (audio_count == 0 || audio_files == NULL) {
        pthread_mutex_unlock(&button_mutex);
        return;
    }
    
    // 停止当前音频播放
    if (audio_player_is_playing()) {
        audio_player_stop();
    }
    
    // 等待一小段时间，确保资源已释放
    usleep(100000);  // 100ms
    
    current_audio_index--;
    if (current_audio_index < 0) {
        current_audio_index = audio_count - 1;  // 循环到最后一首
    }
    
    printf("切换到上一首音频，索引: %d\n", current_audio_index);
    
    // 自动播放新的音频文件
    if (audio_files[current_audio_index] != NULL) {
        const char *file_path = audio_files[current_audio_index];
        audio_player_play(file_path);
    }
    
    pthread_mutex_unlock(&button_mutex);
}

/**
 * @brief 下一首媒体回调（只处理音频）
 * 确保可以多次响应，稳定切换
 */
void next_media_cb(lv_event_t * e) {
    (void)e;
    
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    
    // 加锁保护
    if (pthread_mutex_lock(&button_mutex) != 0) {
        return;
    }
    
    // 只处理音频文件
    if (audio_count == 0 || audio_files == NULL) {
        pthread_mutex_unlock(&button_mutex);
        return;
    }
    
    // 停止当前音频播放
    if (audio_player_is_playing()) {
        audio_player_stop();
    }
    
    // 等待一小段时间，确保资源已释放
    usleep(100000);  // 100ms
    
    current_audio_index++;
    if (current_audio_index >= audio_count) {
        current_audio_index = 0;  // 循环到第一首
    }
    
    printf("切换到下一首音频，索引: %d\n", current_audio_index);
    
    // 自动播放新的音频文件
    if (audio_files[current_audio_index] != NULL) {
        const char *file_path = audio_files[current_audio_index];
        audio_player_play(file_path);
    }
    
    pthread_mutex_unlock(&button_mutex);
}
