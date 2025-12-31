/**
 * @file ui_screens.c
 * @brief UI界面创建模块实现
 */

#include "ui_screens.h"
#include "album_win.h"
#include "music_win.h"
#include "video_win.h"
#include "led_win.h"
#include "weather_win.h"
#include "exit_win.h"
#include "timer_win.h"
#include "../common/common.h"
#include "../image_viewer/image_viewer.h"
#include "../media_player/audio_player.h"
#include "../media_player/simple_video_player.h"
#include "../file_scanner/file_scanner.h"
#include <stdio.h>
#include <string.h>
#include "lvgl/lvgl.h"
#include "lvgl/src/font/lv_font.h"

/* 声明SourceHanSansSC_VF字体（定义在bin/SourceHanSansSC_VF.c中） */
#if LV_FONT_SOURCE_HAN_SANS_SC_VF
extern const lv_font_t SourceHanSansSC_VF;
#endif

/**
 * @brief 创建主屏幕
 */
void create_main_screen(void) {
    main_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(main_screen, lv_color_hex(0xf0f0f0), 0);
    
    /* 创建标题 */
    lv_obj_t *title = lv_label_create(main_screen);
    lv_label_set_text(title, "LVGL多媒体系统");
    lv_obj_set_style_text_font(title, &SourceHanSansSC_VF, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0x1a1a1a), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);
    
    /* 创建按钮容器 - 参考main_win.c的布局 */
    // 创建相册按钮
    lv_obj_t *album_btn = lv_btn_create(main_screen);
    lv_obj_set_size(album_btn, 150, 80);
    lv_obj_align(album_btn, LV_ALIGN_TOP_LEFT, 30, 60);
    lv_obj_t *album_label = lv_label_create(album_btn);
    lv_label_set_text(album_label, "相册");
    lv_obj_set_style_text_font(album_label, &SourceHanSansSC_VF, 0);
    lv_obj_center(album_label);
    lv_obj_add_event_cb(album_btn, main_window_event_handler, LV_EVENT_CLICKED, NULL);

    // 创建视频按钮
    lv_obj_t *video_btn = lv_btn_create(main_screen);
    lv_obj_set_size(video_btn, 150, 80);
    lv_obj_align(video_btn, LV_ALIGN_TOP_MID, 0, 60);
    lv_obj_t *video_label = lv_label_create(video_btn);
    lv_label_set_text(video_label, "视频");
    lv_obj_set_style_text_font(video_label, &SourceHanSansSC_VF, 0);
    lv_obj_center(video_label);
    lv_obj_add_event_cb(video_btn, main_window_event_handler, LV_EVENT_CLICKED, NULL);

    // 创建音乐按钮
    lv_obj_t *music_btn = lv_btn_create(main_screen);
    lv_obj_set_size(music_btn, 150, 80);
    lv_obj_align(music_btn, LV_ALIGN_TOP_RIGHT, -30, 60);
    lv_obj_t *music_label = lv_label_create(music_btn);
    lv_label_set_text(music_label, "音乐");
    lv_obj_set_style_text_font(music_label, &SourceHanSansSC_VF, 0);
    lv_obj_center(music_label);
    lv_obj_add_event_cb(music_btn, main_window_event_handler, LV_EVENT_CLICKED, NULL);

    // 创建LED按钮（第二行，左侧）
    lv_obj_t *led_btn = lv_btn_create(main_screen);
    lv_obj_set_size(led_btn, 150, 80);
    lv_obj_align(led_btn, LV_ALIGN_TOP_LEFT, 30, 160);
    lv_obj_t *led_label = lv_label_create(led_btn);
    lv_label_set_text(led_label, "LED控制");
    lv_obj_set_style_text_font(led_label, &SourceHanSansSC_VF, 0);
    lv_obj_center(led_label);
    lv_obj_add_event_cb(led_btn, main_window_event_handler, LV_EVENT_CLICKED, NULL);

    // 创建天气按钮（第二行，中间）
    lv_obj_t *weather_btn = lv_btn_create(main_screen);
    lv_obj_set_size(weather_btn, 150, 80);
    lv_obj_align(weather_btn, LV_ALIGN_TOP_MID, 0, 160);
    lv_obj_t *weather_label = lv_label_create(weather_btn);
    lv_label_set_text(weather_label, "天气");
    lv_obj_set_style_text_font(weather_label, &SourceHanSansSC_VF, 0);
    lv_obj_center(weather_label);
    lv_obj_add_event_cb(weather_btn, main_window_event_handler, LV_EVENT_CLICKED, NULL);

    // 创建计时器按钮（第二行，右侧）
    lv_obj_t *timer_btn = lv_btn_create(main_screen);
    lv_obj_set_size(timer_btn, 150, 80);
    lv_obj_align(timer_btn, LV_ALIGN_TOP_RIGHT, -30, 160);
    lv_obj_t *timer_label = lv_label_create(timer_btn);
    lv_label_set_text(timer_label, "计时器");
    lv_obj_set_style_text_font(timer_label, &SourceHanSansSC_VF, 0);
    lv_obj_center(timer_label);
    lv_obj_add_event_cb(timer_btn, main_window_event_handler, LV_EVENT_CLICKED, NULL);

    // 创建退出按钮（第三行，居中）
    lv_obj_t *exit_btn = lv_btn_create(main_screen);
    lv_obj_set_size(exit_btn, 150, 80);
    lv_obj_align(exit_btn, LV_ALIGN_TOP_MID, 0, 260);
    lv_obj_t *exit_label = lv_label_create(exit_btn);
    lv_label_set_text(exit_label, "退出");
    lv_obj_set_style_text_font(exit_label, &SourceHanSansSC_VF, 0);
    lv_obj_center(exit_label);
    lv_obj_add_event_cb(exit_btn, main_window_event_handler, LV_EVENT_CLICKED, NULL);
}

