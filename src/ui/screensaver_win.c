/**
 * @file screensaver_win.c
 * @brief 屏保窗口实现
 */

#include "screensaver_win.h"
#include "ui_screens.h"
#include "login_win.h"
#include "../image_viewer/image_viewer.h"
#include "../common/touch_device.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <sys/time.h>
#include "lvgl/lvgl.h"
#include "lvgl/src/font/lv_font.h"
#include "lvgl/src/widgets/lv_canvas.h"
#include "lvgl/src/draw/lv_draw.h"

/* 声明SourceHanSansSC_VF字体 */
#if LV_FONT_SOURCE_HAN_SANS_SC_VF
extern const lv_font_t SourceHanSansSC_VF;
#endif

/* 背景图路径 */
#define SCREENSAVER_BG_IMAGE "/mdata/open.BMP"

/* 钟表参数 */
#define CLOCK_SIZE 220  // 从150增加到220，使钟表更大
#define CLOCK_CENTER_X (CLOCK_SIZE / 2)
#define CLOCK_CENTER_Y (CLOCK_SIZE / 2)
#define CLOCK_RADIUS 95  // 从65增加到95，使钟表更大

/* 滑动检测参数 */
#define SWIPE_THRESHOLD 100  // 最小滑动距离（像素）
#define SWIPE_TIME_THRESHOLD 500000  // 最大滑动时间（微秒，500ms）

static lv_obj_t *screensaver_window = NULL;
static lv_obj_t *bg_canvas = NULL;
static lv_obj_t *clock_canvas = NULL;
static lv_obj_t *time_label = NULL;
static lv_obj_t *weekday_label = NULL;
static lv_obj_t *hint_label = NULL;
static bool is_unlocked = false;
static bool need_show_login = false;  // 标志位：需要在主循环中显示密码锁
static lv_timer_t *clock_timer = NULL;

/* 触摸检测相关 */
static bool touch_pressed = false;
static int touch_start_x = 0;
static int touch_start_y = 0;
static struct timeval swipe_start_time;
static struct timeval swipe_end_time;

/**
 * @brief 绘制钟表
 */
