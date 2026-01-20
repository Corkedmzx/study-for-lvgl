/**
 * @file video_win.c
 * @brief 视频播放窗口（全屏MPlayer播放）
 */

#include "video_win.h"
#include "ui_screens.h"
#include "../media_player/simple_video_player.h"
#include "../ui/video_touch_control.h"
#include "../file_scanner/file_scanner.h"
#include "../common/common.h"
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>  // For access()
#include "lvgl/lvgl.h"

lv_obj_t *video_screen = NULL;  // 视频播放独立屏幕（全局变量，供其他模块访问）
static lv_obj_t *touch_overlay = NULL;  // 全屏透明层用于捕获触屏事件

/**
 * @brief 触屏事件处理回调
 */
static void touch_event_handler(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    
    // 处理所有触屏相关事件
    if (code == LV_EVENT_PRESSED || code == LV_EVENT_RELEASED || 
        code == LV_EVENT_PRESSING || code == LV_EVENT_CLICKED) {
        lv_indev_t *indev = lv_indev_get_act();
        if (indev) {
            lv_point_t point;
            lv_indev_get_point(indev, &point);
            
            bool pressed = (code == LV_EVENT_PRESSED || code == LV_EVENT_PRESSING);
            if (code == LV_EVENT_CLICKED) {
                pressed = false;  // 点击事件是释放后的
            }
            
            video_touch_control_handle_event(point.x, point.y, pressed);
        }
    }
}

/**
 * @brief 显示视频窗口并开始播放指定文件
 * @param file_path 要播放的视频文件路径（如果为NULL，则查找第一个视频文件）
 */