/**
 * @brief 创建图片展示屏幕
 */
void create_image_screen(void) {
    image_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(image_screen, lv_color_hex(0xf0f0f0), 0);
    
    /* 创建返回按钮（提高层级，确保不被遮挡） */
    lv_obj_t *back_btn = lv_btn_create(image_screen);
    lv_obj_set_size(back_btn, 80, 40);
    lv_obj_set_style_bg_color(back_btn, lv_color_hex(0x9E9E9E), 0);
    lv_obj_align(back_btn, LV_ALIGN_TOP_LEFT, 10, 10);
    lv_obj_move_foreground(back_btn);
    lv_obj_t *back_label = lv_label_create(back_btn);
    lv_label_set_text(back_label, "返回");
    lv_obj_set_style_text_font(back_label, &SourceHanSansSC_VF, 0);
    lv_obj_center(back_label);
    lv_obj_add_event_cb(back_btn, back_to_main_cb, LV_EVENT_CLICKED, NULL);
    
    /* 创建标题 */
    lv_obj_t *title = lv_label_create(image_screen);
    lv_label_set_text(title, "图片展示");
    lv_obj_set_style_text_font(title, &SourceHanSansSC_VF, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0x1a1a1a), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 20);
    
    /* 扫描图片文件（如果还没有扫描） */
    extern int image_count;
    extern char **image_files;
    if (image_count == 0 || image_files == NULL) {
        extern int scan_image_directory(const char *);
        scan_image_directory(IMAGE_DIR);
    }
    
    /* 不在这里加载图片，等进入相册时再加载 */
    /* show_images(); */
}

/**
 * @brief 创建播放器屏幕
 */