static void draw_clock_face(void) {
    if (!clock_canvas) {
        return;
    }
    
    time_t rawtime;
    struct tm *timeinfo;
    time(&rawtime);
    timeinfo = localtime(&rawtime);
    
    int hour = timeinfo->tm_hour % 12;
    int minute = timeinfo->tm_min;
    int second = timeinfo->tm_sec;
    
    // 清空画布（透明背景）
    lv_canvas_fill_bg(clock_canvas, lv_color_hex(0x000000), LV_OPA_TRANSP);
    
    // 绘制外圆（白色，加粗）
    lv_draw_arc_dsc_t arc_dsc;
    lv_draw_arc_dsc_init(&arc_dsc);
    arc_dsc.color = lv_color_hex(0xFFFFFF);
    arc_dsc.width = 5;  // 从3增加到5，使外圆更粗
    lv_canvas_draw_arc(clock_canvas, CLOCK_CENTER_X, CLOCK_CENTER_Y, CLOCK_RADIUS, 0, 360, &arc_dsc);
    
    // 绘制刻度（12个主要刻度，加粗加长）
    for (int i = 0; i < 12; i++) {
        double angle = (i * 30 - 90) * M_PI / 180.0;
        int x1 = CLOCK_CENTER_X + (int)((CLOCK_RADIUS - 15) * cos(angle));  // 从10增加到15，使刻度更长
        int y1 = CLOCK_CENTER_Y + (int)((CLOCK_RADIUS - 15) * sin(angle));
        int x2 = CLOCK_CENTER_X + (int)(CLOCK_RADIUS * cos(angle));
        int y2 = CLOCK_CENTER_Y + (int)(CLOCK_RADIUS * sin(angle));
        
        lv_draw_line_dsc_t line_dsc;
        lv_draw_line_dsc_init(&line_dsc);
        line_dsc.color = lv_color_hex(0xFFFFFF);
        line_dsc.width = 4;  // 从2增加到4，使刻度更粗
        lv_point_t points[2] = {{x1, y1}, {x2, y2}};
        lv_canvas_draw_line(clock_canvas, points, 2, &line_dsc);
    }
    
    // 绘制时针（加粗）
    double hour_angle = (hour * 30 + minute * 0.5 - 90) * M_PI / 180.0;
    int hour_x = CLOCK_CENTER_X + (int)(CLOCK_RADIUS * 0.5 * cos(hour_angle));
    int hour_y = CLOCK_CENTER_Y + (int)(CLOCK_RADIUS * 0.5 * sin(hour_angle));
    lv_draw_line_dsc_t hour_line_dsc;
    lv_draw_line_dsc_init(&hour_line_dsc);
    hour_line_dsc.color = lv_color_hex(0xFFFFFF);
    hour_line_dsc.width = 6;  // 从3增加到6，使时针更粗
    lv_point_t hour_points[2] = {{CLOCK_CENTER_X, CLOCK_CENTER_Y}, {hour_x, hour_y}};
    lv_canvas_draw_line(clock_canvas, hour_points, 2, &hour_line_dsc);
    
    // 绘制分针（加粗）
    double minute_angle = (minute * 6 - 90) * M_PI / 180.0;
    int minute_x = CLOCK_CENTER_X + (int)(CLOCK_RADIUS * 0.7 * cos(minute_angle));
    int minute_y = CLOCK_CENTER_Y + (int)(CLOCK_RADIUS * 0.7 * sin(minute_angle));
    lv_draw_line_dsc_t minute_line_dsc;
    lv_draw_line_dsc_init(&minute_line_dsc);
    minute_line_dsc.color = lv_color_hex(0xFFFFFF);
    minute_line_dsc.width = 4;  // 从2增加到4，使分针更粗
    lv_point_t minute_points[2] = {{CLOCK_CENTER_X, CLOCK_CENTER_Y}, {minute_x, minute_y}};
    lv_canvas_draw_line(clock_canvas, minute_points, 2, &minute_line_dsc);
    
    // 绘制秒针（加粗）
    double second_angle = (second * 6 - 90) * M_PI / 180.0;
    int second_x = CLOCK_CENTER_X + (int)(CLOCK_RADIUS * 0.85 * cos(second_angle));
    int second_y = CLOCK_CENTER_Y + (int)(CLOCK_RADIUS * 0.85 * sin(second_angle));
    lv_draw_line_dsc_t second_line_dsc;
    lv_draw_line_dsc_init(&second_line_dsc);
    second_line_dsc.color = lv_color_hex(0xFF0000);
    second_line_dsc.width = 3;  // 从2增加到3，使秒针更粗
    lv_point_t second_points[2] = {{CLOCK_CENTER_X, CLOCK_CENTER_Y}, {second_x, second_y}};
    lv_canvas_draw_line(clock_canvas, second_points, 2, &second_line_dsc);
    
    // 绘制中心点（增大）
    lv_draw_rect_dsc_t center_dsc;
    lv_draw_rect_dsc_init(&center_dsc);
    center_dsc.bg_color = lv_color_hex(0xFFFFFF);
    center_dsc.bg_opa = LV_OPA_COVER;
    center_dsc.radius = LV_RADIUS_CIRCLE;
    lv_canvas_draw_rect(clock_canvas, CLOCK_CENTER_X - 6, CLOCK_CENTER_Y - 6, 12, 12, &center_dsc);  // 从8x8增加到12x12
}

/**
 * @brief 时钟更新定时器回调（在主线程中执行，线程安全）
 */
static void clock_timer_cb(lv_timer_t *timer) {
    (void)timer;
    
    time_t rawtime;
    struct tm *timeinfo;
    char time_str[64];
    char weekday_str[32];
    
    time(&rawtime);
    timeinfo = localtime(&rawtime);
    
    // 格式化时间：HH:MM
    strftime(time_str, sizeof(time_str), "%H:%M", timeinfo);
    
    // 格式化星期
    const char *weekdays[] = {"星期日", "星期一", "星期二", "星期三", "星期四", "星期五", "星期六"};
    snprintf(weekday_str, sizeof(weekday_str), "%s", weekdays[timeinfo->tm_wday]);
    
    if (time_label) {
        lv_label_set_text(time_label, time_str);
    }
    if (weekday_label) {
        lv_label_set_text(weekday_label, weekday_str);
    }
    if (clock_canvas) {
        draw_clock_face();
    }
}

