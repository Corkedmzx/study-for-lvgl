/**
 * @file screensaver_win.h
 * @brief 屏保窗口头文件
 */

#ifndef SCREENSAVER_WIN_H
#define SCREENSAVER_WIN_H

#include "lvgl/lvgl.h"

/**
 * @brief 显示屏保窗口
 */
void screensaver_win_show(void);

/**
 * @brief 隐藏屏保窗口
 */
void screensaver_win_hide(void);

/**
 * @brief 检查屏保是否已解锁
 */
bool screensaver_win_is_unlocked(void);

/**
 * @brief 检查并处理解锁（在主循环中调用）
 */
void screensaver_win_check_unlock(void);

#endif /* SCREENSAVER_WIN_H */

