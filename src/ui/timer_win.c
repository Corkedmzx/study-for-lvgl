/**
 * @file timer_win.c
 * @brief 计时器窗口实现
 */

#include "timer_win.h"
#include "ui_screens.h"
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <pthread.h>
#include <time.h>
#include "lvgl/lvgl.h"
#include "lvgl/src/font/lv_font.h"

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
static lv_obj_t *start_btn = NULL;
static lv_obj_t *stop_btn = NULL;
static lv_obj_t *reset_btn = NULL;

static int buzzer_fd = -1;
static int led_fd = -1;
static bool is_running = false;
static int elapsed_seconds = 0;
static pthread_t timer_thread = 0;
static pthread_mutex_t timer_mutex = PTHREAD_MUTEX_INITIALIZER;

/**
 * @brief 触发蜂鸣器（短响）
 */
static void beep_once(void) {
    if (buzzer_fd >= 0) {
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
            
            // 蜂鸣器响
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
    time_label = lv_label_create(timer_window);
    lv_label_set_text(time_label, "00:00");
    lv_obj_set_style_text_font(time_label, &SourceHanSansSC_VF, 0);
    lv_obj_set_style_text_color(time_label, lv_color_hex(0x0066CC), 0);
    lv_obj_align(time_label, LV_ALIGN_CENTER, 0, -50);
    
    // 开始按钮
    start_btn = lv_btn_create(timer_window);
    lv_obj_set_size(start_btn, 120, 60);
    lv_obj_set_style_bg_color(start_btn, lv_color_hex(0x4CAF50), 0);
    lv_obj_align(start_btn, LV_ALIGN_CENTER, -150, 80);
    lv_obj_t *start_label = lv_label_create(start_btn);
    lv_label_set_text(start_label, "开始");
    lv_obj_set_style_text_font(start_label, &SourceHanSansSC_VF, 0);
    lv_obj_center(start_label);
    lv_obj_add_event_cb(start_btn, start_btn_event_handler, LV_EVENT_CLICKED, NULL);
    
    // 停止按钮
    stop_btn = lv_btn_create(timer_window);
    lv_obj_set_size(stop_btn, 120, 60);
    lv_obj_set_style_bg_color(stop_btn, lv_color_hex(0xF44336), 0);
    lv_obj_align(stop_btn, LV_ALIGN_CENTER, 0, 80);
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
    lv_obj_align(reset_btn, LV_ALIGN_CENTER, 150, 80);
    lv_obj_t *reset_label = lv_label_create(reset_btn);
    lv_label_set_text(reset_label, "重置");
    lv_obj_set_style_text_font(reset_label, &SourceHanSansSC_VF, 0);
    lv_obj_center(reset_label);
    lv_obj_add_event_cb(reset_btn, reset_btn_event_handler, LV_EVENT_CLICKED, NULL);
    
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

