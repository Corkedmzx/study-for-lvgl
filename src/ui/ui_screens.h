/**
 * @file ui_screens.h
 * @brief UI界面创建和导航回调函数
 */

#ifndef UI_SCREENS_H
#define UI_SCREENS_H

#include "lvgl/lvgl.h"
#include "ui_callbacks.h"

/**
 * @brief 创建主屏幕
 */
void create_main_screen(void);

/**
 * @brief 获取主页第一页screen（用于密码锁切换）
 */
lv_obj_t* get_main_page1_screen(void);

/**
 * @brief 获取主页第二页screen
 */
lv_obj_t* get_main_page2_screen(void);

/**
 * @brief 获取当前页面索引（0或1）
 */
int get_current_page_index(void);

/**
 * @brief 切换到指定页面（带动画）
 */
void switch_to_page(int target_page);

/**
 * @brief 创建图片展示屏幕
 */
void create_image_screen(void);

/**
 * @brief 创建播放器屏幕
 */
void create_player_screen(void);

/**
 * @brief 返回主屏幕回调
 */
void back_to_main_cb(lv_event_t * e);

/**
 * @brief 显示图片屏幕回调
 */
void show_image_screen_cb(lv_event_t * e);

/**
 * @brief 显示播放器屏幕回调
 */
void show_player_screen_cb(lv_event_t * e);

/**
 * @brief 主窗口事件处理器
 */
void main_window_event_handler(lv_event_t * e);

/**
 * @brief 退出程序回调
 */
void exit_program_cb(lv_event_t * e);

/**
 * @brief 更新播放列表显示
 */
void update_playlist(void);

/**
 * @brief 通过索引播放音频
 */
void play_audio_by_index(int index);

/**
 * @brief 初始化播放器屏幕的状态回调
 */
void init_player_screen_callbacks(void);

#endif /* UI_SCREENS_H */