void video_win_show_with_file(const char *file_path) {
    // 如果正在播放视频，先停止触屏控制线程和视频播放
    bool was_playing_video = simple_video_is_playing();
    if (was_playing_video) {
        extern void video_touch_control_stop(void);
        video_touch_control_stop();
        extern void simple_video_force_stop(void);
        simple_video_force_stop();
        // 等待framebuffer恢复
        usleep(200000);  // 200ms
    }
    
    // 停止任何正在播放的媒体和其他模块
    extern bool audio_player_is_playing(void);
    extern void audio_player_stop(void);
    
    // 清理其他模块
    extern lv_obj_t *image_screen;
    extern lv_obj_t *player_screen;
    if (image_screen) {
        lv_obj_add_flag(image_screen, LV_OBJ_FLAG_HIDDEN);
    }
    if (player_screen) {
        lv_obj_add_flag(player_screen, LV_OBJ_FLAG_HIDDEN);
    }
    
    if (audio_player_is_playing()) {
        audio_player_stop();
    }
    if (simple_video_is_playing()) {
        simple_video_stop();
    }
    
    // 创建独立的视频屏幕
    if (video_screen == NULL) {
        video_screen = lv_obj_create(NULL);
        // 设置白色背景，显示白屏
        lv_obj_set_style_bg_color(video_screen, lv_color_white(), 0);
        lv_obj_set_style_bg_opa(video_screen, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(video_screen, 0, 0);
        lv_obj_set_style_pad_all(video_screen, 0, 0);
    } else {
        // 确保背景是白色
        lv_obj_set_style_bg_color(video_screen, lv_color_white(), 0);
        lv_obj_set_style_bg_opa(video_screen, LV_OPA_COVER, 0);
    }
    
    // 隐藏主屏幕
    extern lv_obj_t *main_screen;
    if (main_screen) {
        lv_obj_add_flag(main_screen, LV_OBJ_FLAG_HIDDEN);
    }
    
    // 切换到视频屏幕（显示白屏）
    lv_scr_load(video_screen);
    
    // 强制刷新屏幕，确保白屏显示
    lv_refr_now(NULL);
    usleep(100000);  // 100ms，确保白屏显示
    
    // 创建一个全屏透明层用于捕获触屏事件
    // 注意：MPlayer全屏播放时，framebuffer被占用，但LVGL仍可以处理触屏事件
    if (touch_overlay == NULL) {
        touch_overlay = lv_obj_create(video_screen);
        lv_obj_set_size(touch_overlay, LV_HOR_RES, LV_VER_RES);
        lv_obj_align(touch_overlay, LV_ALIGN_TOP_LEFT, 0, 0);
        lv_obj_set_style_bg_opa(touch_overlay, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(touch_overlay, 0, 0);
        lv_obj_set_style_pad_all(touch_overlay, 0, 0);
        lv_obj_clear_flag(touch_overlay, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(touch_overlay, LV_OBJ_FLAG_CLICKABLE);
        // 注册所有可能的触屏事件
        lv_obj_add_event_cb(touch_overlay, touch_event_handler, LV_EVENT_PRESSED, NULL);
        lv_obj_add_event_cb(touch_overlay, touch_event_handler, LV_EVENT_RELEASED, NULL);
        lv_obj_add_event_cb(touch_overlay, touch_event_handler, LV_EVENT_PRESSING, NULL);
        lv_obj_add_event_cb(touch_overlay, touch_event_handler, LV_EVENT_CLICKED, NULL);
        lv_obj_move_foreground(touch_overlay);  // 确保在最上层
    } else {
        // 确保透明层在正确的屏幕上
        lv_obj_set_parent(touch_overlay, video_screen);
        lv_obj_clear_flag(touch_overlay, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(touch_overlay);  // 确保在最上层
    }
    
    // 播放指定的视频文件或查找第一个视频文件
    bool found = false;
    
    if (file_path != NULL) {
        // 播放指定的文件
        printf("开始播放视频: %s\n", file_path);
        // 先启动触屏控制线程
        video_touch_control_start();
        printf("触屏控制已启动\n");
        // 再播放视频
        if (simple_video_play(file_path)) {
            found = true;
            printf("视频播放已启动\n");
        } else {
            printf("错误: 视频播放启动失败\n");
            // 如果播放失败，停止触屏控制
            video_touch_control_stop();
        }
    } else {
    // 查找第一个视频文件并开始播放
        extern char **video_files;
        extern int video_count;
        extern int current_video_index;
    
        if (video_files != NULL && video_count > 0) {
            current_video_index = 0;
            const char *file = video_files[0];
            printf("开始播放视频: %s\n", file);
            // 先启动触屏控制线程
            video_touch_control_start();
            printf("触屏控制已启动\n");
            // 再播放视频
            if (simple_video_play(file)) {
                found = true;
                printf("视频播放已启动\n");
            } else {
                printf("错误: 视频播放启动失败\n");
                // 如果播放失败，停止触屏控制
                video_touch_control_stop();
            }
        }
    }
    
    if (!found) {
        printf("警告: 未找到视频文件\n");
        // 如果没有找到视频，隐藏触屏层
        if (touch_overlay) {
            lv_obj_add_flag(touch_overlay, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

/**
 * @brief 显示视频窗口并开始播放（查找第一个视频文件）
 */
void video_win_show(void) {
    video_win_show_with_file(NULL);
}

/**
 * @brief 视频窗口事件处理
 */
void video_win_event_handler(lv_event_t *e) {
    // 先停止触屏控制线程
    extern void video_touch_control_stop(void);
    video_touch_control_stop();
    // 等待触屏控制线程完全停止
    usleep(500000);  // 500ms，确保线程完全退出
    
    // 立即强制停止mplayer（单线程退出逻辑，不使用互斥锁）
    extern void simple_video_force_stop(void);
    simple_video_force_stop();
    
    // 等待framebuffer完全恢复
    usleep(500000);  // 500ms，确保framebuffer已完全恢复
    
    // 清理视频模块资源
    if (touch_overlay) {
        lv_obj_del(touch_overlay);
        touch_overlay = NULL;
    }
    
    // 隐藏视频屏幕
    extern lv_obj_t *video_screen;
    if (video_screen) {
        lv_obj_add_flag(video_screen, LV_OBJ_FLAG_HIDDEN);
    }
    
    // 返回主屏幕（显示加载界面）
    extern lv_obj_t* get_main_page1_screen(void);
    lv_obj_t *main_page_screen = get_main_page1_screen();
    if (main_page_screen) {
        // 先确保主屏幕可见
        lv_obj_clear_flag(main_page_screen, LV_OBJ_FLAG_HIDDEN);
        // 切换到主屏幕
        lv_scr_load(main_page_screen);
        lv_refr_now(NULL);
        
        // 创建加载界面（全屏遮挡层）
        lv_obj_t *loading_screen = lv_obj_create(main_page_screen);
        lv_obj_set_size(loading_screen, LV_HOR_RES, LV_VER_RES);
        lv_obj_align(loading_screen, LV_ALIGN_TOP_LEFT, 0, 0);
        lv_obj_set_style_bg_color(loading_screen, lv_color_hex(0xf5f5f5), 0);
        lv_obj_set_style_bg_opa(loading_screen, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(loading_screen, 0, 0);
        lv_obj_set_style_pad_all(loading_screen, 0, 0);
        lv_obj_clear_flag(loading_screen, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(loading_screen, LV_OBJ_FLAG_CLICKABLE);  // 可点击，但会阻止下层操作
        lv_obj_move_foreground(loading_screen);  // 确保在最上层
        
        // 创建加载提示文字
        extern lv_font_t SourceHanSansSC_VF;
        lv_obj_t *loading_label = lv_label_create(loading_screen);
        lv_label_set_text(loading_label, "正在返回主页...");
        lv_obj_set_style_text_font(loading_label, &SourceHanSansSC_VF, 0);
        lv_obj_set_style_text_color(loading_label, lv_color_hex(0x333333), 0);
        lv_obj_align(loading_label, LV_ALIGN_CENTER, 0, -30);
        
        // 创建进度条
        lv_obj_t *loading_bar = lv_bar_create(loading_screen);
        lv_obj_set_size(loading_bar, 300, 20);
        lv_obj_align(loading_bar, LV_ALIGN_CENTER, 0, 20);
        lv_bar_set_range(loading_bar, 0, 100);
        lv_bar_set_value(loading_bar, 0, LV_ANIM_OFF);
        lv_obj_set_style_bg_color(loading_bar, lv_color_hex(0xe0e0e0), 0);
        lv_obj_set_style_bg_color(loading_bar, lv_color_hex(0x2196F3), LV_PART_INDICATOR);
        
        // 立即显示加载界面
        lv_refr_now(NULL);
        
            // 模拟进度条动画（给用户反馈）
            for (int i = 0; i <= 100; i += 5) {
                lv_bar_set_value(loading_bar, i, LV_ANIM_ON);
                lv_timer_handler();
                usleep(20000);  // 20ms，更平滑的动画
            }
            
            // 使用快速刷新函数强制刷新整个屏幕
            extern void fast_refresh_main_screen(void);
            fast_refresh_main_screen();
            
            // 多次刷新确保主页完全加载（增加刷新次数和时间）
            for (int i = 0; i < 15; i++) {  // 从10次增加到15次
                lv_timer_handler();
                lv_refr_now(NULL);
                usleep(100000);  // 100ms，总共约1.5秒
            }
            
            // 额外等待确保所有UI元素完全渲染
            usleep(300000);  // 从200ms增加到300ms
            lv_timer_handler();
            lv_refr_now(NULL);
            
            // 再次强制刷新framebuffer
            fast_refresh_main_screen();
        
        // 隐藏加载界面
        lv_obj_del(loading_screen);
        
        // 最后刷新一次，确保主页显示
        lv_timer_handler();
        lv_refr_now(NULL);
    }
}
