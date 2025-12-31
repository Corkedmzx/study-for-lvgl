/**
 * @file audio_player.h
 * @brief 独立的音频播放器模块（仅处理音频文件）
 * 
 * 功能：
 * - 使用MPlayer播放音频文件
 * - 独立的播放状态管理
 * - 与视频播放器完全分离
 */

#ifndef AUDIO_PLAYER_H
#define AUDIO_PLAYER_H

#include <stdbool.h>

/**
 * @brief 初始化音频播放器
 */
void audio_player_init(void);

/**
 * @brief 播放音频文件
 * @param file_path 音频文件路径
 * @return 成功返回true，失败返回false
 */
bool audio_player_play(const char *file_path);

/**
 * @brief 停止播放
 */
void audio_player_stop(void);

/**
 * @brief 暂停/恢复播放
 */
void audio_player_toggle_pause(void);

/**
 * @brief 获取播放状态
 * @return 是否正在播放
 */
bool audio_player_is_playing(void);

/**
 * @brief 获取暂停状态
 * @return 是否已暂停
 */
bool audio_player_is_paused(void);

/**
 * @brief 获取当前播放速度
 * @return 播放速度
 */
float audio_player_get_speed(void);

/**
 * @brief 设置当前播放速度
 * @param speed 播放速度
 */
void audio_player_set_speed(float speed);

/**
 * @brief 增加音量
 */
void audio_player_volume_up(void);

/**
 * @brief 减少音量
 */
void audio_player_volume_down(void);

/**
 * @brief 获取MPlayer进程ID
 * @return 进程ID，如果未运行返回-1
 */
int audio_player_get_pid(void);

/**
 * @brief 检查MPlayer是否仍在运行
 * @return 是否仍在运行
 */
bool audio_player_is_running(void);

/**
 * @brief 设置状态更新回调函数
 * @param callback 回调函数指针
 */
void audio_player_set_status_callback(void (*callback)(const char *));

/**
 * @brief 发送控制命令到音频播放器
 * @param cmd 命令字符串（如 "pause", "speed_set 1.5", "volume +10" 等）
 */
void audio_player_send_command(const char *cmd);

/**
 * @brief 清理资源
 */
void audio_player_cleanup(void);

#endif /* AUDIO_PLAYER_H */

