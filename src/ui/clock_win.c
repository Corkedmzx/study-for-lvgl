/**
 * @file clock_win.c
 * @brief 系统时钟窗口实现
 */

#include "clock_win.h"
#include "ui_screens.h"
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <pthread.h>
#include "lvgl/lvgl.h"
#include "lvgl/src/font/lv_font.h"
#include "lvgl/src/widgets/lv_canvas.h"
#include "lvgl/src/draw/lv_draw.h"

/* 声明SourceHanSansSC_VF字体 */
#if LV_FONT_SOURCE_HAN_SANS_SC_VF
extern const lv_font_t SourceHanSansSC_VF;
#endif

static lv_obj_t *clock_window = NULL;
static lv_obj_t *time_label = NULL;
static lv_obj_t *date_label = NULL;
static lv_obj_t *clock_canvas = NULL;
static pthread_t clock_thread;
static bool clock_running = false;
static pthread_mutex_t clock_mutex = PTHREAD_MUTEX_INITIALIZER;

#define CLOCK_SIZE 200
#define CLOCK_CENTER_X (CLOCK_SIZE / 2)
#define CLOCK_CENTER_Y (CLOCK_SIZE / 2)
#define CLOCK_RADIUS 90

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
    
    // 清空画布
    lv_canvas_fill_bg(clock_canvas, lv_color_hex(0xffffff), LV_OPA_COVER);
    
    // 绘制外圆
    lv_draw_arc_dsc_t arc_dsc;
    lv_draw_arc_dsc_init(&arc_dsc);
    arc_dsc.color = lv_color_hex(0x333333);
    arc_dsc.width = 3;
    lv_canvas_draw_arc(clock_canvas, CLOCK_CENTER_X, CLOCK_CENTER_Y, CLOCK_RADIUS, 0, 360, &arc_dsc);
    
    // 绘制刻度（12个主要刻度）
    for (int i = 0; i < 12; i++) {
        double angle = (i * 30 - 90) * M_PI / 180.0;
        int x1 = CLOCK_CENTER_X + (int)((CLOCK_RADIUS - 15) * cos(angle));
        int y1 = CLOCK_CENTER_Y + (int)((CLOCK_RADIUS - 15) * sin(angle));
        int x2 = CLOCK_CENTER_X + (int)(CLOCK_RADIUS * cos(angle));
        int y2 = CLOCK_CENTER_Y + (int)(CLOCK_RADIUS * sin(angle));
        
        lv_draw_line_dsc_t line_dsc;
        lv_draw_line_dsc_init(&line_dsc);
        line_dsc.color = lv_color_hex(0x333333);
        line_dsc.width = 3;
        lv_point_t points[2] = {{x1, y1}, {x2, y2}};
        lv_canvas_draw_line(clock_canvas, points, 2, &line_dsc);
    }
    
    // 绘制时针（从12点开始，顺时针）
    double hour_angle = (hour * 30 + minute * 0.5 - 90) * M_PI / 180.0;
    int hour_x = CLOCK_CENTER_X + (int)(CLOCK_RADIUS * 0.5 * cos(hour_angle));
    int hour_y = CLOCK_CENTER_Y + (int)(CLOCK_RADIUS * 0.5 * sin(hour_angle));
    lv_draw_line_dsc_t hour_line_dsc;
    lv_draw_line_dsc_init(&hour_line_dsc);
    hour_line_dsc.color = lv_color_hex(0x000000);
    hour_line_dsc.width = 4;
    lv_point_t hour_points[2] = {{CLOCK_CENTER_X, CLOCK_CENTER_Y}, {hour_x, hour_y}};
    lv_canvas_draw_line(clock_canvas, hour_points, 2, &hour_line_dsc);
    
    // 绘制分针
    double minute_angle = (minute * 6 - 90) * M_PI / 180.0;
    int minute_x = CLOCK_CENTER_X + (int)(CLOCK_RADIUS * 0.7 * cos(minute_angle));
    int minute_y = CLOCK_CENTER_Y + (int)(CLOCK_RADIUS * 0.7 * sin(minute_angle));
    lv_draw_line_dsc_t minute_line_dsc;
    lv_draw_line_dsc_init(&minute_line_dsc);
    minute_line_dsc.color = lv_color_hex(0x000000);
    minute_line_dsc.width = 3;
    lv_point_t minute_points[2] = {{CLOCK_CENTER_X, CLOCK_CENTER_Y}, {minute_x, minute_y}};
    lv_canvas_draw_line(clock_canvas, minute_points, 2, &minute_line_dsc);
    
    // 绘制秒针
    double second_angle = (second * 6 - 90) * M_PI / 180.0;
    int second_x = CLOCK_CENTER_X + (int)(CLOCK_RADIUS * 0.85 * cos(second_angle));
    int second_y = CLOCK_CENTER_Y + (int)(CLOCK_RADIUS * 0.85 * sin(second_angle));
    lv_draw_line_dsc_t second_line_dsc;
    lv_draw_line_dsc_init(&second_line_dsc);
    second_line_dsc.color = lv_color_hex(0xff0000);
    second_line_dsc.width = 2;
    lv_point_t second_points[2] = {{CLOCK_CENTER_X, CLOCK_CENTER_Y}, {second_x, second_y}};
    lv_canvas_draw_line(clock_canvas, second_points, 2, &second_line_dsc);
    
    // 绘制中心点
    lv_draw_rect_dsc_t center_dsc;
    lv_draw_rect_dsc_init(&center_dsc);
    center_dsc.bg_color = lv_color_hex(0x000000);
    center_dsc.bg_opa = LV_OPA_COVER;
    center_dsc.radius = LV_RADIUS_CIRCLE;
    lv_canvas_draw_rect(clock_canvas, CLOCK_CENTER_X - 5, CLOCK_CENTER_Y - 5, 10, 10, &center_dsc);
}

