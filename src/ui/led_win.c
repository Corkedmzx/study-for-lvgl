#include "led_win.h"
#include "ui_screens.h"
#include "video_win.h"  // 引入video_screen
#include "weather_win.h"  // 引入weather_window
#include "../common/common.h"
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include "lvgl/lvgl.h"
#include "lvgl/src/font/lv_font.h"

/* 声明SourceHanSansSC_VF字体（定义在bin/SourceHanSansSC_VF.c中） */
#if LV_FONT_SOURCE_HAN_SANS_SC_VF
extern const lv_font_t SourceHanSansSC_VF;
#endif

// 设备节点路径
#define LED_DEVICE "/dev/leds_misc"
#define BUZZER_DEVICE "/dev/buzz_misc"

// 驱动文件路径（参考lcdc.cpp，直接使用文件名）
#define LED_DRIVER "leds_misc.ko"
#define BUZZER_DRIVER "buzz_misc.ko"

// 设备控制码
#define LED1 _IOW('l', 0, unsigned long)
#define LED2 _IOW('l', 1, unsigned long)
#define LED3 _IOW('l', 2, unsigned long)
#define LED4 _IOW('l', 3, unsigned long)
#define LED_ON  0
#define LED_OFF 1

#define BUZZ_ON  _IOW('b', 1, unsigned long)
#define BUZZ_OFF _IOW('b', 0, unsigned long)

static lv_obj_t *led_win = NULL;
static int led_fd = -1;
static int buzzer_fd = -1;
static int led_states[4] = {LED_OFF, LED_OFF, LED_OFF, LED_OFF};

static bool load_drivers(void) {
    // 参考lcdc.cpp的方式，先尝试加载驱动
    // 如果设备已存在，说明驱动已加载
    if (access(LED_DEVICE, F_OK) == 0 && access(BUZZER_DEVICE, F_OK) == 0) {
        return true;
    }

    // 尝试多个可能的路径
    const char *driver_paths[] = {
        "",  // 当前目录
        "./",
        "/mnt/udisk/",
        "/bin/",
        "/usr/lib/modules/",
        NULL
    };

    bool led_loaded = false;
    bool buzzer_loaded = false;

    // 加载LED驱动
    for (int i = 0; driver_paths[i] != NULL; i++) {
        char cmd[256];
        snprintf(cmd, sizeof(cmd), "insmod %s%s", driver_paths[i], LED_DRIVER);
        if (system(cmd) == 0) {
            led_loaded = true;
            printf("LED driver loaded from: %s%s\n", driver_paths[i], LED_DRIVER);
            break;
        }
    }

    if (!led_loaded) {
        printf("Warning: Failed to load LED driver, trying to continue...\n");
    }

    // 加载蜂鸣器驱动
    for (int i = 0; driver_paths[i] != NULL; i++) {
        char cmd[256];
        snprintf(cmd, sizeof(cmd), "insmod %s%s", driver_paths[i], BUZZER_DRIVER);
        if (system(cmd) == 0) {
            buzzer_loaded = true;
            printf("Buzzer driver loaded from: %s%s\n", driver_paths[i], BUZZER_DRIVER);
            break;
        }
    }

    if (!buzzer_loaded) {
        printf("Warning: Failed to load buzzer driver, trying to continue...\n");
        if (led_loaded) {
            system("rmmod leds_misc");
        }
    }

    // 检查设备文件是否存在，如果不存在则创建
    if (access(LED_DEVICE, F_OK) != 0) {
        char cmd[256];
        snprintf(cmd, sizeof(cmd), "mknod %s c 250 0", LED_DEVICE);
        (void)system(cmd);
    }
    if (access(BUZZER_DEVICE, F_OK) != 0) {
        char cmd[256];
        snprintf(cmd, sizeof(cmd), "mknod %s c 251 0", BUZZER_DEVICE);
        (void)system(cmd);
    }
    
    // 设置权限
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "chmod 666 %s %s", LED_DEVICE, BUZZER_DEVICE);
    (void)system(cmd);

    // 如果至少一个设备可用，返回true
    return (access(LED_DEVICE, F_OK) == 0 || access(BUZZER_DEVICE, F_OK) == 0);
}

static void unload_drivers(void) {
    (void)system("rmmod buzz_misc leds_misc 2>/dev/null");
}

static bool init_hardware(void) {
    led_fd = open(LED_DEVICE, O_RDWR);
    if (led_fd < 0) {
        perror("Open LED device failed");
        return false;
    }

    buzzer_fd = open(BUZZER_DEVICE, O_RDWR);
    if (buzzer_fd < 0) {
        perror("Open buzzer device failed");
        close(led_fd);
        led_fd = -1;
        return false;
    }

    for (int i = 0; i < 4; i++) {
        ioctl(led_fd, LED1 + i, LED_OFF);
    }
    ioctl(buzzer_fd, BUZZ_OFF);

    return true;
}

