/**
 * @file common.h
 * @brief 公共定义和全局变量声明
 */

#ifndef COMMON_H
#define COMMON_H

#include <stdbool.h>
#include <stdint.h>
#include "lvgl/lvgl.h"

// 目录路径定义
#define IMAGE_DIR "/mdata"    // 图片目录路径
#define MEDIA_DIR "/mdata"    // 音视频目录路径

// BMP文件头结构
#pragma pack(push, 1)
struct bmp_header {
    uint16_t signature;
    uint32_t file_size;
    uint16_t reserved1;
    uint16_t reserved2;
    uint32_t data_offset;
};
#pragma pack(pop)

// BMP信息头结构
#pragma pack(push, 1)
struct bmp_info_header {
    uint32_t header_size;
    int32_t width;
    int32_t height;
    uint16_t planes;
    uint16_t bpp;
    uint32_t compression;
    uint32_t image_size;
    int32_t x_pixels_per_m;
    int32_t y_pixels_per_m;
    uint32_t colors_used;
    uint32_t colors_important;
};
#pragma pack(pop)

// 全局变量声明（在common.c中定义）
extern lv_obj_t *main_screen;
extern lv_obj_t *image_screen;
extern lv_obj_t *player_screen;
extern lv_obj_t *video_container;
extern lv_obj_t *video_back_btn;  // 视频播放时的返回按钮
extern lv_obj_t *speed_label;
extern lv_obj_t *status_label;
extern lv_obj_t *playlist_container;  // 播放列表容器
extern lv_obj_t *playlist_list;  // 播放列表滚动容器
extern bool should_exit;

// 图片显示相关变量
extern lv_obj_t *current_img_obj;
extern lv_obj_t *img_container;
extern lv_obj_t *img_info_label;
extern int current_img_index;
extern bool is_gif_obj;
extern lv_color_t canvas_buf[680 * 280];

// 音频播放相关变量
extern int current_audio_index;

// 视频播放相关变量
extern int current_video_index;

// 视频退出标志（用于主线程处理返回主页）
extern bool need_return_to_main;

// 2048游戏更新显示标志（用于主线程更新游戏显示）
extern bool need_update_2048_display;

/**
 * @brief 快速刷新主屏幕（使用内存映射强制刷新framebuffer）
 * 用于视频退出后快速恢复主页显示
 */
void fast_refresh_main_screen(void);

/**
 * @brief 强制刷新主页所有按钮和控件
 * 使所有子对象无效化并重新渲染
 */
void force_refresh_main_buttons(void);

#endif /* COMMON_H */

