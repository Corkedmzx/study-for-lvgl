/**
 * @file simple_video_player.h
 * @brief 简单的MPlayer全屏视频播放器
 * 
 * 功能：
 * - 使用MPlayer全屏播放视频
 * - 触屏手势控制（左上返回、左下上一首、右下下一首、右上暂停、滑动控制）
 * - 播放和控制使用不同线程
 */

#ifndef SIMPLE_VIDEO_PLAYER_H
#define SIMPLE_VIDEO_PLAYER_H

#include <stdbool.h>

/**
 * @brief 初始化视频播放器
 */
void simple_video_init(void);

/**
 * @brief 播放视频文件（全屏）
 * @param file_path 视频文件路径
 * @return 成功返回true，失败返回false
 */
bool simple_video_play(const char *file_path);

/**
 * @brief 停止播放
 */
void simple_video_stop(void);

/**
 * @brief 强制停止播放（立即使用SIGKILL终止mplayer进程）
 */
void simple_video_force_stop(void);

/**
 * @brief 暂停/恢复播放
 */
void simple_video_toggle_pause(void);

/**
 * @brief 上一首
 */
void simple_video_prev(void);

/**
 * @brief 下一首
 */
void simple_video_next(void);

/**
 * @brief 加速播放
 */
void simple_video_speed_up(void);

/**
 * @brief 减速播放
 */
void simple_video_speed_down(void);

/**
 * @brief 增加音量
 */
void simple_video_volume_up(void);

/**
 * @brief 减少音量
 */
void simple_video_volume_down(void);

/**
 * @brief 获取播放状态
 * @return 是否正在播放
 */
bool simple_video_is_playing(void);

/**
 * @brief 清理资源
 */
void simple_video_cleanup(void);

#endif /* SIMPLE_VIDEO_PLAYER_H */