static void release_hardware(void) {
    printf("[LED控制] 释放硬件资源...\n");
    if (led_fd >= 0) {
        for (int i = 0; i < 4; i++) {
            int ret = ioctl(led_fd, LED1 + i, LED_OFF);
            if (ret < 0) {
                printf("[LED控制] 警告: ioctl LED%d OFF 失败: %s\n", i + 1, strerror(errno));
            }
        }
        close(led_fd);
        led_fd = -1;
        printf("[LED控制] LED设备已关闭\n");
    }

    if (buzzer_fd >= 0) {
        int ret = ioctl(buzzer_fd, BUZZ_OFF);
        if (ret < 0) {
            printf("[LED控制] 警告: ioctl BUZZ_OFF 失败: %s\n", strerror(errno));
        }
        close(buzzer_fd);
        buzzer_fd = -1;
        printf("[LED控制] 蜂鸣器设备已关闭\n");
    }
    printf("[LED控制] 硬件资源已释放\n");
}

static void led_btn_event_handler(lv_event_t *e) {
    // 检查事件类型，确保是点击事件
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    
    // 检查设备是否初始化
    if (led_fd < 0 || buzzer_fd < 0) {
        printf("错误: LED或蜂鸣器设备未初始化\n");
        return;
    }
    
    lv_obj_t *btn = lv_event_get_target(e);
    int btn_id = (intptr_t)lv_event_get_user_data(e);
    
    if (btn_id < 0 || btn_id >= 4) {
        return;
    }
    
    // 切换LED状态（参考lcdc.cpp）
    led_states[btn_id] = !led_states[btn_id];
    
    // 控制LED（确保可以多次响应）
    if (led_fd >= 0) {
        int ret = ioctl(led_fd, LED1 + btn_id, led_states[btn_id]);
        if (ret < 0) {
            printf("[LED控制] 警告: ioctl LED%d 失败: %s\n", btn_id + 1, strerror(errno));
        }
    }
    
    // 触发蜂鸣器（参考lcdc.cpp）
    if (buzzer_fd >= 0) {
        int ret = ioctl(buzzer_fd, BUZZ_ON);
        if (ret < 0) {
            printf("[LED控制] 警告: ioctl BUZZ_ON 失败: %s\n", strerror(errno));
        }
        usleep(100000);  // 100ms
        ret = ioctl(buzzer_fd, BUZZ_OFF);
        if (ret < 0) {
            printf("[LED控制] 警告: ioctl BUZZ_OFF 失败: %s\n", strerror(errno));
        }
    }
    
    // 更新按钮颜色（参考lcdc.cpp：LED开启时颜色变暗）
    lv_color_t color;
    switch (btn_id) {
        case 0: color = led_states[0] ? lv_color_hex(0x55000000) : lv_color_hex(0xFF0000); break;
        case 1: color = led_states[1] ? lv_color_hex(0x55000000) : lv_color_hex(0x00FF00); break;
        case 2: color = led_states[2] ? lv_color_hex(0x55000000) : lv_color_hex(0x0000FF); break;
        case 3: color = led_states[3] ? lv_color_hex(0x55000000) : lv_color_hex(0xFFFF00); break;
        default: color = lv_color_hex(0xFFFFFF); break;
    }
    lv_obj_set_style_bg_color(btn, color, LV_PART_MAIN);
    
    printf("LED%d %s\n", btn_id + 1, led_states[btn_id] ? "ON" : "OFF");
}

