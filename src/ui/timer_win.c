/**
 * @file timer_win.c
 * @brief 计时器窗口实现
 */

#include "timer_win.h"
#include "ui_screens.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <pthread.h>
#include <time.h>
#include "lvgl/lvgl.h"
#include "lvgl/src/font/lv_font.h"
#include "lvgl/src/widgets/lv_canvas.h"
#include "lvgl/src/draw/lv_draw.h"
#include "lvgl/src/font/lv_symbol_def.h"

/* 声明FontAwesome字体 */
extern const lv_font_t fa_solid_24;

/* 自定义符号定义 */
#define CUSTOM_SYMBOL_VOLUME_MAX "\xEF\x80\xA8"  // FontAwesome volume-max icon (U+F028) - 圆形喇叭

/* 声明SourceHanSansSC_VF字体 */
#if LV_FONT_SOURCE_HAN_SANS_SC_VF
extern const lv_font_t SourceHanSansSC_VF;
#endif

/* 蜂鸣器设备 */
#define BUZZER_DEVICE "/dev/buzz_misc"
#define BUZZ_ON  _IOW('b', 1, unsigned long)
#define BUZZ_OFF _IOW('b', 0, unsigned long)

/* LED设备 */
#define LED_DEVICE "/dev/leds_misc"
#define LED1 _IOW('l', 0, unsigned long)
#define LED_ON  0
#define LED_OFF 1

lv_obj_t *timer_window = NULL;  // 全局变量，供其他模块访问
static lv_obj_t *time_label = NULL;
static lv_obj_t *clock_canvas = NULL;
static lv_obj_t *start_btn = NULL;
static lv_obj_t *stop_btn = NULL;
static lv_obj_t *reset_btn = NULL;

#define CLOCK_SIZE 200
#define CLOCK_CENTER_X (CLOCK_SIZE / 2)
#define CLOCK_CENTER_Y (CLOCK_SIZE / 2)
#define CLOCK_RADIUS 90

static int buzzer_fd = -1;
static int led_fd = -1;
static bool is_running = false;
static int elapsed_seconds = 0;
static pthread_t timer_thread = 0;
static pthread_mutex_t timer_mutex = PTHREAD_MUTEX_INITIALIZER;
static bool buzzer_enabled = true;  // 蜂鸣器开关状态，默认开启
static lv_obj_t *buzzer_btn = NULL;  // 蜂鸣器控制按钮

/**
 * @brief 触发蜂鸣器（短响）
 */
static void beep_once(void) {
    if (buzzer_enabled && buzzer_fd >= 0) {
        ioctl(buzzer_fd, BUZZ_ON);
        usleep(100000);  // 100ms
        ioctl(buzzer_fd, BUZZ_OFF);
    }
}

/**
 * @brief 加载蜂鸣器驱动
 */
static bool load_buzzer_driver(void) {
    if (access(BUZZER_DEVICE, F_OK) == 0) {
        return true;
    }

    const char *driver_paths[] = {
        "",
        "./",
        "/mnt/udisk/",
        "/bin/",
        "/usr/lib/modules/",
        NULL
    };

    bool buzzer_loaded = false;
    for (int i = 0; driver_paths[i] != NULL; i++) {
        char cmd[256];
        snprintf(cmd, sizeof(cmd), "insmod %s%s", driver_paths[i], "buzz_misc.ko");
        if (system(cmd) == 0) {
            buzzer_loaded = true;
            break;
        }
    }

    if (access(BUZZER_DEVICE, F_OK) != 0) {
        char cmd[256];
        snprintf(cmd, sizeof(cmd), "mknod %s c 251 0", BUZZER_DEVICE);
        (void)system(cmd);
    }
    
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "chmod 666 %s", BUZZER_DEVICE);
    (void)system(cmd);
    
    return buzzer_loaded;
}

/**
 * @brief 加载LED驱动
 */
