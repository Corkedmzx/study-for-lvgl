/**
 * @file common.c
 * @brief 公共定义和全局变量定义
 */

#include "common.h"
#include <unistd.h>  // for usleep
#include <fcntl.h>  // for open
#include <sys/mman.h>  // for mmap, msync, munmap
#include <sys/ioctl.h>  // for ioctl
#include <linux/fb.h>  // for framebuffer structures
#include <string.h>  // for memset, strcmp, strlen
#include <stdint.h>  // for intptr_t
#include "lvgl/lvgl.h"
#include "lvgl/src/widgets/lv_label.h"
#include "../media_player/simple_video_player.h"  // for simple_video_is_playing
#include <stdio.h>

// 全局变量定义
lv_obj_t *main_screen = NULL;
lv_obj_t *image_screen = NULL;
lv_obj_t *player_screen = NULL;
lv_obj_t *video_container = NULL;
lv_obj_t *video_back_btn = NULL;
lv_obj_t *speed_label = NULL;
lv_obj_t *status_label = NULL;
lv_obj_t *playlist_container = NULL;
lv_obj_t *playlist_list = NULL;
bool should_exit = false;

// 图片显示相关变量
lv_obj_t *current_img_obj = NULL;
lv_obj_t *img_container = NULL;
lv_obj_t *img_info_label = NULL;
int current_img_index = 0;
bool is_gif_obj = false;
lv_color_t canvas_buf[680 * 280];

// 音频播放相关变量
int current_audio_index = 0;

// 视频播放相关变量
int current_video_index = 0;

// 视频退出标志（用于主线程处理返回主页）
bool need_return_to_main = false;

// 2048游戏更新显示标志（用于主线程更新游戏显示）
bool need_update_2048_display = false;

/**
 * @brief 使用内存映射快速刷新主屏幕到framebuffer（简化版本，减少mmap调用）
 * 注意：在视频播放时不应该调用此函数，避免与MPlayer的framebuffer访问冲突
 */
void fast_refresh_main_screen(void) {
    lv_disp_t *disp = lv_disp_get_default();
    if (!disp) {
        return;
    }
    
    // 获取当前显示的页面screen
    extern lv_obj_t* get_main_page1_screen(void);
    lv_obj_t *current_page = get_main_page1_screen();
    if (!current_page) {
        return;
    }
    
    // 检查是否正在播放视频，如果是则不进行mmap操作，避免冲突
    extern bool simple_video_is_playing(void);
    bool video_playing = simple_video_is_playing();
    
    // 1. 先让LVGL渲染整个屏幕（使所有对象无效化）
    lv_obj_invalidate(current_page);
    
    // 2. 处理定时器，让LVGL完成渲染（减少循环次数）
    for (int i = 0; i < 20; i++) {  // 从50次减少到20次
        lv_timer_handler();
        usleep(2000);
    }
    
    // 3. 强制立即刷新，触发所有flush_cb调用
    lv_refr_now(disp);
    
    // 4. 再次处理定时器，确保所有flush_cb完成（减少循环次数）
    for (int i = 0; i < 20; i++) {  // 从50次减少到20次
        lv_timer_handler();
        usleep(2000);
    }
    
    // 5. 只在非视频播放时使用mmap同步framebuffer（避免与MPlayer冲突）
    if (!video_playing) {
        int fb_fd = open("/dev/fb0", O_RDWR);
        if (fb_fd >= 0) {
            struct fb_var_screeninfo vinfo;
            struct fb_fix_screeninfo finfo;
            
            if (ioctl(fb_fd, FBIOGET_VSCREENINFO, &vinfo) == 0 &&
                ioctl(fb_fd, FBIOGET_FSCREENINFO, &finfo) == 0) {
                long int screensize = vinfo.yres_virtual * finfo.line_length;
                char *fbp = (char *)mmap(0, screensize, PROT_READ | PROT_WRITE, MAP_SHARED, fb_fd, 0);
                
                if ((intptr_t)fbp != -1) {
                    // 使用MS_SYNC强制同步到framebuffer设备
                    msync(fbp, screensize, MS_SYNC);
                    munmap(fbp, screensize);
                }
            }
            close(fb_fd);
        }
    }
    
    // 6. 最后一次强制刷新
    lv_refr_now(disp);
}

