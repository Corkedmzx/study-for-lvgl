/**
 * @file login_win.c
 * @brief 登录窗口实现（密码锁模式）
 */

#include "login_win.h"
#include "ui_screens.h"
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <errno.h>
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

/* 正确密码 */
#define CORRECT_PASSWORD "114514"
#define MAX_PASSWORD_LEN 32

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

    bool buzzer_loaded = false;

    // 加载蜂鸣器驱动
    for (int i = 0; driver_paths[i] != NULL; i++) {
        char cmd[256];
        snprintf(cmd, sizeof(cmd), "insmod %s%s", driver_paths[i], "buzz_misc.ko");
        if (system(cmd) == 0) {
            buzzer_loaded = true;
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

    bool led_loaded = false;

    // 加载LED驱动
    for (int i = 0; driver_paths[i] != NULL; i++) {
        char cmd[256];
        snprintf(cmd, sizeof(cmd), "insmod %s%s", driver_paths[i], "leds_misc.ko");
        if (system(cmd) == 0) {
            led_loaded = true;
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
    
    char key = btn_text[0];
    
    // 处理删除键（*键作为删除，不触发蜂鸣器）
    if (key == '*') {
        if (password_len > 0) {
            password_len--;
            password_input[password_len] = '\0';
            update_password_display();
        }
        return;
    }
    
    // 处理确认键（#键作为确认，不触发蜂鸣器）
    if (key == '#') {
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
            
            // 切换到主屏幕
            extern lv_obj_t *main_screen;
            if (main_screen) {
                lv_obj_clear_flag(main_screen, LV_OBJ_FLAG_HIDDEN);
                lv_scr_load(main_screen);
                
                // 强制刷新
                lv_refr_now(NULL);
            }
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
    if (key >= '0' && key <= '9') {
        // 触发蜂鸣器
        beep_once();
        
        if (password_len < MAX_PASSWORD_LEN - 1) {
            password_input[password_len] = key;
            password_len++;
            password_input[password_len] = '\0';
            update_password_display();
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
        lv_obj_set_style_bg_color(login_screen, lv_color_hex(0xf0f0f0), 0);
        lv_obj_set_size(login_screen, LV_HOR_RES, LV_VER_RES);
        
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
        
        // 创建数字键盘（3x4布局：1-9, *, 0, #，往上移）
        const char *keypad_labels[12] = {"1", "2", "3", "4", "5", "6", "7", "8", "9", "*", "0", "#"};
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
            lv_obj_set_style_bg_color(keypad_btns[i], lv_color_hex(0xffffff), 0);
            lv_obj_set_style_border_color(keypad_btns[i], lv_color_hex(0xcccccc), 0);
            lv_obj_set_style_border_width(keypad_btns[i], 2, 0);
            
            lv_obj_t *btn_label = lv_label_create(keypad_btns[i]);
            lv_label_set_text(btn_label, keypad_labels[i]);
            lv_obj_set_style_text_font(btn_label, &SourceHanSansSC_VF, 0);
            lv_obj_set_style_text_color(btn_label, lv_color_hex(0x1a1a1a), 0);
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
    
    // 强制刷新
    lv_refr_now(NULL);
}

/**
 * @brief 检查是否已登录
 */
bool login_win_is_logged_in(void) {
    return is_logged_in;
}

