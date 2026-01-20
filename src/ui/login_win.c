/**
 * @file login_win.c
 * @brief 登录窗口实现（密码锁模式）
 */

#include "login_win.h"
#include "ui_screens.h"
#include "exit_win.h"
#include "../image_viewer/image_viewer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <errno.h>
#include "lvgl/lvgl.h"
#include "lvgl/src/font/lv_font.h"
#include "lvgl/src/font/lv_symbol_def.h"
#include "lvgl/src/widgets/lv_canvas.h"

/* 声明FontAwesome字体 */
extern const lv_font_t fa_solid_24;

/* 自定义符号定义 */
#define CUSTOM_SYMBOL_VOLUME_MAX "\xEF\x80\xA8"  // FontAwesome volume-max icon (U+F028) - 圆形喇叭

/* 背景图路径（与屏保共用） */
#define SCREENSAVER_BG_IMAGE "/mdata/open.BMP"

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
#define LED2 _IOW('l', 1, unsigned long)
#define LED3 _IOW('l', 2, unsigned long)
#define LED4 _IOW('l', 3, unsigned long)
#define LED_ON  0
#define LED_OFF 1

static lv_obj_t *login_screen = NULL;
static lv_obj_t *password_display = NULL;  // 密码显示栏
static lv_obj_t *error_label = NULL;
static lv_obj_t *keypad_btns[12] = {NULL};  // 数字键盘按钮（0-9, *, #）
static bool is_logged_in = false;
static char password_input[32] = {0};  // 输入的密码
static int password_len = 0;  // 当前密码长度
static int buzzer_fd = -1;  // 蜂鸣器文件描述符
static int led_fd = -1;  // LED文件描述符
static bool buzzer_enabled = true;  // 蜂鸣器开关状态，默认开启
static lv_obj_t *buzzer_btn = NULL;  // 蜂鸣器控制按钮
static lv_obj_t *bg_canvas = NULL;  // 背景图canvas
static bool need_show_main_screen = false;  // 标志位：需要在主循环中显示主屏幕

/* 正确密码 */
#define CORRECT_PASSWORD "114514"
#define MAX_PASSWORD_LEN 32

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
    // 如果设备已存在，说明驱动已加载
    if (access(BUZZER_DEVICE, F_OK) == 0) {
        return true;
    }

    // 尝试多个可能的路径加载驱动
    const char *driver_paths[] = {
        "",  // 当前目录
        "./",
        "/mnt/udisk/",
        "/bin/",
        "/usr/lib/modules/",
        NULL
    };

    // 加载蜂鸣器驱动
    for (int i = 0; driver_paths[i] != NULL; i++) {
        char cmd[256];
        snprintf(cmd, sizeof(cmd), "insmod %s%s", driver_paths[i], "buzz_misc.ko");
        if (system(cmd) == 0) {
            break;
        }
    }

    // 检查设备文件是否存在，如果不存在则创建
    if (access(BUZZER_DEVICE, F_OK) != 0) {
        char cmd[256];
        snprintf(cmd, sizeof(cmd), "mknod %s c 251 0", BUZZER_DEVICE);
        (void)system(cmd);
    }
    
    // 设置权限
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "chmod 666 %s", BUZZER_DEVICE);
    (void)system(cmd);

    return (access(BUZZER_DEVICE, F_OK) == 0);
}

/**
 * @brief 加载LED驱动
 */