/**
 * @brief 更新时钟显示
 */
static void update_clock_display(void) {
    time_t rawtime;
    struct tm *timeinfo;
    char time_str[64];
    char date_str[64];
    
    time(&rawtime);
    timeinfo = localtime(&rawtime);
    
    // 格式化时间：HH:MM:SS
    strftime(time_str, sizeof(time_str), "%H:%M:%S", timeinfo);
    
    // 格式化日期：YYYY年MM月DD日 星期X
    strftime(date_str, sizeof(date_str), "%Y年%m月%d日", timeinfo);
    
    // 添加星期
    const char *weekdays[] = {"日", "一", "二", "三", "四", "五", "六"};
    char weekday_str[32];
    snprintf(weekday_str, sizeof(weekday_str), " 星期%s", weekdays[timeinfo->tm_wday]);
    strcat(date_str, weekday_str);
    
    pthread_mutex_lock(&clock_mutex);
    if (time_label) {
        lv_label_set_text(time_label, time_str);
    }
    if (date_label) {
        lv_label_set_text(date_label, date_str);
    }
    if (clock_canvas) {
        draw_clock_face();
    }
    pthread_mutex_unlock(&clock_mutex);
}

/**
 * @brief 时钟更新线程
 */
static void *clock_thread_func(void *arg) {
    (void)arg;
    
    while (clock_running) {
        update_clock_display();
        sleep(1);  // 每秒更新一次
    }
    
    return NULL;
}

/**
 * @brief 返回按钮事件处理
 */
static void back_btn_event_handler(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    
    clock_win_hide();
    
    // 切换到主屏幕
    extern lv_obj_t* get_main_page1_screen(void);
    lv_obj_t *main_page_screen = get_main_page1_screen();
    if (main_page_screen) {
        lv_obj_clear_flag(main_page_screen, LV_OBJ_FLAG_HIDDEN);
        lv_scr_load(main_page_screen);
        lv_refr_now(NULL);
    }
}

/**
 * @brief 显示时钟窗口
 */
void clock_win_show(void) {
    if (clock_window == NULL) {
        // 创建时钟窗口
        clock_window = lv_obj_create(NULL);
        lv_obj_set_style_bg_color(clock_window, lv_color_hex(0xf0f0f0), 0);
        lv_obj_set_size(clock_window, LV_HOR_RES, LV_VER_RES);
        
        // 创建标题
        lv_obj_t *title = lv_label_create(clock_window);
        lv_label_set_text(title, "系统时钟");
        lv_obj_set_style_text_font(title, &SourceHanSansSC_VF, 0);
        lv_obj_set_style_text_color(title, lv_color_hex(0x1a1a1a), 0);
        lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 20);
        
        // 创建钟表画布
        static lv_color_t clock_buf[LV_CANVAS_BUF_SIZE_TRUE_COLOR(CLOCK_SIZE, CLOCK_SIZE)];
        clock_canvas = lv_canvas_create(clock_window);
        lv_canvas_set_buffer(clock_canvas, clock_buf, CLOCK_SIZE, CLOCK_SIZE, LV_IMG_CF_TRUE_COLOR);
        lv_obj_align(clock_canvas, LV_ALIGN_CENTER, 0, -60);
        
        // 创建时间显示标签（大字体）
        time_label = lv_label_create(clock_window);
        lv_label_set_text(time_label, "00:00:00");
        lv_obj_set_style_text_font(time_label, &SourceHanSansSC_VF, 0);
        lv_obj_set_style_text_color(time_label, lv_color_hex(0x1a1a1a), 0);
        lv_obj_set_style_text_align(time_label, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_align(time_label, LV_ALIGN_CENTER, 0, 80);
        
        // 创建日期显示标签
        date_label = lv_label_create(clock_window);
        lv_label_set_text(date_label, "2025年01月01日 星期一");
        lv_obj_set_style_text_font(date_label, &SourceHanSansSC_VF, 0);
        lv_obj_set_style_text_color(date_label, lv_color_hex(0x666666), 0);
        lv_obj_set_style_text_align(date_label, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_align(date_label, LV_ALIGN_CENTER, 0, 120);
        
        // 创建返回按钮
        lv_obj_t *back_btn = lv_btn_create(clock_window);
        lv_obj_set_size(back_btn, 150, 60);
        lv_obj_align(back_btn, LV_ALIGN_BOTTOM_MID, 0, -30);
        lv_obj_set_style_bg_color(back_btn, lv_color_hex(0x9E9E9E), 0);
        lv_obj_t *back_label = lv_label_create(back_btn);
        lv_label_set_text(back_label, "返回");
        lv_obj_set_style_text_font(back_label, &SourceHanSansSC_VF, 0);
        lv_obj_center(back_label);
        lv_obj_add_event_cb(back_btn, back_btn_event_handler, LV_EVENT_CLICKED, NULL);
    } else {
        lv_obj_clear_flag(clock_window, LV_OBJ_FLAG_HIDDEN);
    }
    
    // 立即更新一次显示
    update_clock_display();
    
    // 启动时钟更新线程
    if (!clock_running) {
        clock_running = true;
        pthread_create(&clock_thread, NULL, clock_thread_func, NULL);
    }
    
    // 切换到时钟窗口
    lv_scr_load(clock_window);
    lv_refr_now(NULL);
}

/**
 * @brief 隐藏时钟窗口
 */
void clock_win_hide(void) {
    // 停止时钟更新线程
    if (clock_running) {
        clock_running = false;
        pthread_join(clock_thread, NULL);
    }
    
    if (clock_window) {
        lv_obj_add_flag(clock_window, LV_OBJ_FLAG_HIDDEN);
    }
}