/**
 * @brief 强制刷新主页所有按钮和控件
 * 使所有子对象无效化并重新渲染，确保按钮背景和文字不透明，恢复文字颜色
 */
void force_refresh_main_buttons(void) {
    // 获取当前显示的页面screen
    extern lv_obj_t* get_main_page1_screen(void);
    lv_obj_t *current_page = get_main_page1_screen();
    if (!current_page) {
        return;
    }
    
    // 使主屏幕及其所有子对象无效化（直接操作，不使用lv_obj_is_valid）
    lv_obj_invalidate(current_page);
    
    // 递归使所有子对象无效化，并确保背景和文字不透明，恢复颜色（只检查NULL，不使用lv_obj_is_valid）
    uint32_t child_cnt = lv_obj_get_child_cnt(current_page);
    for (uint32_t i = 0; i < child_cnt; i++) {
        // 每次循环都检查current_page是否为NULL
        if (!current_page) {
            printf("[刷新] 警告: current_page在刷新过程中变为NULL\n");
            break;
        }
        
        lv_obj_t *child = lv_obj_get_child(current_page, i);
        if (child) {
            // 确保子对象背景不透明
            lv_obj_set_style_bg_opa(child, LV_OPA_COVER, 0);
            
            // 检查是否是退出按钮（通过检查子对象文字是否为"退出"）
            bool is_exit_btn = false;
            uint32_t grandchild_cnt = lv_obj_get_child_cnt(child);
            for (uint32_t j = 0; j < grandchild_cnt; j++) {
                lv_obj_t *grandchild = lv_obj_get_child(child, j);
                if (grandchild) {
                    // 检查是否是label（通过尝试获取文字来判断）
                    const char *text = NULL;
                    // 尝试获取label文字（如果是label，会有text属性）
                    // 使用lv_label_get_text，如果不是label会返回NULL或空字符串
                    text = lv_label_get_text(grandchild);
                    if (text && strlen(text) > 0) {
                        // 这是一个label
                        if (strcmp(text, "退出") == 0) {
                            is_exit_btn = true;
                            // 恢复退出按钮的红色背景
                            lv_obj_set_style_bg_color(child, lv_color_hex(0xF44336), 0);
                            lv_obj_set_style_border_color(child, lv_color_hex(0xd32f2f), 0);
                            // 恢复退出按钮的白色文字
                            lv_obj_set_style_text_color(grandchild, lv_color_hex(0xffffff), 0);
                            lv_obj_set_style_text_opa(grandchild, LV_OPA_COVER, 0);
                        } else {
                            // 其他按钮的label，确保文字不透明
                            lv_obj_set_style_text_opa(grandchild, LV_OPA_COVER, 0);
                            // 不设置文字颜色，使用LVGL默认颜色（深色）
                        }
                    }
                    
                    // 确保所有子对象背景不透明
                    lv_obj_set_style_bg_opa(grandchild, LV_OPA_COVER, 0);
                    lv_obj_invalidate(grandchild);
                }
            }
            
            lv_obj_invalidate(child);
        }
    }
    
    // 多次处理定时器，确保所有对象重新渲染
    for (int i = 0; i < 100; i++) {  // 100次
        // 每次循环都检查current_page是否为NULL
        if (!current_page) {
            printf("[刷新] 警告: current_page在定时器处理过程中变为NULL\n");
            break;
        }
        lv_timer_handler();
        usleep(5000);  // 5000ms
    }
    
    // 最终检查后强制刷新
    if (current_page) {
        lv_refr_now(NULL);
        // 再次确保主屏幕在最上层
        lv_scr_load(current_page);
        lv_timer_handler();
        lv_refr_now(NULL);
    }
}