static bool load_led_driver(void) {
    // 如果设备已存在，说明驱动已加载
    if (access(LED_DEVICE, F_OK) == 0) {
        return true;
    }

    // 尝试多个可能的路径加载驱动
    const char *driver_paths[] = {
        "",  // 当前目录
        "./",
        "/mnt/udisk/",
        "/bin/",
        "/usr/lib/modules/",
        NULL
    };

    // 加载LED驱动
    for (int i = 0; driver_paths[i] != NULL; i++) {
        char cmd[256];
        snprintf(cmd, sizeof(cmd), "insmod %s%s", driver_paths[i], "leds_misc.ko");
        if (system(cmd) == 0) {
            break;
        }
    }

    // 检查设备文件是否存在，如果不存在则创建
    if (access(LED_DEVICE, F_OK) != 0) {
        char cmd[256];
        snprintf(cmd, sizeof(cmd), "mknod %s c 250 0", LED_DEVICE);
        (void)system(cmd);
    }
    
    // 设置权限
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "chmod 666 %s", LED_DEVICE);
    (void)system(cmd);

    return (access(LED_DEVICE, F_OK) == 0);
}

/**
 * @brief 初始化蜂鸣器设备
 */
static void init_buzzer(void) {
    // 先尝试加载驱动
    load_buzzer_driver();
    
    // 尝试打开蜂鸣器设备
    buzzer_fd = open(BUZZER_DEVICE, O_RDWR);
}

/**
 * @brief 初始化LED设备
 */
static void init_led(void) {
    // 先尝试加载驱动
    load_led_driver();
    
    // 尝试打开LED设备
    led_fd = open(LED_DEVICE, O_RDWR);
    if (led_fd < 0) {
        // 再次尝试创建设备节点
        char cmd[256];
        snprintf(cmd, sizeof(cmd), "mknod %s c 250 0 2>/dev/null", LED_DEVICE);
        (void)system(cmd);
        snprintf(cmd, sizeof(cmd), "chmod 666 %s 2>/dev/null", LED_DEVICE);
        (void)system(cmd);
        // 再次尝试打开
        led_fd = open(LED_DEVICE, O_RDWR);
    } else {
        // 初始化LED状态为关闭（使用unsigned long类型）
        unsigned long led_state = LED_OFF;
        ioctl(led_fd, LED1, led_state);
    }
}

/**
 * @brief LED闪烁效果（用于密码验证反馈）
 * @param times 闪烁次数
 * @param delay_ms 每次闪烁的延迟（毫秒），越小闪烁越快
 */
static void led_blink(int times, int delay_ms) {
    if (led_fd < 0) {
        // 尝试重新初始化LED
        init_led();
        if (led_fd < 0) {
            return;
        }
    }
    
    for (int i = 0; i < times; i++) {
        // LED开启（使用LED1，第三个参数需要是unsigned long类型）
        unsigned long led_state = LED_ON;
        ioctl(led_fd, LED1, led_state);
        usleep(delay_ms * 1000);
        
        // LED关闭
        led_state = LED_OFF;
        ioctl(led_fd, LED1, led_state);
        
        if (i < times - 1) {  // 最后一次不需要延迟
            usleep(delay_ms * 1000);
        }
    }
}

/**
 * @brief 更新密码显示（显示为*号）
 */
static void update_password_display(void) {
    if (!password_display) {
        return;
    }
    
    char display_str[MAX_PASSWORD_LEN + 1] = {0};
    for (int i = 0; i < password_len; i++) {
        display_str[i] = '*';
    }
    if (password_len == 0) {
        lv_label_set_text(password_display, "请输入密码");
    } else {
        lv_label_set_text(password_display, display_str);
    }
}

/**
 * @brief 动画回调函数（避免类型转换警告）
 */
static void anim_set_y_cb(void *var, int32_t v) {
    lv_obj_set_y((lv_obj_t *)var, (lv_coord_t)v);
}

/**
 * @brief 数字键盘按钮点击事件处理
 */