/**
 * @brief 屏保触摸事件处理（使用LVGL事件系统）
 */
static void screensaver_touch_event_handler(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_indev_t *indev = lv_indev_get_act();
    
    if (!indev) {
        return;
    }
    
    lv_point_t point;
    lv_indev_get_point(indev, &point);
    
    if (code == LV_EVENT_PRESSED) {
        // 按下
        touch_pressed = true;
        touch_start_x = point.x;
        touch_start_y = point.y;
        gettimeofday(&swipe_start_time, NULL);
    } else if (code == LV_EVENT_RELEASED) {
        // 释放
        if (touch_pressed) {
            int touch_end_x = point.x;
            int touch_end_y = point.y;
            gettimeofday(&swipe_end_time, NULL);
            
            // 计算滑动距离和时间
            int dx = touch_end_x - touch_start_x;
            int dy = touch_end_y - touch_start_y;
            long duration = (swipe_end_time.tv_sec - swipe_start_time.tv_sec) * 1000000 + 
                           (swipe_end_time.tv_usec - swipe_start_time.tv_usec);
            
            // 计算总滑动距离
            float distance = sqrt(dx * dx + dy * dy);
            
            // 检测向上滑动
            if (distance >= SWIPE_THRESHOLD && duration <= SWIPE_TIME_THRESHOLD) {
                int abs_dx = abs(dx);
                int abs_dy = abs(dy);
                
                // 判断是否为向上滑动（垂直方向为主，且向上）
                if (abs_dy > abs_dx && dy < -SWIPE_THRESHOLD) {
                    printf("[屏保] 检测到向上滑动，解锁\n");
                    is_unlocked = true;
                }
            }
            
            touch_pressed = false;
        }
    }
}

/**
 * @brief 滑动动画回调（向上滑动屏保窗口）
 */
static void swipe_anim_cb(void *var, int32_t value) {
    lv_obj_t *obj = (lv_obj_t *)var;
    lv_obj_set_y(obj, value);
}

/**
 * @brief 滑动动画完成回调
 */
static void swipe_anim_completed_cb(lv_anim_t *a) {
    (void)a;
    // 动画完成后，隐藏屏保窗口
    if (screensaver_window) {
        lv_obj_add_flag(screensaver_window, LV_OBJ_FLAG_HIDDEN);
    }
    
    // 设置标志位，在主循环中显示密码锁（避免在动画回调中直接切换屏幕）
    need_show_login = true;
}

/**
 * @brief 显示屏保窗口
 */