static bool load_led_driver(void) {
    if (access(LED_DEVICE, F_OK) == 0) {
        return true;
    }

    const char *driver_paths[] = {
        "",
        "./",
        "/mnt/udisk/",
        "/bin/",
        "/usr/lib/modules/",
        NULL
    };

    bool led_loaded = false;
    for (int i = 0; driver_paths[i] != NULL; i++) {
        char cmd[256];
        snprintf(cmd, sizeof(cmd), "insmod %s%s", driver_paths[i], "leds_misc.ko");
        if (system(cmd) == 0) {
            led_loaded = true;
            break;
        }
    }

    if (access(LED_DEVICE, F_OK) != 0) {
        char cmd[256];
        snprintf(cmd, sizeof(cmd), "mknod %s c 250 0", LED_DEVICE);
        (void)system(cmd);
    }
    
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "chmod 666 %s", LED_DEVICE);
    (void)system(cmd);
    
    return led_loaded;
}

/**
 * @brief 初始化蜂鸣器
 */
static void init_buzzer(void) {
    load_buzzer_driver();
    buzzer_fd = open(BUZZER_DEVICE, O_RDWR);
}

/**
 * @brief 初始化LED
 */
static void init_led(void) {
    load_led_driver();
    led_fd = open(LED_DEVICE, O_RDWR);
    if (led_fd < 0) {
        char cmd[256];
        snprintf(cmd, sizeof(cmd), "mknod %s c 250 0", LED_DEVICE);
        (void)system(cmd);
        led_fd = open(LED_DEVICE, O_RDWR);
    }
}

/**
 * @brief LED闪烁（单次）
 */
static void led_flash_once(void) {
    if (led_fd < 0) {
        init_led();
        if (led_fd < 0) {
            return;
        }
    }
    
    ioctl(led_fd, LED1, (unsigned long)LED_ON);
    usleep(50000);  // 50ms
    ioctl(led_fd, LED1, (unsigned long)LED_OFF);
}

/**
 * @brief 绘制计时器钟表
 */