static void keypad_btn_event_handler(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    
    lv_obj_t *btn = lv_event_get_target(e);
    const char *btn_text = lv_label_get_text(lv_obj_get_child(btn, 0));
    
    if (!btn_text) {
        return;
    }
    
    // 使用字符串比较来处理符号（LVGL符号是多字节字符）
    // 处理删除键（LV_SYMBOL_CLOSE作为删除，不触发蜂鸣器）
    if (strcmp(btn_text, LV_SYMBOL_CLOSE) == 0 || btn_text[0] == '*') {
        if (password_len > 0) {
            password_len--;
            password_input[password_len] = '\0';
            update_password_display();
        }
        return;
    }
    
    // 处理确认键（LV_SYMBOL_OK作为确认，不触发蜂鸣器）
    if (strcmp(btn_text, LV_SYMBOL_OK) == 0 || btn_text[0] == '#') {
        // 验证密码
        if (strcmp(password_input, CORRECT_PASSWORD) == 0) {
            // 登录成功：LED闪烁2次（较慢闪烁）
            led_blink(2, 200);  // 2次，每次200ms（较慢）
            
            // 登录成功
            is_logged_in = true;
            
            // 隐藏错误提示
            if (error_label) {
                lv_obj_add_flag(error_label, LV_OBJ_FLAG_HIDDEN);
            }
            
            // 隐藏登录屏幕
            if (login_screen) {
                lv_obj_add_flag(login_screen, LV_OBJ_FLAG_HIDDEN);
            }
            
            // 设置标志位，在主循环中切换到主屏幕（避免在事件处理中阻塞）
            need_show_main_screen = true;
            printf("[密码锁] 密码验证成功，准备切换到主屏幕\n");
        } else {
            // 登录失败：LED闪烁5次（高频闪烁）
            led_blink(5, 100);  // 5次，每次100ms（高频）
            
            // 显示错误提示
            if (error_label) {
                lv_label_set_text(error_label, "密码错误，请重试");
                lv_obj_clear_flag(error_label, LV_OBJ_FLAG_HIDDEN);
            }
            
            // 清空密码
            password_len = 0;
            password_input[0] = '\0';
            update_password_display();
        }
        return;
    }
    
    // 处理数字键（0-9），触发蜂鸣器
    // 检查第一个字符是否为数字
    if (btn_text[0] >= '0' && btn_text[0] <= '9') {
        // 触发蜂鸣器（如果启用）
        beep_once();
        
        if (password_len < MAX_PASSWORD_LEN - 1) {
            password_input[password_len] = btn_text[0];
            password_len++;
            password_input[password_len] = '\0';
            update_password_display();
        }
    }
}

/**
 * @brief 退出按钮事件处理
 */
