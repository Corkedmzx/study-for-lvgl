/**
 * @file media_player.h
 * @brief 媒体播放器模块头文件（兼容层，委托给独立的音频和视频播放器）
 * 
 * 重构说明：
 * - 音频播放：委托给 audio_player 模块
 * - 视频播放：委托给 simple_video_player 模块
 * - 两者完全独立，互不干扰
 */

#ifndef MEDIA_PLAYER_H
#define MEDIA_PLAYER_H

#include <stdbool.h>

/**
 * @brief 启动播放器播放文件（兼容接口）
 * @param file 文件路径
 * @param video 是否为视频文件（true=视频，false=音频）
 */
void start_mplayer(const char* file, bool video);

/**
 * @brief 停止MPlayer播放
 */
void stop_mplayer(void);

/**
 * @brief 向MPlayer发送控制命令
 * @param cmd 命令字符串
 */
void send_command(const char* cmd);

/**
 * @brief 检查文件是否存在
 * @param path 文件路径
 */
void check_file_exists(const char *path);

/**
 * @brief 获取当前播放状态
 * @return 是否正在播放
 */
bool get_is_playing(void);

/**
 * @brief 获取当前暂停状态
 * @return 是否已暂停
 */
bool get_is_paused(void);

/**
 * @brief 获取当前媒体类型
 * @return 是否为视频
 */
bool get_is_video(void);

/**
 * @brief 设置播放状态
 * @param playing 播放状态
 */
void set_is_playing(bool playing);

/**
 * @brief 设置暂停状态
 * @param paused 暂停状态
 */
void set_is_paused(bool paused);

/**
 * @brief 设置媒体类型
 * @param video 是否为视频
 */
void set_is_video(bool video);

/**
 * @brief 获取当前播放速度
 * @return 播放速度
 */
float get_current_speed(void);

/**
 * @brief 设置当前播放速度
 * @param speed 播放速度
 */
void set_current_speed(float speed);

/**
 * @brief 获取MPlayer进程ID
 * @return 进程ID，如果未运行返回-1
 */
int get_mplayer_pid(void);

/**
 * @brief 检查MPlayer是否仍在运行
 * @return 是否仍在运行
 */
bool is_mplayer_running(void);

/**
 * @brief 设置状态更新回调函数
 * @param callback 回调函数指针
 */
void media_player_set_status_callback(void (*callback)(const char *));

#endif /* MEDIA_PLAYER_H */

