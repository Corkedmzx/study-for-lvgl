/**
 * @file login_win.h
 * @brief 登录窗口模块
 */

#ifndef LOGIN_WIN_H
#define LOGIN_WIN_H

#include "lvgl/lvgl.h"

/**
 * @brief 显示登录窗口
 */
void login_win_show(void);

/**
 * @brief 检查是否已登录
 * @return 已登录返回true，未登录返回false
 */
bool login_win_is_logged_in(void);

/**
 * @brief 检查并处理主屏幕显示（在主循环中调用）
 */
void login_win_check_show_main(void);

#endif /* LOGIN_WIN_H */

