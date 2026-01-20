/**
 * @file touch_draw.h
 * @brief 触摸绘图功能模块
 */

#ifndef TOUCH_DRAW_H
#define TOUCH_DRAW_H

#include "lvgl/lvgl.h"

/**
 * @brief 显示触摸绘图窗口
 */
void touch_draw_win_show(void);

/**
 * @brief 隐藏触摸绘图窗口
 */
void touch_draw_win_hide(void);

/**
 * @brief 初始化触摸绘图模块
 */
void touch_draw_init(void);

/**
 * @brief 清理触摸绘图模块
 */
void touch_draw_cleanup(void);

/**
 * @brief 检查触摸绘图模式是否激活
 * @return true if touch draw mode is active, false otherwise
 */
bool touch_draw_is_active(void);

#endif /* TOUCH_DRAW_H */
