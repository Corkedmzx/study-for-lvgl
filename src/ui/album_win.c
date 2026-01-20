#include "album_win.h"
#include "ui_screens.h"
#include "../image_viewer/image_viewer.h"
#include "../common/common.h"
#include <string.h>
#include "lvgl/lvgl.h"
#include "lvgl/src/font/lv_font.h"

/* 声明SourceHanSansSC_VF字体（定义在bin/SourceHanSansSC_VF.c中） */
#if LV_FONT_SOURCE_HAN_SANS_SC_VF
extern const lv_font_t SourceHanSansSC_VF;
#endif

static lv_obj_t *album_win = NULL;
static lv_obj_t *img_display = NULL;

static const char *image_paths[] = {
    "S:/mnt/udisk/1.bmp",
    "S:/mnt/udisk/2.bmp",
    "S:/mnt/udisk/3.bmp"
};
static int current_image = 0;

static void prev_btn_event_handler(lv_event_t *e) {
    // 检查事件类型，确保是点击事件
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    
    extern char **image_files;
    extern int image_count;
    
    if (image_count > 0 && image_files) {
        extern int current_img_index;
        current_img_index--;
        if (current_img_index < 0) {
            current_img_index = image_count - 1;
        }
        show_current_image();
    } else {
        // 使用硬编码的路径
        if(--current_image < 0) current_image = 2;
        if (img_display) {
            lv_img_set_src(img_display, image_paths[current_image]);
        }
    }
}

static void next_btn_event_handler(lv_event_t *e) {
    // 检查事件类型，确保是点击事件
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    
    extern char **image_files;
    extern int image_count;
    
    if (image_count > 0 && image_files) {
        extern int current_img_index;
        current_img_index++;
        if (current_img_index >= image_count) {
            current_img_index = 0;
        }
        show_current_image();
    } else {
        // 使用硬编码的路径
        if(++current_image > 2) current_image = 0;
        if (img_display) {
            lv_img_set_src(img_display, image_paths[current_image]);
        }
    }
}