void create_player_screen(void) {
    player_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(player_screen, lv_color_hex(0xf0f0f0), 0);
    
    /* 创建视频容器（使用mmap内存映射，减少容器面积） */
    video_container = lv_obj_create(player_screen);
    lv_obj_set_size(video_container, 640, 360);  // 减小尺寸，使用mmap提高性能
    lv_obj_align(video_container, LV_ALIGN_TOP_MID, 0, 50);  // 向下移动，预留顶部空间
    lv_obj_set_style_border_width(video_container, 0, 0);
    lv_obj_set_style_bg_color(video_container, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(video_container, LV_OPA_TRANSP, 0);
    lv_obj_add_flag(video_container, LV_OBJ_FLAG_HIDDEN);
    
    // 在顶部添加返回按钮（视频播放时可见）- 使用全局变量
    extern lv_obj_t *video_back_btn;
    video_back_btn = lv_btn_create(player_screen);
    lv_obj_set_size(video_back_btn, 80, 40);
    lv_obj_set_style_bg_color(video_back_btn, lv_color_hex(0x9E9E9E), 0);
    lv_obj_align(video_back_btn, LV_ALIGN_TOP_LEFT, 10, 10);
    lv_obj_move_foreground(video_back_btn);  // 确保在最上层
    lv_obj_t *video_back_label = lv_label_create(video_back_btn);
    lv_label_set_text(video_back_label, "返回");
    lv_obj_set_style_text_font(video_back_label, &SourceHanSansSC_VF, 0);
    lv_obj_center(video_back_label);
    lv_obj_add_event_cb(video_back_btn, back_to_main_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_flag(video_back_btn, LV_OBJ_FLAG_HIDDEN);  // 默认隐藏，视频播放时显示
    
    // 添加提示文字
    lv_obj_t *video_hint = lv_label_create(video_container);
    lv_label_set_text(video_hint, "视频播放区域\n(640x360)\n使用内存映射提高性能");
    lv_obj_set_style_text_font(video_hint, &SourceHanSansSC_VF, 0);
    lv_obj_set_style_text_color(video_hint, lv_color_hex(0xffffff), 0);
    lv_obj_center(video_hint);
    
    
    /* 添加多媒体控制区域 */
    lv_obj_t *media_panel = lv_obj_create(player_screen);
    lv_obj_set_size(media_panel, 800, 80);
    lv_obj_align(media_panel, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(media_panel, lv_color_hex(0xe0e0e0), 0);
    lv_obj_set_style_border_width(media_panel, 0, 0);
    lv_obj_set_style_radius(media_panel, 15, 0);
    lv_obj_set_style_pad_all(media_panel, 10, 0);
    
    /* 创建控制按钮容器 */
    lv_obj_t *btn_container = lv_obj_create(media_panel);
    lv_obj_set_size(btn_container, 780, 60);
    lv_obj_align(btn_container, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_flex_flow(btn_container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btn_container, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_border_width(btn_container, 0, 0);
    lv_obj_set_style_bg_opa(btn_container, LV_OPA_0, 0);
    lv_obj_set_style_pad_gap(btn_container, 6, 0);
    
    // 返回按钮
    lv_obj_t *back_btn = lv_btn_create(btn_container);
    lv_obj_set_size(back_btn, 60, 45);
    lv_obj_set_style_bg_color(back_btn, lv_color_hex(0x9E9E9E), 0);
    lv_obj_t *back_label = lv_label_create(back_btn);
    lv_label_set_text(back_label, "返回");
    lv_obj_set_style_text_font(back_label, &SourceHanSansSC_VF, 0);
    lv_obj_center(back_label);
    lv_obj_add_event_cb(back_btn, back_to_main_cb, LV_EVENT_CLICKED, NULL);
    
    // 音量减按钮
    extern void volume_down_cb(lv_event_t * e);
    lv_obj_t *vol_down_btn = lv_btn_create(btn_container);
    lv_obj_set_size(vol_down_btn, 70, 50);
    lv_obj_t *vol_down_label = lv_label_create(vol_down_btn);
    lv_label_set_text(vol_down_label, "音量-");
    lv_obj_set_style_text_font(vol_down_label, &SourceHanSansSC_VF, 0);
    lv_obj_center(vol_down_label);
    lv_obj_add_event_cb(vol_down_btn, volume_down_cb, LV_EVENT_CLICKED, NULL);
    
    // 音量加按钮
    extern void volume_up_cb(lv_event_t * e);
    lv_obj_t *vol_up_btn = lv_btn_create(btn_container);
    lv_obj_set_size(vol_up_btn, 70, 50);
    lv_obj_t *vol_up_label = lv_label_create(vol_up_btn);
    lv_label_set_text(vol_up_label, "音量+");
    lv_obj_set_style_text_font(vol_up_label, &SourceHanSansSC_VF, 0);
    lv_obj_center(vol_up_label);
    lv_obj_add_event_cb(vol_up_btn, volume_up_cb, LV_EVENT_CLICKED, NULL);
    
    // 减速按钮
    extern void slower_cb(lv_event_t * e);
    lv_obj_t *slower_btn = lv_btn_create(btn_container);
    lv_obj_set_size(slower_btn, 70, 50);
    lv_obj_t *slower_label = lv_label_create(slower_btn);
    lv_label_set_text(slower_label, "速度-");
    lv_obj_set_style_text_font(slower_label, &SourceHanSansSC_VF, 0);
    lv_obj_center(slower_label);
    lv_obj_add_event_cb(slower_btn, slower_cb, LV_EVENT_CLICKED, NULL);
    
    // 加速按钮
    extern void faster_cb(lv_event_t * e);
    lv_obj_t *faster_btn = lv_btn_create(btn_container);
    lv_obj_set_size(faster_btn, 70, 50);
    lv_obj_t *faster_label = lv_label_create(faster_btn);
    lv_label_set_text(faster_label, "速度+");
    lv_obj_set_style_text_font(faster_label, &SourceHanSansSC_VF, 0);
    lv_obj_center(faster_label);
    lv_obj_add_event_cb(faster_btn, faster_cb, LV_EVENT_CLICKED, NULL);
    
    // 播放按钮
    extern void play_audio_cb(lv_event_t * e);
    lv_obj_t *play_btn = lv_btn_create(btn_container);
    lv_obj_set_size(play_btn, 80, 50);
    lv_obj_set_style_bg_color(play_btn, lv_color_hex(0x4CAF50), 0);
    lv_obj_t *play_label = lv_label_create(play_btn);
    lv_label_set_text(play_label, "播放");
    lv_obj_set_style_text_font(play_label, &SourceHanSansSC_VF, 0);
    lv_obj_center(play_label);
    lv_obj_add_event_cb(play_btn, play_audio_cb, LV_EVENT_CLICKED, NULL);
    
    // 停止按钮
    extern void stop_cb(lv_event_t * e);
    lv_obj_t *stop_btn = lv_btn_create(btn_container);
    lv_obj_set_size(stop_btn, 70, 50);
    lv_obj_set_style_bg_color(stop_btn, lv_color_hex(0xF44336), 0);
    lv_obj_t *stop_label = lv_label_create(stop_btn);
    lv_label_set_text(stop_label, "停止");
    lv_obj_set_style_text_font(stop_label, &SourceHanSansSC_VF, 0);
    lv_obj_center(stop_label);
    lv_obj_add_event_cb(stop_btn, stop_cb, LV_EVENT_CLICKED, NULL);
    
    // 上一首按钮
    extern void prev_media_cb(lv_event_t * e);
    lv_obj_t *prev_btn = lv_btn_create(btn_container);
    lv_obj_set_size(prev_btn, 70, 50);
    lv_obj_set_style_bg_color(prev_btn, lv_color_hex(0x2196F3), 0);
    lv_obj_t *prev_label = lv_label_create(prev_btn);
    lv_label_set_text(prev_label, "上一首");
    lv_obj_set_style_text_font(prev_label, &SourceHanSansSC_VF, 0);
    lv_obj_center(prev_label);
    lv_obj_add_event_cb(prev_btn, prev_media_cb, LV_EVENT_CLICKED, NULL);
    
    // 下一首按钮
    extern void next_media_cb(lv_event_t * e);
    lv_obj_t *next_btn = lv_btn_create(btn_container);
    lv_obj_set_size(next_btn, 70, 50);
    lv_obj_set_style_bg_color(next_btn, lv_color_hex(0x2196F3), 0);
    lv_obj_t *next_label = lv_label_create(next_btn);
    lv_label_set_text(next_label, "下一首");
    lv_obj_set_style_text_font(next_label, &SourceHanSansSC_VF, 0);
    lv_obj_center(next_label);
    lv_obj_add_event_cb(next_btn, next_media_cb, LV_EVENT_CLICKED, NULL);
    
    /* 创建状态显示区域（在控制按钮上方） */
    lv_obj_t *status_container = lv_obj_create(player_screen);
    lv_obj_set_size(status_container, 750, 30);
    lv_obj_align(status_container, LV_ALIGN_BOTTOM_MID, 0, -90);
    lv_obj_set_flex_flow(status_container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(status_container, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_border_width(status_container, 0, 0);
    lv_obj_set_style_bg_opa(status_container, LV_OPA_0, 0);
    
    // 状态标签
    status_label = lv_label_create(status_container);
    lv_label_set_text(status_label, "状态: 未开始");
    lv_obj_set_style_text_font(status_label, &SourceHanSansSC_VF, 0);
    lv_obj_set_style_text_color(status_label, lv_color_hex(0x1a1a1a), 0);
    
    // 速度标签
    speed_label = lv_label_create(status_container);
    lv_label_set_text(speed_label, "速度: 1.00x");
    lv_obj_set_style_text_font(speed_label, &SourceHanSansSC_VF, 0);
    lv_obj_set_style_text_color(speed_label, lv_color_hex(0x1a1a1a), 0);
}

/**
 * @brief 返回主屏幕回调
 */
void back_to_main_cb(lv_event_t * e) {
    // 检查事件类型，确保是点击事件
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    
    // 停止任何正在播放的媒体
    extern bool simple_video_is_playing(void);
    extern void simple_video_stop(void);
    extern bool audio_player_is_playing(void);
    extern void audio_player_stop(void);
    
    // 停止视频播放
    extern void video_touch_control_stop(void);
    if (simple_video_is_playing()) {
        // 先停止触屏控制线程（确保不再处理触屏事件）
        video_touch_control_stop();
        // 等待触屏控制线程完全停止
        usleep(150000);  // 150ms
        // 立即强制停止mplayer（使用SIGKILL）
        extern void simple_video_force_stop(void);
        simple_video_force_stop();
        // 等待MPlayer完全退出和framebuffer恢复
        usleep(200000);  // 200ms，确保framebuffer已恢复
    }
    
    // 停止音频播放
    if (audio_player_is_playing()) {
        audio_player_stop();
    }
    
    // 清理所有模块状态
    extern lv_obj_t *main_screen;
    extern lv_obj_t *player_screen;
    extern lv_obj_t *image_screen;
    
    // 停止并隐藏所有功能模块
    if (player_screen) {
        lv_obj_add_flag(player_screen, LV_OBJ_FLAG_HIDDEN);
    }
    if (image_screen) {
        lv_obj_add_flag(image_screen, LV_OBJ_FLAG_HIDDEN);
    }
    if (video_screen) {
        lv_obj_add_flag(video_screen, LV_OBJ_FLAG_HIDDEN);
    }
    if (weather_window) {
        lv_obj_add_flag(weather_window, LV_OBJ_FLAG_HIDDEN);
    }
    
    // 恢复主屏幕
    if (main_screen) {
        // 先确保主屏幕可见
        lv_obj_clear_flag(main_screen, LV_OBJ_FLAG_HIDDEN);
        // 切换到主屏幕
        lv_scr_load(main_screen);
        
        // 使用快速刷新函数强制刷新整个屏幕
        extern void fast_refresh_main_screen(void);
        fast_refresh_main_screen();
        
        // 额外等待确保显示稳定
        usleep(50000);  // 50ms
        // 再次处理定时器和刷新
        lv_timer_handler();
        lv_refr_now(NULL);
    }
}

/**
 * @brief 显示图片屏幕回调
 */
void show_image_screen_cb(lv_event_t * e) {
    (void)e;
    
    extern lv_obj_t *image_screen;
    lv_scr_load(image_screen);
}

/**
 * @brief 显示播放器屏幕回调
 */
void show_player_screen_cb(lv_event_t * e) {
    (void)e;
    
    extern lv_obj_t *player_screen;
    lv_scr_load(player_screen);
}

/**
 * @brief 主窗口事件处理器（参考main_win.c）
 */
void main_window_event_handler(lv_event_t *e) {
    // 检查事件类型，确保是点击事件
    if(lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    
    // 确保主屏幕显示（在点击按钮时）
    if (main_screen) {
        lv_obj_clear_flag(main_screen, LV_OBJ_FLAG_HIDDEN);
    }
    
    lv_obj_t *btn = lv_event_get_target(e);
    lv_obj_t *label = lv_obj_get_child(btn, 0);
    if (!label) return;
    
    const char *text = lv_label_get_text(label);
    if (!text) return;
    
    // 在进入任何功能前，先清理所有其他模块
    extern lv_obj_t *image_screen;
    extern lv_obj_t *player_screen;
    extern bool simple_video_is_playing(void);
    extern void simple_video_stop(void);
    extern void video_touch_control_stop(void);
    extern bool audio_player_is_playing(void);
    extern void audio_player_stop(void);
    
    // 停止所有播放
    if (simple_video_is_playing()) {
        video_touch_control_stop();
        simple_video_stop();
    }
    if (audio_player_is_playing()) {
        audio_player_stop();
    }
    
    // 隐藏所有功能屏幕
    if (image_screen) {
        lv_obj_add_flag(image_screen, LV_OBJ_FLAG_HIDDEN);
    }
    if (player_screen) {
        lv_obj_add_flag(player_screen, LV_OBJ_FLAG_HIDDEN);
    }
    if (video_screen) {
        lv_obj_add_flag(video_screen, LV_OBJ_FLAG_HIDDEN);
    }
    if (weather_window) {
        lv_obj_add_flag(weather_window, LV_OBJ_FLAG_HIDDEN);
    }
    extern lv_obj_t *timer_window;
    if (timer_window) {
        lv_obj_add_flag(timer_window, LV_OBJ_FLAG_HIDDEN);
    }
    
    // 根据按钮文本执行相应功能
    if(strcmp(text, "相册") == 0) {
        show_album_window();
    } else if(strcmp(text, "视频") == 0) {
        video_win_show();
    } else if(strcmp(text, "音乐") == 0) {
        music_win_show();
    } else if(strcmp(text, "LED控制") == 0) {
        show_led_window();
    } else if(strcmp(text, "天气") == 0) {
        show_weather_window();
    } else if(strcmp(text, "计时器") == 0) {
        timer_win_show();
    } else if(strcmp(text, "退出") == 0) {
        exit_application();
    }
}

/**
 * @brief 退出程序回调（保留兼容性）
 */
void exit_program_cb(lv_event_t * e) {
    (void)e;
    exit_application();
}