void screensaver_win_show(void) {
    is_unlocked = false;
    
    // 创建屏保窗口
    if (screensaver_window == NULL) {
        screensaver_window = lv_obj_create(NULL);
        lv_obj_set_size(screensaver_window, LV_HOR_RES, LV_VER_RES);
        lv_obj_set_style_bg_opa(screensaver_window, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_opa(screensaver_window, LV_OPA_TRANSP, 0);
        lv_obj_clear_flag(screensaver_window, LV_OBJ_FLAG_SCROLLABLE);
        
        // 创建背景图canvas（全屏）
        // 使用固定大小避免编译错误（800x480，32位色深）
        #define SCREENSAVER_BG_WIDTH 800
        #define SCREENSAVER_BG_HEIGHT 480
        static lv_color_t bg_buf[LV_CANVAS_BUF_SIZE_TRUE_COLOR(SCREENSAVER_BG_WIDTH, SCREENSAVER_BG_HEIGHT)];
        bg_canvas = lv_canvas_create(screensaver_window);
        lv_canvas_set_buffer(bg_canvas, bg_buf, SCREENSAVER_BG_WIDTH, SCREENSAVER_BG_HEIGHT, LV_IMG_CF_TRUE_COLOR);
        lv_obj_align(bg_canvas, LV_ALIGN_TOP_LEFT, 0, 0);
        
        // 加载背景图
        if (load_bmp_to_canvas(bg_canvas, SCREENSAVER_BG_IMAGE) != 0) {
            printf("[屏保] 背景图加载失败，使用黑色背景\n");
            lv_canvas_fill_bg(bg_canvas, lv_color_hex(0x000000), LV_OPA_COVER);
        } else {
            printf("[屏保] 背景图加载成功\n");
        }
        
        // 创建钟表画布
        static lv_color_t clock_buf[LV_CANVAS_BUF_SIZE_TRUE_COLOR(CLOCK_SIZE, CLOCK_SIZE)];
        clock_canvas = lv_canvas_create(screensaver_window);
        lv_canvas_set_buffer(clock_canvas, clock_buf, CLOCK_SIZE, CLOCK_SIZE, LV_IMG_CF_TRUE_COLOR_ALPHA);
        lv_obj_align(clock_canvas, LV_ALIGN_CENTER, 0, -80);
        
        // 创建时间标签
        time_label = lv_label_create(screensaver_window);
        lv_label_set_text(time_label, "00:00");
        lv_obj_set_style_text_font(time_label, &SourceHanSansSC_VF, 0);
        lv_obj_set_style_text_color(time_label, lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_text_align(time_label, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_align(time_label, LV_ALIGN_CENTER, 0, 20);
        
        // 创建星期标签
        weekday_label = lv_label_create(screensaver_window);
        lv_label_set_text(weekday_label, "星期一");
        lv_obj_set_style_text_font(weekday_label, &SourceHanSansSC_VF, 0);
        lv_obj_set_style_text_color(weekday_label, lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_text_align(weekday_label, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_align(weekday_label, LV_ALIGN_CENTER, 0, 60);
        
        // 创建提示标签（向上滑动解锁）
        hint_label = lv_label_create(screensaver_window);
        lv_label_set_text(hint_label, "↑ 向上滑动解锁");
        lv_obj_set_style_text_font(hint_label, &SourceHanSansSC_VF, 0);
        lv_obj_set_style_text_color(hint_label, lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_text_align(hint_label, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_align(hint_label, LV_ALIGN_BOTTOM_MID, 0, -40);
        
        // 初始化时钟显示
        draw_clock_face();
    } else {
        lv_obj_clear_flag(screensaver_window, LV_OBJ_FLAG_HIDDEN);
    }
    
    // 添加触摸事件处理
    lv_obj_add_event_cb(screensaver_window, screensaver_touch_event_handler, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(screensaver_window, screensaver_touch_event_handler, LV_EVENT_RELEASED, NULL);
    
    // 启动时钟更新定时器（在主线程中执行，线程安全）
    if (!clock_timer) {
        clock_timer = lv_timer_create(clock_timer_cb, 1000, NULL);  // 每秒更新一次
        lv_timer_set_repeat_count(clock_timer, LV_ANIM_REPEAT_INFINITE);  // 无限重复
    }
    
    // 切换到屏保窗口
    lv_scr_load(screensaver_window);
    lv_refr_now(NULL);
}

/**
 * @brief 检查并处理解锁（在主循环中调用）
 */
void screensaver_win_check_unlock(void) {
    // 检查是否需要显示密码锁（动画完成后）
    if (need_show_login) {
        need_show_login = false;
        printf("[屏保] 准备显示密码锁窗口\n");
        // 在主循环中安全地显示密码锁
        login_win_show();
        printf("[屏保] 密码锁窗口显示完成\n");
        return;
    }
    
    // 检查是否检测到解锁手势
    if (!is_unlocked) {
        return;
    }
    
    // 重置解锁标志，避免重复处理
    is_unlocked = false;
    
    // 创建向上滑动动画
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, screensaver_window);
    lv_anim_set_values(&a, 0, -480);  // 使用固定值避免宏问题
    lv_anim_set_time(&a, 300);  // 300ms动画
    lv_anim_set_exec_cb(&a, swipe_anim_cb);
    lv_anim_set_ready_cb(&a, swipe_anim_completed_cb);
    lv_anim_start(&a);
}

/**
 * @brief 隐藏屏保窗口
 */
void screensaver_win_hide(void) {
    // 停止时钟更新定时器
    if (clock_timer) {
        lv_timer_del(clock_timer);
        clock_timer = NULL;
    }
    
    if (screensaver_window) {
        lv_obj_add_flag(screensaver_window, LV_OBJ_FLAG_HIDDEN);
    }
}

/**
 * @brief 检查屏保是否已解锁
 */
bool screensaver_win_is_unlocked(void) {
    return is_unlocked;
}