static void draw_timer_clock_face(void) {
    if (!clock_canvas) {
        return;
    }
    
    int hours = elapsed_seconds / 3600;
    int minutes = (elapsed_seconds % 3600) / 60;
    int seconds = elapsed_seconds % 60;
    
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
    
    // 绘制时针（基于小时，但计时器通常只显示分钟和秒）
    double hour_angle = (hours % 12 * 30 + minutes * 0.5 - 90) * M_PI / 180.0;
    int hour_x = CLOCK_CENTER_X + (int)(CLOCK_RADIUS * 0.5 * cos(hour_angle));
    int hour_y = CLOCK_CENTER_Y + (int)(CLOCK_RADIUS * 0.5 * sin(hour_angle));
    lv_draw_line_dsc_t hour_line_dsc;
    lv_draw_line_dsc_init(&hour_line_dsc);
    hour_line_dsc.color = lv_color_hex(0x000000);
    hour_line_dsc.width = 4;
    lv_point_t hour_points[2] = {{CLOCK_CENTER_X, CLOCK_CENTER_Y}, {hour_x, hour_y}};
    lv_canvas_draw_line(clock_canvas, hour_points, 2, &hour_line_dsc);
    
    // 绘制分针
    double minute_angle = (minutes * 6 - 90) * M_PI / 180.0;
    int minute_x = CLOCK_CENTER_X + (int)(CLOCK_RADIUS * 0.7 * cos(minute_angle));
    int minute_y = CLOCK_CENTER_Y + (int)(CLOCK_RADIUS * 0.7 * sin(minute_angle));
    lv_draw_line_dsc_t minute_line_dsc;
    lv_draw_line_dsc_init(&minute_line_dsc);
    minute_line_dsc.color = lv_color_hex(0x000000);
    minute_line_dsc.width = 3;
    lv_point_t minute_points[2] = {{CLOCK_CENTER_X, CLOCK_CENTER_Y}, {minute_x, minute_y}};
    lv_canvas_draw_line(clock_canvas, minute_points, 2, &minute_line_dsc);
    
    // 绘制秒针
    double second_angle = (seconds * 6 - 90) * M_PI / 180.0;
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
 * @brief 更新时间显示
 */
static void update_time_display(void) {
    if (!time_label) return;
    
    int hours = elapsed_seconds / 3600;
    int minutes = (elapsed_seconds % 3600) / 60;
    int seconds = elapsed_seconds % 60;
    
    char time_str[32];
    if (hours > 0) {
        snprintf(time_str, sizeof(time_str), "%02d:%02d:%02d", hours, minutes, seconds);
    } else {
        snprintf(time_str, sizeof(time_str), "%02d:%02d", minutes, seconds);
    }
    
    lv_label_set_text(time_label, time_str);
    
    // 更新钟表显示
    if (clock_canvas) {
        draw_timer_clock_face();
    }
}

/**
 * @brief 计时器线程函数
 */
static void *timer_thread_func(void *arg) {
    (void)arg;
    
    while (1) {
        pthread_mutex_lock(&timer_mutex);
        bool running = is_running;
        pthread_mutex_unlock(&timer_mutex);
        
        if (!running) {
            break;
        }
        
        // 等待1秒
        sleep(1);
        
        pthread_mutex_lock(&timer_mutex);
        if (is_running) {
            elapsed_seconds++;
            
            // 更新显示
            update_time_display();
            
            // LED闪烁
            led_flash_once();
            
            // 蜂鸣器响（如果启用）
            beep_once();
        }
        pthread_mutex_unlock(&timer_mutex);
    }
    
    return NULL;
}

/**
 * @brief 开始按钮事件处理
 */
static void start_btn_event_handler(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    
    pthread_mutex_lock(&timer_mutex);
    if (!is_running) {
        is_running = true;
        
        // 创建计时器线程
        if (timer_thread == 0) {
            pthread_create(&timer_thread, NULL, timer_thread_func, NULL);
        }
        
        // 更新按钮状态
        if (start_btn) {
            lv_obj_add_flag(start_btn, LV_OBJ_FLAG_HIDDEN);
        }
        if (stop_btn) {
            lv_obj_clear_flag(stop_btn, LV_OBJ_FLAG_HIDDEN);
        }
    }
    pthread_mutex_unlock(&timer_mutex);
}

/**
 * @brief 停止按钮事件处理
 */
static void stop_btn_event_handler(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    
    pthread_mutex_lock(&timer_mutex);
    if (is_running) {
        is_running = false;
        
        // 等待线程结束
        if (timer_thread != 0) {
            pthread_mutex_unlock(&timer_mutex);
            pthread_join(timer_thread, NULL);
            pthread_mutex_lock(&timer_mutex);
            timer_thread = 0;
        }
        
        // 更新按钮状态
        if (stop_btn) {
            lv_obj_add_flag(stop_btn, LV_OBJ_FLAG_HIDDEN);
        }
        if (start_btn) {
            lv_obj_clear_flag(start_btn, LV_OBJ_FLAG_HIDDEN);
        }
    }
    pthread_mutex_unlock(&timer_mutex);
}

/**
 * @brief 重置按钮事件处理
 */
static void reset_btn_event_handler(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    
    pthread_mutex_lock(&timer_mutex);
    
    // 如果正在运行，先停止
    if (is_running) {
        is_running = false;
        if (timer_thread != 0) {
            pthread_mutex_unlock(&timer_mutex);
            pthread_join(timer_thread, NULL);
            pthread_mutex_lock(&timer_mutex);
            timer_thread = 0;
        }
    }
    
    // 重置时间
    elapsed_seconds = 0;
    update_time_display();
    
    // 更新按钮状态
    if (stop_btn) {
        lv_obj_add_flag(stop_btn, LV_OBJ_FLAG_HIDDEN);
    }
    if (start_btn) {
        lv_obj_clear_flag(start_btn, LV_OBJ_FLAG_HIDDEN);
    }
    
    pthread_mutex_unlock(&timer_mutex);
}

/**
 * @brief 蜂鸣器控制按钮事件处理
 */
static void buzzer_btn_event_handler(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    
    // 切换蜂鸣器开关状态
    buzzer_enabled = !buzzer_enabled;
    
    // 更新按钮样式和图标
    if (buzzer_btn) {
        lv_obj_t *icon_label = lv_obj_get_child(buzzer_btn, 0);
        if (icon_label) {
            lv_label_set_text(icon_label, CUSTOM_SYMBOL_VOLUME_MAX);
            // 根据开关状态设置颜色：开启=蓝色，关闭=灰色
            if (buzzer_enabled) {
                lv_obj_set_style_bg_color(buzzer_btn, lv_color_hex(0x2196F3), 0);
                lv_obj_set_style_text_color(icon_label, lv_color_hex(0xFFFFFF), 0);
            } else {
                lv_obj_set_style_bg_color(buzzer_btn, lv_color_hex(0x9E9E9E), 0);
                lv_obj_set_style_text_color(icon_label, lv_color_hex(0xFFFFFF), 0);
            }
        }
    }
}

/**
 * @brief 返回按钮事件处理
 */
static void back_btn_event_handler(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    
    // 停止计时器
    pthread_mutex_lock(&timer_mutex);
    if (is_running) {
        is_running = false;
        if (timer_thread != 0) {
            pthread_mutex_unlock(&timer_mutex);
            pthread_join(timer_thread, NULL);
            pthread_mutex_lock(&timer_mutex);
            timer_thread = 0;
        }
    }
    pthread_mutex_unlock(&timer_mutex);
    
    // 关闭设备
    if (buzzer_fd >= 0) {
        close(buzzer_fd);
        buzzer_fd = -1;
    }
    if (led_fd >= 0) {
        close(led_fd);
        led_fd = -1;
    }
    
    // 隐藏计时器窗口
    if (timer_window) {
        lv_obj_add_flag(timer_window, LV_OBJ_FLAG_HIDDEN);
    }
    
    // 显示主屏幕
    extern lv_obj_t *main_screen;
    if (main_screen) {
        lv_obj_clear_flag(main_screen, LV_OBJ_FLAG_HIDDEN);
        lv_scr_load(main_screen);
        lv_refr_now(NULL);
    }
}

/**
 * @brief 显示计时器窗口
 */
void timer_win_show(void) {
    // 初始化设备
    if (buzzer_fd < 0) {
        init_buzzer();
    }
    if (led_fd < 0) {
        init_led();
    }
    
    // 创建或显示计时器窗口
    if (timer_window == NULL) {
        timer_window = lv_obj_create(NULL);
        lv_obj_set_size(timer_window, 800, 480);
        lv_obj_set_style_bg_color(timer_window, lv_color_white(), 0);
        lv_obj_clear_flag(timer_window, LV_OBJ_FLAG_SCROLLABLE);
    } else {
        lv_obj_clean(timer_window);
        lv_obj_clear_flag(timer_window, LV_OBJ_FLAG_HIDDEN);
    }
    
    // 标题
    lv_obj_t *title = lv_label_create(timer_window);
    lv_label_set_text(title, "计时器");
    lv_obj_set_style_text_font(title, &SourceHanSansSC_VF, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 30);
    
    // 时间显示
    // 创建钟表画布
    static lv_color_t clock_buf[LV_CANVAS_BUF_SIZE_TRUE_COLOR(CLOCK_SIZE, CLOCK_SIZE)];
    clock_canvas = lv_canvas_create(timer_window);
    lv_canvas_set_buffer(clock_canvas, clock_buf, CLOCK_SIZE, CLOCK_SIZE, LV_IMG_CF_TRUE_COLOR);
    lv_obj_align(clock_canvas, LV_ALIGN_CENTER, 0, -80);
    
    time_label = lv_label_create(timer_window);
    lv_label_set_text(time_label, "00:00");
    lv_obj_set_style_text_font(time_label, &SourceHanSansSC_VF, 0);
    lv_obj_set_style_text_color(time_label, lv_color_hex(0x0066CC), 0);
    lv_obj_align(time_label, LV_ALIGN_CENTER, 0, 40);  // 向上移动，避免被按钮遮挡
    
    // 初始化钟表显示
    draw_timer_clock_face();
    
    // 开始按钮
    start_btn = lv_btn_create(timer_window);
    lv_obj_set_size(start_btn, 120, 60);
    lv_obj_set_style_bg_color(start_btn, lv_color_hex(0x4CAF50), 0);
    lv_obj_align(start_btn, LV_ALIGN_CENTER, -150, 120);  // 向下移动，给时间标签留出空间
    lv_obj_t *start_label = lv_label_create(start_btn);
    lv_label_set_text(start_label, "开始");
    lv_obj_set_style_text_font(start_label, &SourceHanSansSC_VF, 0);
    lv_obj_center(start_label);
    lv_obj_add_event_cb(start_btn, start_btn_event_handler, LV_EVENT_CLICKED, NULL);
    
    // 停止按钮
    stop_btn = lv_btn_create(timer_window);
    lv_obj_set_size(stop_btn, 120, 60);
    lv_obj_set_style_bg_color(stop_btn, lv_color_hex(0xF44336), 0);
    lv_obj_align(stop_btn, LV_ALIGN_CENTER, 0, 120);  // 向下移动，给时间标签留出空间
    lv_obj_t *stop_label = lv_label_create(stop_btn);
    lv_label_set_text(stop_label, "停止");
    lv_obj_set_style_text_font(stop_label, &SourceHanSansSC_VF, 0);
    lv_obj_center(stop_label);
    lv_obj_add_event_cb(stop_btn, stop_btn_event_handler, LV_EVENT_CLICKED, NULL);
    lv_obj_add_flag(stop_btn, LV_OBJ_FLAG_HIDDEN);  // 初始隐藏
    
    // 重置按钮
    reset_btn = lv_btn_create(timer_window);
    lv_obj_set_size(reset_btn, 120, 60);
    lv_obj_set_style_bg_color(reset_btn, lv_color_hex(0xFF9800), 0);
    lv_obj_align(reset_btn, LV_ALIGN_CENTER, 150, 120);  // 向下移动，给时间标签留出空间
    lv_obj_t *reset_label = lv_label_create(reset_btn);
    lv_label_set_text(reset_label, "重置");
    lv_obj_set_style_text_font(reset_label, &SourceHanSansSC_VF, 0);
    lv_obj_center(reset_label);
    lv_obj_add_event_cb(reset_btn, reset_btn_event_handler, LV_EVENT_CLICKED, NULL);
    
    // 蜂鸣器控制按钮（圆形喇叭图标）
    buzzer_btn = lv_btn_create(timer_window);
    lv_obj_set_size(buzzer_btn, 60, 60);
    lv_obj_set_style_bg_color(buzzer_btn, lv_color_hex(0x2196F3), 0);  // 蓝色（开启状态）
    lv_obj_set_style_radius(buzzer_btn, LV_RADIUS_CIRCLE, 0);  // 圆形按钮
    lv_obj_align(buzzer_btn, LV_ALIGN_TOP_LEFT, 20, 20);
    lv_obj_move_foreground(buzzer_btn);
    lv_obj_t *buzzer_icon = lv_label_create(buzzer_btn);
    lv_label_set_text(buzzer_icon, CUSTOM_SYMBOL_VOLUME_MAX);
    lv_obj_set_style_text_font(buzzer_icon, &fa_solid_24, 0);
    lv_obj_set_style_text_color(buzzer_icon, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(buzzer_icon);
    lv_obj_add_event_cb(buzzer_btn, buzzer_btn_event_handler, LV_EVENT_CLICKED, NULL);
    
    // 返回按钮
    lv_obj_t *back_btn = lv_btn_create(timer_window);
    lv_obj_set_size(back_btn, 100, 50);
    lv_obj_set_style_bg_color(back_btn, lv_color_hex(0x0000FF), 0);
    lv_obj_align(back_btn, LV_ALIGN_TOP_RIGHT, -20, 20);
    lv_obj_move_foreground(back_btn);
    lv_obj_t *back_label = lv_label_create(back_btn);
    lv_label_set_text(back_label, "返回");
    lv_obj_set_style_text_font(back_label, &SourceHanSansSC_VF, 0);
    lv_obj_center(back_label);
    lv_obj_add_event_cb(back_btn, back_btn_event_handler, LV_EVENT_CLICKED, NULL);
    
    // 隐藏主屏幕
    extern lv_obj_t *main_screen;
    if (main_screen) {
        lv_obj_add_flag(main_screen, LV_OBJ_FLAG_HIDDEN);
    }
    
    // 切换到计时器窗口
    lv_scr_load(timer_window);
    lv_refr_now(NULL);
}