static void exit_btn_event_handler(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    exit_application();
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
 * @brief 显示登录窗口
 */
void login_win_show(void) {
    // 如果已经登录，直接返回
    if (is_logged_in) {
        return;
    }
    
    // 初始化蜂鸣器和LED
    if (buzzer_fd < 0) {
        init_buzzer();
    }
    if (led_fd < 0) {
        init_led();
    }
    
    // 创建登录屏幕
    if (login_screen == NULL) {
        login_screen = lv_obj_create(NULL);
        lv_obj_set_style_bg_opa(login_screen, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_opa(login_screen, LV_OPA_TRANSP, 0);
        lv_obj_set_size(login_screen, LV_HOR_RES, LV_VER_RES);
        
        // 创建背景图canvas（全屏）
        // 使用固定大小避免编译错误（800x480，32位色深）
        #define BG_CANVAS_WIDTH 800
        #define BG_CANVAS_HEIGHT 480
        static lv_color_t bg_buf[LV_CANVAS_BUF_SIZE_TRUE_COLOR(BG_CANVAS_WIDTH, BG_CANVAS_HEIGHT)];
        bg_canvas = lv_canvas_create(login_screen);
        lv_canvas_set_buffer(bg_canvas, bg_buf, BG_CANVAS_WIDTH, BG_CANVAS_HEIGHT, LV_IMG_CF_TRUE_COLOR);
        lv_obj_align(bg_canvas, LV_ALIGN_TOP_LEFT, 0, 0);
        lv_obj_move_background(bg_canvas);  // 移到最底层
        
        // 加载背景图
        if (load_bmp_to_canvas(bg_canvas, SCREENSAVER_BG_IMAGE) != 0) {
            printf("[密码锁] 背景图加载失败，使用灰色背景\n");
            lv_canvas_fill_bg(bg_canvas, lv_color_hex(0xf0f0f0), LV_OPA_COVER);
        } else {
            printf("[密码锁] 背景图加载成功\n");
        }
        
        // 创建密码显示栏（正上方居中，往上移）
        password_display = lv_label_create(login_screen);
        lv_label_set_text(password_display, "请输入密码");
        lv_obj_set_style_text_font(password_display, &SourceHanSansSC_VF, 0);
        lv_obj_set_style_text_color(password_display, lv_color_hex(0x1a1a1a), 0);
        lv_obj_set_style_bg_color(password_display, lv_color_hex(0xffffff), 0);
        lv_obj_set_style_bg_opa(password_display, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(password_display, lv_color_hex(0xcccccc), 0);
        lv_obj_set_style_border_width(password_display, 2, 0);
        lv_obj_set_style_pad_all(password_display, 10, 0);
        lv_obj_set_size(password_display, 400, 60);
        lv_obj_align(password_display, LV_ALIGN_TOP_MID, 0, 30);
        
        // 创建数字键盘（3x4布局：1-9, ×, 0, √，往上移）
        // 使用LVGL内置符号：LV_SYMBOL_CLOSE (×) 和 LV_SYMBOL_OK (√)
        const char *keypad_labels[12] = {"1", "2", "3", "4", "5", "6", "7", "8", "9", LV_SYMBOL_CLOSE, "0", LV_SYMBOL_OK};
        int btn_width = 80;
        int btn_height = 80;
        int btn_spacing = 10;
        int start_x = (LV_HOR_RES - (3 * btn_width + 2 * btn_spacing)) / 2;
        int start_y = 120;  // 从200改为120，往上移80px
        
        for (int i = 0; i < 12; i++) {
            int row = i / 3;
            int col = i % 3;
            int x = start_x + col * (btn_width + btn_spacing);
            int y = start_y + row * (btn_height + btn_spacing);
            
            keypad_btns[i] = lv_btn_create(login_screen);
            lv_obj_set_size(keypad_btns[i], btn_width, btn_height);
            lv_obj_set_pos(keypad_btns[i], x, y);
            
            // 根据按键类型设置颜色：LV_SYMBOL_CLOSE（取消键）为红色，LV_SYMBOL_OK（确认键）为绿色
            // 使用字符串比较来处理LVGL符号
            if (strcmp(keypad_labels[i], LV_SYMBOL_CLOSE) == 0) {
                // 取消键：红色
                lv_obj_set_style_bg_color(keypad_btns[i], lv_color_hex(0xF44336), 0);
                lv_obj_set_style_border_color(keypad_btns[i], lv_color_hex(0xd32f2f), 0);
            } else if (strcmp(keypad_labels[i], LV_SYMBOL_OK) == 0) {
                // 确认键：绿色
                lv_obj_set_style_bg_color(keypad_btns[i], lv_color_hex(0x4CAF50), 0);
                lv_obj_set_style_border_color(keypad_btns[i], lv_color_hex(0x388e3c), 0);
            } else {
                // 数字键：白色
                lv_obj_set_style_bg_color(keypad_btns[i], lv_color_hex(0xffffff), 0);
                lv_obj_set_style_border_color(keypad_btns[i], lv_color_hex(0xcccccc), 0);
            }
            lv_obj_set_style_border_width(keypad_btns[i], 2, 0);
            
            lv_obj_t *btn_label = lv_label_create(keypad_btns[i]);
            lv_label_set_text(btn_label, keypad_labels[i]);
            // 对于符号按钮，使用LVGL默认字体（包含FontAwesome符号）
            // 对于数字键，使用中文字体
            if (strcmp(keypad_labels[i], LV_SYMBOL_CLOSE) == 0 || strcmp(keypad_labels[i], LV_SYMBOL_OK) == 0) {
                // 符号按钮：使用LVGL默认字体（Montserrat，包含FontAwesome符号）
                // 使用条件编译选择可用的字体
                #if LV_FONT_MONTSERRAT_20
                extern const lv_font_t lv_font_montserrat_20;
                lv_obj_set_style_text_font(btn_label, &lv_font_montserrat_20, 0);
                #elif LV_FONT_MONTSERRAT_18
                extern const lv_font_t lv_font_montserrat_18;
                lv_obj_set_style_text_font(btn_label, &lv_font_montserrat_18, 0);
                #elif LV_FONT_MONTSERRAT_16
                extern const lv_font_t lv_font_montserrat_16;
                lv_obj_set_style_text_font(btn_label, &lv_font_montserrat_16, 0);
                #elif LV_FONT_MONTSERRAT_14
                extern const lv_font_t lv_font_montserrat_14;
                lv_obj_set_style_text_font(btn_label, &lv_font_montserrat_14, 0);
                #else
                // 如果没有可用的Montserrat字体，使用默认字体
                lv_obj_set_style_text_font(btn_label, LV_FONT_DEFAULT, 0);
                #endif
                lv_obj_set_style_text_color(btn_label, lv_color_hex(0xffffff), 0);
            } else {
                // 数字键：使用中文字体
                lv_obj_set_style_text_font(btn_label, &SourceHanSansSC_VF, 0);
                lv_obj_set_style_text_color(btn_label, lv_color_hex(0x1a1a1a), 0);
            }
            lv_obj_center(btn_label);
            
            lv_obj_add_event_cb(keypad_btns[i], keypad_btn_event_handler, LV_EVENT_CLICKED, NULL);
        }
        
        // 创建错误提示标签
        error_label = lv_label_create(login_screen);
        lv_label_set_text(error_label, "");
        lv_obj_set_style_text_font(error_label, &SourceHanSansSC_VF, 0);
        lv_obj_set_style_text_color(error_label, lv_color_hex(0xff0000), 0);
        lv_obj_align(error_label, LV_ALIGN_BOTTOM_MID, 0, -20);
        lv_obj_add_flag(error_label, LV_OBJ_FLAG_HIDDEN);
        
        // 蜂鸣器控制按钮（圆形喇叭图标）
        buzzer_btn = lv_btn_create(login_screen);
        lv_obj_set_size(buzzer_btn, 60, 60);
        lv_obj_set_style_bg_color(buzzer_btn, lv_color_hex(0x2196F3), 0);  // 蓝色（开启状态）
        lv_obj_set_style_radius(buzzer_btn, LV_RADIUS_CIRCLE, 0);  // 圆形按钮
        lv_obj_align(buzzer_btn, LV_ALIGN_TOP_RIGHT, -20, 20);
        lv_obj_move_foreground(buzzer_btn);
        lv_obj_t *buzzer_icon = lv_label_create(buzzer_btn);
        lv_label_set_text(buzzer_icon, CUSTOM_SYMBOL_VOLUME_MAX);
        lv_obj_set_style_text_font(buzzer_icon, &fa_solid_24, 0);
        lv_obj_set_style_text_color(buzzer_icon, lv_color_hex(0xFFFFFF), 0);
        lv_obj_center(buzzer_icon);
        lv_obj_add_event_cb(buzzer_btn, buzzer_btn_event_handler, LV_EVENT_CLICKED, NULL);
        
        // 退出程序按钮（右下角，红色，小圆角矩形，带图标）
        lv_obj_t *exit_btn = lv_btn_create(login_screen);
        lv_obj_set_size(exit_btn, 100, 50);
        lv_obj_set_style_bg_color(exit_btn, lv_color_hex(0xF44336), 0);  // 红色
        lv_obj_set_style_radius(exit_btn, 8, 0);  // 小圆角
        lv_obj_clear_flag(exit_btn, LV_OBJ_FLAG_SCROLLABLE);  // 禁用滚动
        lv_obj_align(exit_btn, LV_ALIGN_BOTTOM_RIGHT, -20, -20);
        lv_obj_move_foreground(exit_btn);
        
        // 退出图标和文字直接放在按钮上（不使用容器，避免滚动条）
        lv_obj_t *exit_label = lv_label_create(exit_btn);
        // 使用图标和文字组合
        char exit_text[32];
        snprintf(exit_text, sizeof(exit_text), "%s 退出", LV_SYMBOL_POWER);
        lv_label_set_text(exit_label, exit_text);
        lv_obj_set_style_text_font(exit_label, &SourceHanSansSC_VF, 0);
        lv_obj_set_style_text_color(exit_label, lv_color_hex(0xFFFFFF), 0);
        lv_obj_center(exit_label);
        
        // 退出按钮事件处理
        lv_obj_add_event_cb(exit_btn, exit_btn_event_handler, LV_EVENT_CLICKED, NULL);
        
        // 初始化密码
        password_len = 0;
        password_input[0] = '\0';
    } else {
        // 如果登录屏幕已存在，确保它可见
        lv_obj_clear_flag(login_screen, LV_OBJ_FLAG_HIDDEN);
        
        // 清空密码
        password_len = 0;
        password_input[0] = '\0';
        update_password_display();
        
        // 隐藏错误提示
        if (error_label) {
            lv_obj_add_flag(error_label, LV_OBJ_FLAG_HIDDEN);
        }
    }
    
    // 切换到登录屏幕
    lv_scr_load(login_screen);
    
    // 处理几次定时器，确保LVGL完成渲染
    for (int i = 0; i < 5; i++) {
        lv_timer_handler();
        usleep(10000);  // 10ms
    }
    
    // 强制刷新
    lv_refr_now(NULL);
    
    printf("[密码锁] 密码锁窗口显示完成\n");
}

/**
 * @brief 检查是否已登录
 */
bool login_win_is_logged_in(void) {
    return is_logged_in;
}

/**
 * @brief 检查并处理主屏幕显示（在主循环中调用）
 */
void login_win_check_show_main(void) {
    if (!need_show_main_screen) {
        return;
    }
    
    need_show_main_screen = false;
    
    // 获取主页第一页screen
    extern lv_obj_t* get_main_page1_screen(void);
    lv_obj_t *main_page_screen = get_main_page1_screen();
    if (!main_page_screen) {
        printf("[密码锁] 错误：主屏幕未初始化\n");
        return;
    }
    
    printf("[密码锁] 开始切换到主屏幕\n");
    
    // 隐藏登录屏幕
    if (login_screen) {
        lv_obj_add_flag(login_screen, LV_OBJ_FLAG_HIDDEN);
    }
    
    // 切换到主屏幕（使用滑动动画）
    lv_obj_set_y(login_screen, 0);
    lv_obj_set_y(main_page_screen, 480);  // 使用固定值
    lv_obj_clear_flag(main_page_screen, LV_OBJ_FLAG_HIDDEN);
    lv_scr_load(main_page_screen);
    
    // 创建动画：密码锁向上滑出，主页从下往上滑入
    lv_anim_t anim1, anim2;
    
    // 密码锁向上滑出
    lv_anim_init(&anim1);
    lv_anim_set_var(&anim1, login_screen);
    lv_anim_set_values(&anim1, 0, -480);
    lv_anim_set_time(&anim1, 300);
    lv_anim_set_exec_cb(&anim1, anim_set_y_cb);
    lv_anim_start(&anim1);
    
    // 主页从下往上滑入
    lv_anim_init(&anim2);
    lv_anim_set_var(&anim2, main_page_screen);
    lv_anim_set_values(&anim2, 480, 0);
    lv_anim_set_time(&anim2, 300);
    lv_anim_set_exec_cb(&anim2, anim_set_y_cb);
    lv_anim_start(&anim2);
    
    // 强制刷新
    lv_refr_now(NULL);
    
    printf("[密码锁] 主屏幕切换完成\n");
}