void led_win_event_handler(lv_event_t *e) {
    if(lv_event_get_code(e) == LV_EVENT_CLICKED) {
        release_hardware();
        unload_drivers();
        
        if (led_win) {
            lv_obj_del(led_win);
            led_win = NULL;
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
}

void show_led_window(void) {
    // 停止其他模块
    extern bool simple_video_is_playing(void);
    extern void simple_video_stop(void);
    extern void video_touch_control_stop(void);
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
    if (video_screen) {
        lv_obj_add_flag(video_screen, LV_OBJ_FLAG_HIDDEN);
    }
    if (weather_window) {
        lv_obj_add_flag(weather_window, LV_OBJ_FLAG_HIDDEN);
    }
    
    if (simple_video_is_playing()) {
        video_touch_control_stop();
        simple_video_stop();
    }
    if (audio_player_is_playing()) {
        audio_player_stop();
    }
    
    // 如果窗口已存在，先清理旧的窗口和硬件资源
    if (led_win) {
        // 先释放硬件资源
        release_hardware();
        // 删除旧窗口
        lv_obj_del(led_win);
        led_win = NULL;
    }

    printf("正在加载LED驱动...\n");
    // 检查驱动是否已加载，如果已加载则跳过
    if (access(LED_DEVICE, F_OK) != 0 || access(BUZZER_DEVICE, F_OK) != 0) {
        if (!load_drivers()) {
            printf("警告: LED驱动加载失败，尝试继续...\n");
            // 不返回，继续创建UI，即使驱动加载失败
        }
    } else {
        printf("LED驱动已加载\n");
    }

    printf("正在初始化LED硬件...\n");
    // 确保硬件已关闭，然后重新初始化
    release_hardware();
    if (!init_hardware()) {
        printf("警告: LED硬件初始化失败，尝试继续...\n");
        // 不返回，继续创建UI，即使硬件初始化失败
    }

    // 获取屏幕尺寸（使用固定值，因为屏幕是独立的）
    lv_coord_t screen_width = 800;
    lv_coord_t screen_height = 480;

    // 创建全屏窗口
    // 创建独立的LED控制屏幕
    led_win = lv_obj_create(NULL);
    lv_obj_set_size(led_win, screen_width, screen_height);
    lv_obj_set_pos(led_win, 0, 0);
    lv_obj_set_style_bg_color(led_win, lv_color_white(), 0);
    lv_obj_clear_flag(led_win, LV_OBJ_FLAG_SCROLLABLE);

    // 参考lcdc.cpp，创建四个色块区域（缩小尺寸以预留返回按钮空间）
    const int block_width = screen_width / 2;
    const int block_height = (screen_height - 60) / 2;  // 预留顶部60像素给返回按钮
    
    // 创建四个色块（使用canvas或直接绘制）
    lv_color_t block_colors[] = {
        lv_color_hex(0xFF0000),  // 红色 - LED1
        lv_color_hex(0x00FF00),  // 绿色 - LED2
        lv_color_hex(0x0000FF),  // 蓝色 - LED3
        lv_color_hex(0xFFFF00)   // 黄色 - LED4
    };
    
    const char *btn_labels[] = {"LED1", "LED2", "LED3", "LED4"};

    // 创建四个色块按钮（缩小尺寸）
    for (int i = 0; i < 4; i++) {
        lv_obj_t *btn = lv_btn_create(led_win);
        lv_obj_set_size(btn, block_width - 20, block_height - 20);  // 缩小按钮
        lv_obj_set_style_bg_color(btn, block_colors[i], LV_PART_MAIN);
        lv_obj_set_style_radius(btn, 10, 0);
        
        // 计算位置（参考lcdc.cpp的布局）
        int x_pos = (i % 2) * block_width + 10;
        int y_pos = (i / 2) * block_height + 60;  // 向下偏移60像素预留返回按钮空间
        
        lv_obj_set_pos(btn, x_pos, y_pos);
        
        lv_obj_t *label = lv_label_create(btn);
        lv_label_set_text(label, btn_labels[i]);
        lv_obj_set_style_text_font(label, &SourceHanSansSC_VF, 0);
        lv_obj_center(label);
        
        lv_obj_add_event_cb(btn, led_btn_event_handler, LV_EVENT_CLICKED, (void*)(intptr_t)i);
    }

    // 返回按钮（放在顶部右侧）
    lv_obj_t *exit_btn = lv_btn_create(led_win);
    lv_obj_set_size(exit_btn, 100, 50);
    lv_obj_set_style_bg_color(exit_btn, lv_color_hex(0x0000FF), 0);
    lv_obj_align(exit_btn, LV_ALIGN_TOP_RIGHT, -20, 10);
    lv_obj_move_foreground(exit_btn);  // 确保在最上层
    lv_obj_t *exit_label = lv_label_create(exit_btn);
    lv_label_set_text(exit_label, "返回");
    lv_obj_set_style_text_font(exit_label, &SourceHanSansSC_VF, 0);
    lv_obj_center(exit_label);
    lv_obj_add_event_cb(exit_btn, led_win_event_handler, LV_EVENT_CLICKED, NULL);

    // 隐藏主窗口
    extern lv_obj_t *main_screen;
    if (main_screen) {
        lv_obj_add_flag(main_screen, LV_OBJ_FLAG_HIDDEN);
    }
    
    // 切换到LED窗口并强制刷新
    lv_scr_load(led_win);
    lv_refr_now(NULL);  // 强制刷新显示
    
}