void show_album_window(void) {
    // 创建图片显示区域
    extern char **image_files;
    extern int image_count;
    
    // 如果有图片文件，使用image_screen
    if (image_count > 0 && image_files) {
        // 直接使用image_viewer模块的功能
        extern lv_obj_t *image_screen;
        extern void show_current_image(void);
        extern int current_img_index;
        
        // 确保image_screen已创建
        if (!image_screen) {
            extern void create_image_screen(void);
            create_image_screen();
        }
        
        // 确保image_screen显示
        if (image_screen) {
            lv_obj_clear_flag(image_screen, LV_OBJ_FLAG_HIDDEN);
        }
        
        // 检查UI元素是否已创建，如果没有则创建
        extern lv_obj_t *current_img_obj;
        extern lv_obj_t *img_container;
        if (current_img_obj == NULL || img_container == NULL) {
            extern void show_images(void);
            show_images();  // 这会创建UI元素并加载第一张图片
        } else {
            // UI元素已存在，只需重置索引并显示第一张图片
            current_img_index = 0;
            show_current_image();
        }
        
        // 确保主屏幕隐藏
        extern lv_obj_t *main_screen;
        if (main_screen) {
            lv_obj_add_flag(main_screen, LV_OBJ_FLAG_HIDDEN);
        }
        
        // 切换到image_screen来显示图片
        lv_scr_load(image_screen);
        lv_refr_now(NULL);
        
        // 清理旧的album_win（如果存在）
        if (album_win) {
            lv_obj_del(album_win);
            album_win = NULL;
            img_display = NULL;
        }
        
        return;
    }
    
    // 如果没有图片文件，使用album_win（硬编码路径备用方案）
    // 如果窗口已存在且有效，直接显示
    if (album_win && lv_obj_is_valid(album_win)) {
        // 确保主屏幕隐藏
        extern lv_obj_t *main_screen;
        if (main_screen) {
            lv_obj_add_flag(main_screen, LV_OBJ_FLAG_HIDDEN);
        }
        lv_obj_clear_flag(album_win, LV_OBJ_FLAG_HIDDEN);
        lv_scr_load(album_win);
        lv_refr_now(NULL);
        return;
    }
    
    // 如果窗口存在但无效，清理它
    if (album_win) {
        lv_obj_del(album_win);
        album_win = NULL;
        img_display = NULL;
    }
    
    // 创建相册窗口
    // 创建独立的相册窗口（备用方案）
    album_win = lv_obj_create(NULL);
    lv_obj_set_size(album_win, 800, 480);
    lv_obj_set_style_bg_color(album_win, lv_color_white(), 0);

    // 使用硬编码路径（备用方案）
    img_display = lv_img_create(album_win);
    lv_img_set_src(img_display, image_paths[current_image]);
    lv_obj_align(img_display, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_size(img_display, 600, 400);

    // 创建"上一张"按钮
    lv_obj_t *prev_btn = lv_btn_create(album_win);
    lv_obj_set_size(prev_btn, 80, 50);
    lv_obj_align(prev_btn, LV_ALIGN_LEFT_MID, 20, 0);
    lv_obj_t *prev_label = lv_label_create(prev_btn);
    lv_label_set_text(prev_label, LV_SYMBOL_LEFT);
    lv_obj_center(prev_label);
    lv_obj_add_event_cb(prev_btn, prev_btn_event_handler, LV_EVENT_CLICKED, NULL);

    // 创建"下一张"按钮
    lv_obj_t *next_btn = lv_btn_create(album_win);
    lv_obj_set_size(next_btn, 80, 50);
    lv_obj_align(next_btn, LV_ALIGN_RIGHT_MID, -20, 0);
    lv_obj_t *next_label = lv_label_create(next_btn);
    lv_label_set_text(next_label, LV_SYMBOL_RIGHT);
    lv_obj_center(next_label);
    lv_obj_add_event_cb(next_btn, next_btn_event_handler, LV_EVENT_CLICKED, NULL);

    // 创建退出按钮
    lv_obj_t *exit_btn = lv_btn_create(album_win);
    lv_obj_set_size(exit_btn, 100, 50);
    lv_obj_set_style_bg_color(exit_btn, lv_color_hex(0x0000FF), 0);
    lv_obj_align(exit_btn, LV_ALIGN_TOP_RIGHT, -20, 20);
    lv_obj_move_foreground(exit_btn);  // 确保在最上层
    lv_obj_t *exit_label = lv_label_create(exit_btn);
    lv_label_set_text(exit_label, "返回");
    lv_obj_set_style_text_font(exit_label, &SourceHanSansSC_VF, 0);
    lv_obj_center(exit_label);
    lv_obj_add_event_cb(exit_btn, album_window_event_handler, LV_EVENT_CLICKED, NULL);
    
    // 隐藏主窗口
    extern lv_obj_t *main_screen;
    if (main_screen) {
        lv_obj_add_flag(main_screen, LV_OBJ_FLAG_HIDDEN);
    }
}

void album_window_event_handler(lv_event_t *e) {
    // 检查事件类型，确保是点击事件
    lv_event_code_t code = lv_event_get_code(e);
    if(code != LV_EVENT_CLICKED) {
        return;
    }
    
    // 清理album_win（如果存在）
    if (album_win) {
        lv_obj_del(album_win);
        album_win = NULL;
        img_display = NULL;
    }
    
    // 恢复主屏幕显示
    extern lv_obj_t* get_main_page1_screen(void);
    lv_obj_t *main_page_screen = get_main_page1_screen();
    if (main_page_screen) {
        lv_obj_clear_flag(main_page_screen, LV_OBJ_FLAG_HIDDEN);
        lv_scr_load(main_page_screen);
        lv_refr_now(NULL);  // 强制刷新显示
    }
}

