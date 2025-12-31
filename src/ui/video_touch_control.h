/**
 * @file video_touch_control.h
 * @brief 视频播放触屏手势控制
 * 
 * 控制区域：
 * - 左上：返回主页
 * - 左下：上一首
 * - 右下：下一首
 * - 右上：暂停
 * - 左滑：减速
 * - 右滑：加速
 * - 上划：加音量
 * - 下划：减音量
 */

#ifndef VIDEO_TOUCH_CONTROL_H
#define VIDEO_TOUCH_CONTROL_H

#include <stdbool.h>

/**
 * @brief 初始化触屏控制
 */
void video_touch_control_init(void);

/**
 * @brief 启动触屏控制线程
 */
void video_touch_control_start(void);

/**
 * @brief 停止触屏控制线程
 */
void video_touch_control_stop(void);

/**
 * @brief 处理触屏事件
 * @param x 触摸X坐标
 * @param y 触摸Y坐标
 * @param pressed 是否按下
 */
void video_touch_control_handle_event(int x, int y, bool pressed);

#endif /* VIDEO_TOUCH_CONTROL_H */

