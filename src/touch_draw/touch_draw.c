/**
 * @file touch_draw.c
 * @brief 触摸绘图功能模块实现
 * 
 * 基于05touch.cpp的功能，集成到LVGL系统中
 * 支持触摸屏绘图，使用framebuffer直接绘制
 */

#include "touch_draw.h"
#include "../common/common.h"
#include "../common/touch_device.h"
#include "lvgl/src/font/lv_font.h"
#include "lvgl/src/font/lv_symbol_def.h"
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/input.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/fb.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <inttypes.h>

// 设备路径
#define TOUCH_DEVICE "/dev/input/event0"
#define FRAMEBUFFER_DEV "/dev/fb0"

// 屏幕分辨率
#define SCREEN_WIDTH 800
#define SCREEN_HEIGHT 480

// Linux framebuffer通常使用BGRA格式（而不是ARGB）
// 需要将ARGB格式转换为BGRA格式
// ARGB格式：0xAARRGGBB (Alpha, Red, Green, Blue)
// BGRA格式：0xBBGGRRAA (Blue, Green, Red, Alpha)

// ARGB格式的颜色定义（alpha通道应该是0xFF表示不透明）
// 注意：ARGB格式是 0xAARRGGBB（高位到低位：Alpha, Red, Green, Blue）
#define COLOR_RED_ARGB     0xFFFF0000  // A=0xFF, R=0xFF, G=0x00, B=0x00 (红色)
#define COLOR_GREEN_ARGB   0xFF00FF00  // A=0xFF, R=0x00, G=0xFF, B=0x00 (绿色)
#define COLOR_BLUE_ARGB    0xFF0000FF  // A=0xFF, R=0x00, G=0x00, B=0xFF (蓝色)
#define COLOR_YELLOW_ARGB  0xFFFFFF00  // A=0xFF, R=0xFF, G=0xFF, B=0x00 (黄色)
#define COLOR_GRAY_ARGB    0xFF808080  // A=0xFF, R=0x80, G=0x80, B=0x80 (灰色)
#define COLOR_WHITE_ARGB   0xFFFFFFFF  // A=0xFF, R=0xFF, G=0xFF, B=0xFF (白色)
#define COLOR_BLACK_ARGB   0xFF000000  // A=0xFF, R=0x00, G=0x00, B=0x00 (黑色)

// ARGB转BGRA的函数
// LVGL使用BGRA格式：blue在最低字节，alpha在最高字节
// ARGB格式：0xAARRGGBB (高字节到低字节: Alpha, Red, Green, Blue)
// BGRA格式：0xAARRGGBB (但结构体是: blue, green, red, alpha)
// 在小端序中，BGRA结构体存储为：低地址[b][g][r][a]高地址
// 所以uint32_t值应该是：0xAARRGGBB，其中：
//   字节0(低位): B (blue)
//   字节1: G (green)  
//   字节2: R (red)
//   字节3(高位): A (alpha)
static inline uint32_t argb_to_bgra(uint32_t argb) {
    uint32_t a = (argb >> 24) & 0xFF;  // Alpha
    uint32_t r = (argb >> 16) & 0xFF;  // Red
    uint32_t g = (argb >> 8) & 0xFF;   // Green
    uint32_t b = (argb >> 0) & 0xFF;   // Blue
    // 转换为BGRA格式的uint32_t: 低字节到高字节是[b][g][r][a]
    // 在小端序中，0xAARRGGBB表示字节顺序从低到高是[b][g][r][a]
    return (b) | (g << 8) | (r << 16) | (a << 24);
}

// 转换为framebuffer期望的颜色格式
// 根据错误报告：红色绘制成蓝色，说明R和B的位置交换了
// framebuffer可能期望ARGB格式（0xAARRGGBB）而不是BGRA
// 或者framebuffer期望RGBA格式，或者需要字节交换
// 测试：如果红色(R=255,G=0,B=0)绘制成蓝色，可能是：
// - 当前写的是BGRA格式的红色：0xFF0000FF (B=0, G=0, R=255, A=255)
// - framebuffer期望ARGB格式：0xFFFF0000 (A=255, R=255, G=0, B=0)
// 如果framebuffer按ARGB解释，0xFF0000FF会被解释为蓝色(B=255)
// 因此需要转换为ARGB格式
// ARGB格式：0xAARRGGBB (Alpha, Red, Green, Blue)
#define COLOR_RED_BGRA     0xFFFF0000  // ARGB: 红色 R=255, G=0, B=0
#define COLOR_GREEN_BGRA   0xFF00FF00  // ARGB: 绿色 R=0, G=255, B=0
#define COLOR_BLUE_BGRA    0xFF0000FF  // ARGB: 蓝色 R=0, G=0, B=255
#define COLOR_YELLOW_BGRA  0xFFFFFF00  // ARGB: 黄色 R=255, G=255, B=0
#define COLOR_GRAY_BGRA    0xFF808080  // ARGB: 灰色 R=128, G=128, B=128
#define COLOR_WHITE_BGRA   0xFFFFFFFF  // ARGB: 白色 R=255, G=255, B=255
#define COLOR_BLACK_BGRA   0xFF000000  // ARGB: 黑色 R=0, G=0, B=0

// 兼容宏（使用BGRA值）
#define COLOR_RED     COLOR_RED_BGRA
#define COLOR_GREEN   COLOR_GREEN_BGRA
#define COLOR_BLUE    COLOR_BLUE_BGRA
#define COLOR_YELLOW  COLOR_YELLOW_BGRA
#define COLOR_GRAY    COLOR_GRAY_BGRA
#define COLOR_WHITE   COLOR_WHITE_BGRA
#define COLOR_BLACK   COLOR_BLACK_BGRA

// 触摸状态
enum TouchState {
    TOUCH_IDLE,
    TOUCH_PRESSED,
    TOUCH_MOVING
};

struct FramebufferInfo {
    int fd;
    char* fbp;
    struct fb_var_screeninfo vinfo;
    struct fb_fix_screeninfo finfo;
    long int screensize;
};

// 全局变量
static lv_obj_t *touch_draw_window = NULL;
static bool touch_draw_running = false;
static pthread_t touch_draw_thread;
static struct FramebufferInfo fb_info;
static int touch_fd = -1;
static int saved_page_index = 0;  // 保存进入触摸绘图时的页面索引
static pthread_mutex_t fb_mutex = PTHREAD_MUTEX_INITIALIZER;  // 保护framebuffer访问的互斥锁

// 触摸坐标范围
static int touch_min_x = 0;
static int touch_max_x = 1024;
static int touch_min_y = 0;
static int touch_max_y = 600;

// 绘图状态变量
static int pen_size = 2;  // 笔触大小：1=细，2=中，3=粗
static int current_color_index = 0;  // 当前颜色索引
static bool eraser_mode = false;  // 橡皮擦模式
static uint32_t color_list[] = {
    COLOR_RED_BGRA,
    COLOR_GREEN_BGRA,
    COLOR_BLUE_BGRA,
    COLOR_YELLOW_BGRA,
    COLOR_BLACK_BGRA,
    COLOR_GRAY_BGRA,
};
static const int color_count = sizeof(color_list) / sizeof(color_list[0]);

// BGRA转ARGB函数（用于LVGL显示）
static inline uint32_t bgra_to_argb(uint32_t bgra) {
    uint32_t b = (bgra >> 0) & 0xFF;   // Blue (低位)
    uint32_t g = (bgra >> 8) & 0xFF;   // Green
    uint32_t r = (bgra >> 16) & 0xFF;  // Red
    uint32_t a = (bgra >> 24) & 0xFF;  // Alpha (高位)
    // 转换为ARGB格式: [A][R][G][B]
    return (a << 24) | (r << 16) | (g << 8) | b;
}

// 保存按钮对象指针（用于更新选中状态）
static lv_obj_t *pen_size_btns[3] = {NULL, NULL, NULL};  // 细、中、粗三个按钮
static lv_obj_t *color_btns[6] = {NULL, NULL, NULL, NULL, NULL, NULL};  // 六个颜色按钮
static lv_obj_t *eraser_btn = NULL;

// 清除屏幕
static void clear_screen(struct FramebufferInfo* fb, uint32_t color) {
    pthread_mutex_lock(&fb_mutex);
    uint32_t* fb_ptr = (uint32_t*)fb->fbp;
    for (int i = 0; i < fb->screensize / 4; i++) {
        fb_ptr[i] = color;
    }
    pthread_mutex_unlock(&fb_mutex);
}

// 绘制一个像素点
static void draw_pixel(struct FramebufferInfo* fb, int x, int y, uint32_t color) {
    if (x < 0 || x >= fb->vinfo.xres || y < 0 || y >= fb->vinfo.yres) {
        return;
    }
    
    pthread_mutex_lock(&fb_mutex);
    // 直接计算位置
    int offset = (y * fb->finfo.line_length) + (x * 4); // 32位=4字节
    *((uint32_t*)(fb->fbp + offset)) = color;
    pthread_mutex_unlock(&fb_mutex);
}

// 绘制一个圆点（根据笔触大小）
static void draw_circle_point(struct FramebufferInfo* fb, int x, int y, uint32_t color, int radius) {
    // 根据半径绘制圆形区域
    for (int dy = -radius; dy <= radius; dy++) {
        for (int dx = -radius; dx <= radius; dx++) {
            // 检查是否在圆形内
            if (dx * dx + dy * dy <= radius * radius) {
                draw_pixel(fb, x + dx, y + dy, color);
            }
        }
    }
}

// 绘制一条线（Bresenham算法）
static void draw_line(struct FramebufferInfo* fb, int x0, int y0, int x1, int y1, uint32_t color) {
    int dx = abs(x1 - x0);
    int dy = abs(y1 - y0);
    int sx = (x0 < x1) ? 1 : -1;
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx - dy;
    
    while (1) {
        draw_pixel(fb, x0, y0, color);
        
        if (x0 == x1 && y0 == y1) break;
        
        int e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x0 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y0 += sy;
        }
    }
}

// 返回主页回调
static void touch_draw_back_cb(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    
    // 停止触摸绘图
    touch_draw_cleanup();
    
    // 返回到进入时的页面
    extern lv_obj_t* get_main_page1_screen(void);
    extern lv_obj_t* get_main_page2_screen(void);
    extern void switch_to_page(int);
    
    lv_obj_t *target_screen = (saved_page_index == 0) ? get_main_page1_screen() : get_main_page2_screen();
    if (target_screen) {
        // 切换到保存的页面
        extern int get_current_page_index(void);
        int current = get_current_page_index();
        if (current != saved_page_index) {
            switch_to_page(saved_page_index);
        } else {
            lv_obj_clear_flag(target_screen, LV_OBJ_FLAG_HIDDEN);
            lv_scr_load(target_screen);
        }
        
        // 强制刷新
        lv_refr_now(NULL);
        printf("[触摸绘图] 返回到页面 %d\n", saved_page_index);
    }
}

// 笔触大小选择回调（通过用户数据传递大小索引）
static void pen_size_select_cb(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    
    lv_obj_t *btn = lv_event_get_target(e);
    int size_idx = (int)(intptr_t)lv_obj_get_user_data(btn);
    pen_size = size_idx + 1;  // 1=细，2=中，3=粗
    printf("[触摸绘图] 笔触大小切换为: %d\n", pen_size);
    
    // 更新所有笔触大小按钮的选中状态
    for (int i = 0; i < 3; i++) {
        if (pen_size_btns[i]) {
            if (i == size_idx) {
                // 选中状态：深色边框
                lv_obj_set_style_border_width(pen_size_btns[i], 3, 0);
                lv_obj_set_style_border_color(pen_size_btns[i], lv_color_hex(0x0000FF), 0);
            } else {
                // 未选中状态：浅色边框
                lv_obj_set_style_border_width(pen_size_btns[i], 2, 0);
                lv_obj_set_style_border_color(pen_size_btns[i], lv_color_hex(0xCCCCCC), 0);
            }
        }
    }
}

// 颜色选择回调（通过用户数据传递颜色索引）
static void color_select_cb(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    
    lv_obj_t *btn = lv_event_get_target(e);
    int color_idx = (int)(intptr_t)lv_obj_get_user_data(btn);
    current_color_index = color_idx;
    
    // 退出橡皮擦模式
    eraser_mode = false;
    if (eraser_btn) {
        lv_obj_set_style_bg_color(eraser_btn, lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_border_color(eraser_btn, lv_color_hex(0xCCCCCC), 0);
    }
    
    printf("[触摸绘图] 颜色切换为: %d\n", current_color_index);
    
    // 更新所有颜色按钮的选中状态
    for (int i = 0; i < color_count; i++) {
        if (color_btns[i]) {
            if (i == color_idx) {
                // 选中状态：加粗边框
                lv_obj_set_style_border_width(color_btns[i], 4, 0);
                lv_obj_set_style_border_color(color_btns[i], lv_color_hex(0x000000), 0);
            } else {
                // 未选中状态：细边框
                lv_obj_set_style_border_width(color_btns[i], 2, 0);
                lv_obj_set_style_border_color(color_btns[i], lv_color_hex(0xCCCCCC), 0);
            }
        }
    }
}

// 橡皮擦切换回调
static void eraser_toggle_cb(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    
    eraser_mode = !eraser_mode;
    printf("[触摸绘图] 橡皮擦模式: %s\n", eraser_mode ? "开启" : "关闭");
    
    // 更新按钮样式
    lv_obj_t *btn = lv_event_get_target(e);
    if (eraser_mode) {
        lv_obj_set_style_bg_color(btn, lv_color_hex(0xFFE0E0), 0);  // 浅红色表示激活
        lv_obj_set_style_border_width(btn, 3, 0);
        lv_obj_set_style_border_color(btn, lv_color_hex(0xFF0000), 0);  // 红色边框
    } else {
        lv_obj_set_style_bg_color(btn, lv_color_hex(0xFFFFFF), 0);  // 白色表示未激活
        lv_obj_set_style_border_width(btn, 2, 0);
        lv_obj_set_style_border_color(btn, lv_color_hex(0xCCCCCC), 0);  // 灰色边框
    }
}

// 清屏回调
static void clear_screen_cb(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    
    printf("[触摸绘图] 清屏\n");
    
    // 在主线程中清屏framebuffer
    // 如果线程已经运行，使用已映射的framebuffer
    if (fb_info.fbp && fb_info.fbp != MAP_FAILED) {
        pthread_mutex_lock(&fb_mutex);
        uint32_t* fb_ptr = (uint32_t*)fb_info.fbp;
        // 清屏绘图区域（保留顶部、底部和右侧工具栏）
        int top_bar = 60;      // 顶部区域
        int bottom_bar = 80;   // 底部工具栏
        int right_bar = 80;    // 右侧工具栏
        int right_start_x = fb_info.vinfo.xres - right_bar;
        // 只清空中间绘图区域（排除右侧工具栏）
        for (int y = top_bar; y < fb_info.vinfo.yres - bottom_bar; y++) {
            for (int x = 0; x < right_start_x; x++) {
                int pixel_idx = y * fb_info.vinfo.xres + x;
                fb_ptr[pixel_idx] = COLOR_WHITE;
            }
        }
        msync(fb_info.fbp, fb_info.screensize, MS_SYNC);
        pthread_mutex_unlock(&fb_mutex);
        printf("[触摸绘图] 清屏完成\n");
        // 刷新LVGL显示，确保按钮位置正确
        lv_refr_now(NULL);
    } else {
        // 如果framebuffer还未映射，直接打开并清屏
        int fb_fd = open(FRAMEBUFFER_DEV, O_RDWR);
        if (fb_fd >= 0) {
            struct fb_var_screeninfo vinfo;
            struct fb_fix_screeninfo finfo;
            
            if (ioctl(fb_fd, FBIOGET_VSCREENINFO, &vinfo) == 0 &&
                ioctl(fb_fd, FBIOGET_FSCREENINFO, &finfo) == 0) {
                long int screensize = vinfo.yres * finfo.line_length;
                char *fbp = (char *)mmap(0, screensize, PROT_READ | PROT_WRITE, MAP_SHARED, fb_fd, 0);
                
                if ((intptr_t)fbp != -1) {
                    uint32_t* fb_ptr = (uint32_t*)fbp;
                    int top_bar = 60;      // 顶部区域
                    int bottom_bar = 80;   // 底部工具栏
                    int right_bar = 80;    // 右侧工具栏
                    int right_start_x = vinfo.xres - right_bar;
                    // 只清空中间绘图区域（排除右侧工具栏）
                    for (int y = top_bar; y < vinfo.yres - bottom_bar; y++) {
                        for (int x = 0; x < right_start_x; x++) {
                            int pixel_idx = y * vinfo.xres + x;
                            fb_ptr[pixel_idx] = COLOR_WHITE;
                        }
                    }
                    msync(fbp, screensize, MS_SYNC);
                    munmap(fbp, screensize);
                    printf("[触摸绘图] 清屏完成（临时映射）\n");
                }
            }
            close(fb_fd);
        }
    }
}

// 触摸绘图线程函数
static void* touch_draw_thread_func(void* arg) {
    struct input_event ev;
    enum TouchState touch_state = TOUCH_IDLE;
    
    // 触摸坐标和上一个触摸坐标（用于画线）
    int touch_x = 0, touch_y = 0;
    int last_screen_x = 0, last_screen_y = 0;
    int is_first_point = 1;
    
    printf("[触摸绘图] 线程启动\n");
    
    // 打开触摸屏设备
    touch_fd = open(TOUCH_DEVICE, O_RDONLY);
    if (touch_fd == -1) {
        perror("[触摸绘图] Error opening touch device");
        touch_draw_running = false;
        return NULL;
    }
    
    // 初始化帧缓冲区
    fb_info.fd = open(FRAMEBUFFER_DEV, O_RDWR);
    if (fb_info.fd == -1) {
        perror("[触摸绘图] Error opening framebuffer");
        close(touch_fd);
        touch_fd = -1;
        touch_draw_running = false;
        return NULL;
    }
    
    // 获取屏幕信息
    if (ioctl(fb_info.fd, FBIOGET_FSCREENINFO, &fb_info.finfo)) {
        perror("[触摸绘图] Error reading fixed framebuffer info");
        close(touch_fd);
        close(fb_info.fd);
        touch_fd = -1;
        touch_draw_running = false;
        return NULL;
    }
    
    if (ioctl(fb_info.fd, FBIOGET_VSCREENINFO, &fb_info.vinfo)) {
        perror("[触摸绘图] Error reading variable framebuffer info");
        close(touch_fd);
        close(fb_info.fd);
        touch_fd = -1;
        touch_draw_running = false;
        return NULL;
    }
    
    printf("[触摸绘图] Framebuffer info:\n");
    printf("  Resolution: %dx%d\n", fb_info.vinfo.xres, fb_info.vinfo.yres);
    printf("  Bits per pixel: %d\n", fb_info.vinfo.bits_per_pixel);
    printf("  Line length: %d bytes\n", fb_info.finfo.line_length);
    
    // 映射帧缓冲区到内存
    fb_info.screensize = fb_info.vinfo.yres * fb_info.finfo.line_length;
    fb_info.fbp = (char*)mmap(0, fb_info.screensize, 
                             PROT_READ | PROT_WRITE, 
                             MAP_SHARED, fb_info.fd, 0);
    
    if (fb_info.fbp == MAP_FAILED) {
        perror("[触摸绘图] Error mapping framebuffer");
        close(touch_fd);
        close(fb_info.fd);
        touch_fd = -1;
        touch_draw_running = false;
        return NULL;
    }
    
    // 等待一小段时间，确保LVGL完成初始渲染
    usleep(200000);  // 200ms，确保LVGL完成窗口显示
    
    // 清屏为白色（只清屏顶部、底部和右侧工具栏以外的区域）
    // 注意：主线程已经清屏过了，这里只是为了确保，或者处理线程重新启动的情况
    pthread_mutex_lock(&fb_mutex);
    uint32_t* fb_ptr = (uint32_t*)fb_info.fbp;
    int top_bar = 60;      // 顶部区域
    int bottom_bar = 80;   // 底部工具栏
    int right_bar = 80;    // 右侧工具栏
    int right_start_x = fb_info.vinfo.xres - right_bar;
    // 只清空中间绘图区域（排除右侧工具栏）
    for (int y = top_bar; y < fb_info.vinfo.yres - bottom_bar; y++) {
        for (int x = 0; x < right_start_x; x++) {
            int pixel_idx = y * fb_info.vinfo.xres + x;
            fb_ptr[pixel_idx] = COLOR_WHITE;
        }
    }
    msync(fb_info.fbp, fb_info.screensize, MS_SYNC);
    pthread_mutex_unlock(&fb_mutex);
    
    printf("[触摸绘图] Framebuffer已清屏为白色（保留顶部区域）\n");
    printf("[触摸绘图] 触摸绘图程序已启动\n");
    printf("  Screen size: %dx%d\n", SCREEN_WIDTH, SCREEN_HEIGHT);
    printf("  Touch range: X[%d-%d], Y[%d-%d]\n", 
           touch_min_x, touch_max_x, touch_min_y, touch_max_y);
    
    // 主事件循环
    while (touch_draw_running) {
        // 读取触摸事件
        ssize_t n = read(touch_fd, &ev, sizeof(struct input_event));
        if (n == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                usleep(10000);  // 等待10ms
                continue;
            }
            if (errno == EINTR) {
                // 被信号中断，继续
                continue;
            }
            perror("[触摸绘图] Error reading touch event");
            break;
        }
        
        if (n != sizeof(struct input_event)) {
            continue;
        }
        
        // 处理触摸事件
        if (ev.type == EV_ABS) {
            if (ev.code == ABS_X) {
                touch_x = ev.value;
            }
            if (ev.code == ABS_Y) {
                touch_y = ev.value;
            }
        }
        else if (ev.type == EV_KEY && ev.code == BTN_TOUCH) {
            // 在按下时就检查是否在工具栏区域
            if (ev.value) {  // 触摸按下
                // 先检查触摸位置是否在工具栏区域
                int screen_x, screen_y;
                screen_x = (touch_x - touch_min_x) * SCREEN_WIDTH / (touch_max_x - touch_min_x);
                screen_y = (touch_y - touch_min_y) * SCREEN_HEIGHT / (touch_max_y - touch_min_y);
                
                // 边界检查
                if (screen_x < 0) screen_x = 0;
                if (screen_x >= SCREEN_WIDTH) screen_x = SCREEN_WIDTH - 1;
                if (screen_y < 0) screen_y = 0;
                if (screen_y >= SCREEN_HEIGHT) screen_y = SCREEN_HEIGHT - 1;
                
                // 检查是否在工具栏区域（顶部、底部、右侧）
                bool in_toolbar = false;
                // 顶部工具栏：0-59像素（包括返回按钮区域）
                if (screen_y < 60) {
                    in_toolbar = true;
                }
                // 底部工具栏：400-479像素（480-80=400），包括颜色按钮区域
                else if (screen_y >= 400) {
                    in_toolbar = true;
                }
                // 右侧工具栏：720-799像素（800-80=720），高度280像素（从60像素开始到340像素）
                else if (screen_x >= 720 && screen_y >= 60 && screen_y < 340) {
                    in_toolbar = true;
                }
                
                if (in_toolbar) {
                    // 在工具栏区域，不设置触摸状态，直接跳过
                    printf("[触摸绘图] Touch in toolbar area, ignored: (%d, %d)\n", screen_x, screen_y);
                    continue;
                }
                
                touch_state = TOUCH_PRESSED;
                printf("[触摸绘图] Touch pressed at: (%d, %d)\n", touch_x, touch_y);
                
                // 记录第一个点
                is_first_point = 1;
            } else {        // 触摸释放
                touch_state = TOUCH_IDLE;
                printf("[触摸绘图] Touch released\n");
            }
        }
        else if (ev.type == EV_SYN && ev.code == SYN_REPORT) {
            // 同步事件，可以在这里进行绘制
            if (touch_state == TOUCH_PRESSED || touch_state == TOUCH_MOVING) {
                // 将触摸坐标映射到屏幕坐标
                int screen_x, screen_y;
                
                // X轴映射：触摸范围 [0, 1024] -> 屏幕范围 [0, 800]
                screen_x = (touch_x - touch_min_x) * SCREEN_WIDTH / (touch_max_x - touch_min_x);
                
                // Y轴映射：触摸范围 [0, 600] -> 屏幕范围 [0, 480]
                screen_y = (touch_y - touch_min_y) * SCREEN_HEIGHT / (touch_max_y - touch_min_y);
                
                // 边界检查
                if (screen_x < 0) screen_x = 0;
                if (screen_x >= SCREEN_WIDTH) screen_x = SCREEN_WIDTH - 1;
                if (screen_y < 0) screen_y = 0;
                if (screen_y >= SCREEN_HEIGHT) screen_y = SCREEN_HEIGHT - 1;
                
                // 再次检查是否在工具栏区域（顶部、底部、右侧）- 如果在工具栏区域则不绘制
                bool in_toolbar = false;
                // 顶部工具栏：0-59像素（包括返回按钮区域）
                if (screen_y < 60) {
                    in_toolbar = true;
                }
                // 底部工具栏：400-479像素（480-80=400），包括颜色按钮区域
                else if (screen_y >= 400) {
                    in_toolbar = true;
                }
                // 右侧工具栏：720-799像素（800-80=720），高度280像素（从60像素开始到340像素）
                else if (screen_x >= 720 && screen_y >= 60 && screen_y < 340) {
                    in_toolbar = true;
                }
                
                if (in_toolbar) {
                    // 在工具栏区域，不绘制，重置触摸状态并跳过
                    touch_state = TOUCH_IDLE;
                    continue;
                }
                
                // 获取当前绘制颜色（橡皮擦模式使用白色，否则使用当前选择的颜色）
                uint32_t draw_color = eraser_mode ? COLOR_WHITE : color_list[current_color_index];
                
                // 根据笔触大小决定绘制方式
                int radius = pen_size;  // 1=细(半径1), 2=中(半径2), 3=粗(半径3)
                
                // 绘制点
                if (is_first_point) {
                    // 如果是第一个点，绘制一个圆点
                    draw_circle_point(&fb_info, screen_x, screen_y, draw_color, radius);
                    is_first_point = 0;
                } else {
                    // 绘制从上一点到当前点的线，使用粗线条（根据笔触大小）
                    // 对于粗线条，在线上每隔一段距离绘制一个圆点
                    int dx = abs(screen_x - last_screen_x);
                    int dy = abs(screen_y - last_screen_y);
                    int steps = (dx > dy ? dx : dy) + 1;
                    
                    for (int i = 0; i <= steps; i++) {
                        int px = last_screen_x + (screen_x - last_screen_x) * i / steps;
                        int py = last_screen_y + (screen_y - last_screen_y) * i / steps;
                        draw_circle_point(&fb_info, px, py, draw_color, radius);
                    }
                }
                
                // 同步framebuffer到显示（确保绘制立即显示，已加锁保护）
                pthread_mutex_lock(&fb_mutex);
                msync(fb_info.fbp, fb_info.screensize, MS_SYNC);
                pthread_mutex_unlock(&fb_mutex);
                
                // 更新上一个点的坐标
                last_screen_x = screen_x;
                last_screen_y = screen_y;
                
                touch_state = TOUCH_MOVING;
            }
        }
    }
    
    // 清理资源
    printf("[触摸绘图] 清理资源...\n");
    if (fb_info.fbp != MAP_FAILED && fb_info.fbp != NULL) {
        munmap(fb_info.fbp, fb_info.screensize);
        fb_info.fbp = NULL;
    }
    if (touch_fd >= 0) {
        close(touch_fd);
        touch_fd = -1;
    }
    if (fb_info.fd >= 0) {
        close(fb_info.fd);
        fb_info.fd = -1;
    }
    
    printf("[触摸绘图] 线程退出\n");
    return NULL;
}

/**
 * @brief 显示触摸绘图窗口
 */
void touch_draw_win_show(void) {
    extern const lv_font_t SourceHanSansSC_VF;
    
    if (touch_draw_window != NULL) {
        // 先确保触摸绘图模式未激活，允许LVGL正常刷新整个窗口
        touch_draw_running = false;
        
        lv_obj_clear_flag(touch_draw_window, LV_OBJ_FLAG_HIDDEN);
        lv_scr_load(touch_draw_window);
        
        // 等待LVGL完成渲染整个窗口（包括按钮和标题）
        for (int i = 0; i < 20; i++) {
            lv_timer_handler();
            usleep(10000);  // 10ms
        }
        lv_refr_now(NULL);
        
        // 额外等待，确保LVGL刷新完成
        usleep(50000);  // 50ms
        
        // 在LVGL刷新完成后，再设置触摸绘图模式为激活（禁用LVGL刷新）
        touch_draw_running = true;
        printf("[触摸绘图] 触摸绘图模式已激活，LVGL刷新已禁用\n");
        
        // 然后清屏framebuffer为白色（只清屏60像素以下的区域，保留按钮和标题）
        int fb_fd = open(FRAMEBUFFER_DEV, O_RDWR);
        if (fb_fd >= 0) {
            struct fb_var_screeninfo vinfo;
            struct fb_fix_screeninfo finfo;
            
            if (ioctl(fb_fd, FBIOGET_VSCREENINFO, &vinfo) == 0 &&
                ioctl(fb_fd, FBIOGET_FSCREENINFO, &finfo) == 0) {
                long int screensize = vinfo.yres * finfo.line_length;
                char *fbp = (char *)mmap(0, screensize, PROT_READ | PROT_WRITE, MAP_SHARED, fb_fd, 0);
                
                if ((intptr_t)fbp != -1) {
                    // 清屏为白色（只清屏顶部、底部和右侧工具栏以外的区域）
                    uint32_t* fb_ptr = (uint32_t*)fbp;
                    int top_bar = 60;      // 顶部区域
                    int bottom_bar = 80;   // 底部工具栏
                    int right_bar = 80;    // 右侧工具栏
                    int right_start_x = vinfo.xres - right_bar;
                    // 只清空中间绘图区域（排除右侧工具栏）
                    for (int y = top_bar; y < vinfo.yres - bottom_bar; y++) {
                        for (int x = 0; x < right_start_x; x++) {
                            int pixel_idx = y * vinfo.xres + x;
                            fb_ptr[pixel_idx] = COLOR_WHITE;
                        }
                    }
                    msync(fbp, screensize, MS_SYNC);
                    munmap(fbp, screensize);
                    printf("[触摸绘图] Framebuffer已清屏为白色（保留顶部区域）\n");
                }
            }
            close(fb_fd);
        }
        
        // 检查线程是否存活，如果不存在或已退出，创建新线程
        // 使用pthread_kill检查线程是否存活（发送0信号不会杀死线程）
        int thread_alive = 0;
        if (touch_draw_thread != 0) {
            thread_alive = pthread_kill(touch_draw_thread, 0);
        }
        
        if (thread_alive != 0 || touch_draw_thread == 0) {
            // 线程不存在或已退出，重置线程ID并创建新线程
            touch_draw_thread = 0;
            // 确保之前的资源已清理
            if (touch_fd >= 0) {
                close(touch_fd);
                touch_fd = -1;
            }
            if (fb_info.fbp != MAP_FAILED && fb_info.fbp != NULL) {
                munmap(fb_info.fbp, fb_info.screensize);
                fb_info.fbp = NULL;
            }
            if (fb_info.fd >= 0) {
                close(fb_info.fd);
                fb_info.fd = -1;
            }
            
            // 创建新线程
            if (pthread_create(&touch_draw_thread, NULL, touch_draw_thread_func, NULL) != 0) {
                perror("[触摸绘图] Failed to create thread");
                touch_draw_running = false;
            } else {
                printf("[触摸绘图] 线程已启动（重新创建）\n");
            }
            // 等待线程初始化完成
            usleep(100000);  // 100ms，等待线程完成初始化
        } else {
            // 线程还在运行，但需要确保framebuffer已映射
            // 如果framebuffer未映射，需要重新映射（这种情况不应该发生，但为了安全）
            if (fb_info.fbp == NULL || fb_info.fbp == MAP_FAILED) {
                printf("[触摸绘图] 警告：线程运行中但framebuffer未映射，重新映射\n");
                // 这种情况下应该重新创建线程
                touch_draw_running = false;
                pthread_join(touch_draw_thread, NULL);
                touch_draw_thread = 0;
                touch_draw_running = true;
                if (pthread_create(&touch_draw_thread, NULL, touch_draw_thread_func, NULL) != 0) {
                    perror("[触摸绘图] Failed to recreate thread");
                    touch_draw_running = false;
                } else {
                    printf("[触摸绘图] 线程已重新创建\n");
                }
                usleep(100000);
            }
        }
        return;
    }
    
    // 创建窗口
    touch_draw_window = lv_obj_create(NULL);
    lv_obj_set_size(touch_draw_window, 800, 480);
    // 使用透明背景，让framebuffer直接显示，避免LVGL刷新覆盖
    lv_obj_set_style_bg_opa(touch_draw_window, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_opa(touch_draw_window, LV_OPA_TRANSP, 0);
    
    // 创建标题
    lv_obj_t *title = lv_label_create(touch_draw_window);
    lv_label_set_text(title, "触摸绘图");
    lv_obj_set_style_text_font(title, &SourceHanSansSC_VF, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0x000000), 0);  // 黑色文字（白色背景）
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);
    
    // 创建返回按钮（同一层级，不使用move_foreground）
    lv_obj_t *back_btn = lv_btn_create(touch_draw_window);
    lv_obj_set_size(back_btn, 80, 40);
    lv_obj_set_style_bg_color(back_btn, lv_color_hex(0x9E9E9E), 0);
    lv_obj_align(back_btn, LV_ALIGN_TOP_LEFT, 10, 10);
    lv_obj_t *back_label = lv_label_create(back_btn);
    lv_label_set_text(back_label, "返回");
    lv_obj_set_style_text_font(back_label, &SourceHanSansSC_VF, 0);
    lv_obj_center(back_label);
    lv_obj_add_event_cb(back_btn, touch_draw_back_cb, LV_EVENT_CLICKED, NULL);
    
    // 创建底部工具栏容器（左侧用于颜色选择）- 同一层级
    lv_obj_t *toolbar = lv_obj_create(touch_draw_window);
    lv_obj_set_size(toolbar, 800, 80);
    lv_obj_align(toolbar, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(toolbar, lv_color_hex(0xF5F5F5), 0);
    lv_obj_set_style_border_width(toolbar, 2, 0);
    lv_obj_set_style_border_color(toolbar, lv_color_hex(0xCCCCCC), 0);
    lv_obj_set_style_pad_all(toolbar, 8, 0);
    
    // 工具栏按钮参数
    int btn_size = 60;  // 按钮大小（正方形）
    int btn_spacing = 10;
    int start_x = 20;  // 工具栏起始位置
    
    // 创建6个颜色按钮（水平排列在底部工具栏）
    // ARGB颜色（用于LVGL显示）
    uint32_t colors_lvgl[] = {
        0xFFFF0000,  // 红色
        0xFF00FF00,  // 绿色
        0xFF0000FF,  // 蓝色
        0xFFFFFF00,  // 黄色
        0xFF000000,  // 黑色
        0xFF808080,  // 灰色
    };
    
    for (int i = 0; i < color_count; i++) {
        lv_obj_t *btn = lv_btn_create(toolbar);
        lv_obj_set_size(btn, btn_size, btn_size);
        lv_obj_set_style_bg_color(btn, lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_border_width(btn, (i == 0) ? 4 : 2, 0);  // 默认选中红色
        lv_obj_set_style_border_color(btn, (i == 0) ? lv_color_hex(0x000000) : lv_color_hex(0xCCCCCC), 0);
        lv_obj_align(btn, LV_ALIGN_LEFT_MID, start_x + i * (btn_size + btn_spacing), 0);
        
        // 颜色预览（圆形）
        lv_obj_t *preview = lv_obj_create(btn);
        lv_obj_set_size(preview, 45, 45);
        // 使用RGB颜色值（lv_color_hex只需要RGB，去掉alpha通道）
        uint32_t rgb_color = colors_lvgl[i] & 0x00FFFFFF;  // 去掉alpha通道
        lv_obj_set_style_bg_color(preview, lv_color_hex(rgb_color), 0);
        lv_obj_set_style_radius(preview, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_border_width(preview, 0, 0);
        lv_obj_clear_flag(preview, LV_OBJ_FLAG_CLICKABLE);  // 预览不拦截点击事件
        lv_obj_center(preview);
        
        color_btns[i] = btn;
        lv_obj_set_user_data(btn, (void*)(intptr_t)i);  // 存储索引
        lv_obj_add_event_cb(btn, color_select_cb, LV_EVENT_CLICKED, NULL);
    }
    
    // 创建右侧竖列工具栏（笔触大小）- 同一层级
    int right_btn_size = 60;
    int right_btn_spacing = 15;
    int right_toolbar_height = 280;  // 稍微拉长到280像素
    lv_obj_t *right_toolbar = lv_obj_create(touch_draw_window);
    lv_obj_set_size(right_toolbar, 80, right_toolbar_height);
    lv_obj_align(right_toolbar, LV_ALIGN_TOP_RIGHT, 0, 60);  // 从顶部工具栏下方开始，右侧对齐
    lv_obj_set_style_bg_color(right_toolbar, lv_color_hex(0xF5F5F5), 0);
    lv_obj_set_style_border_width(right_toolbar, 2, 0);
    lv_obj_set_style_border_color(right_toolbar, lv_color_hex(0xCCCCCC), 0);
    lv_obj_set_style_pad_all(right_toolbar, 8, 0);
    
    // 右侧工具栏按钮参数
    int right_start_y = 8;  // 从工具栏顶部padding开始
    
    // 创建3个笔触大小按钮（细、中、粗）- 竖列
    int pen_sizes[] = {8, 15, 22};  // 预览圆点大小
    for (int i = 0; i < 3; i++) {
        lv_obj_t *btn = lv_btn_create(right_toolbar);
        lv_obj_set_size(btn, right_btn_size, right_btn_size);
        lv_obj_set_style_bg_color(btn, lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_border_width(btn, (i == 1) ? 3 : 2, 0);  // 默认选中"中"
        lv_obj_set_style_border_color(btn, (i == 1) ? lv_color_hex(0x0000FF) : lv_color_hex(0xCCCCCC), 0);
        lv_obj_align(btn, LV_ALIGN_TOP_MID, 0, right_start_y + i * (right_btn_size + right_btn_spacing));
        
        // 笔触大小预览（圆形点）
        lv_obj_t *preview = lv_obj_create(btn);
        lv_obj_set_size(preview, pen_sizes[i], pen_sizes[i]);
        lv_obj_set_style_bg_color(preview, lv_color_hex(0x000000), 0);
        lv_obj_set_style_radius(preview, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_border_width(preview, 0, 0);
        lv_obj_clear_flag(preview, LV_OBJ_FLAG_CLICKABLE);  // 预览不拦截点击事件
        lv_obj_center(preview);
        
        pen_size_btns[i] = btn;
        lv_obj_set_user_data(btn, (void*)(intptr_t)i);  // 存储索引
        lv_obj_add_event_cb(btn, pen_size_select_cb, LV_EVENT_CLICKED, NULL);
    }
    
    // 橡皮擦按钮（底部工具栏右侧）
    int right_start_x = 800 - 20 - 2 * (btn_size + btn_spacing);  // 从右侧开始，留2个按钮的空间
    eraser_btn = lv_btn_create(toolbar);
    lv_obj_set_size(eraser_btn, btn_size, btn_size);
    lv_obj_set_style_bg_color(eraser_btn, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_border_width(eraser_btn, 2, 0);
    lv_obj_set_style_border_color(eraser_btn, lv_color_hex(0xCCCCCC), 0);
    lv_obj_align(eraser_btn, LV_ALIGN_LEFT_MID, right_start_x, 0);
    
    // 橡皮擦图标（使用FontAwesome橡皮擦图标 0xF12D）
    lv_obj_t *eraser_icon = lv_label_create(eraser_btn);
    extern const lv_font_t fa_solid_24;
    lv_label_set_text(eraser_icon, "\xEF\x84\xAD");  // FontAwesome eraser icon (U+F12D, UTF-8: \xEF\x84\xAD)
    lv_obj_set_style_text_font(eraser_icon, &fa_solid_24, 0);
    lv_obj_set_style_text_color(eraser_icon, lv_color_hex(0x666666), 0);
    lv_obj_set_style_text_align(eraser_icon, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(eraser_icon, LV_PCT(100));
    lv_obj_center(eraser_icon);
    lv_obj_clear_flag(eraser_icon, LV_OBJ_FLAG_CLICKABLE);  // 图标不拦截点击事件
    lv_obj_add_event_cb(eraser_btn, eraser_toggle_cb, LV_EVENT_CLICKED, NULL);
    
    // 清屏按钮（底部工具栏右侧，橡皮擦旁边）
    lv_obj_t *clear_btn = lv_btn_create(toolbar);
    lv_obj_set_size(clear_btn, btn_size, btn_size);
    lv_obj_set_style_bg_color(clear_btn, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_border_width(clear_btn, 2, 0);
    lv_obj_set_style_border_color(clear_btn, lv_color_hex(0xCCCCCC), 0);
    lv_obj_align(clear_btn, LV_ALIGN_LEFT_MID, right_start_x + btn_size + btn_spacing, 0);
    
    // 清屏图标（使用LVGL符号）
    lv_obj_t *clear_icon = lv_label_create(clear_btn);
    lv_label_set_text(clear_icon, LV_SYMBOL_TRASH);  // 使用LVGL内置垃圾桶图标
    lv_obj_set_style_text_font(clear_icon, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(clear_icon, lv_color_hex(0x666666), 0);
    lv_obj_set_style_text_align(clear_icon, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(clear_icon, LV_PCT(100));
    lv_obj_center(clear_icon);
    lv_obj_clear_flag(clear_icon, LV_OBJ_FLAG_CLICKABLE);  // 图标不拦截点击事件
    lv_obj_add_event_cb(clear_btn, clear_screen_cb, LV_EVENT_CLICKED, NULL);
    
    // 保存当前页面索引
    extern int get_current_page_index(void);
    saved_page_index = get_current_page_index();
    printf("[触摸绘图] 保存当前页面索引: %d\n", saved_page_index);
    
    // 先确保触摸绘图模式未激活，允许LVGL正常刷新整个窗口
    touch_draw_running = false;
    
    // 切换到窗口
    lv_scr_load(touch_draw_window);
    
    // 等待LVGL完成初始渲染整个窗口（包括按钮和标题）
    for (int i = 0; i < 20; i++) {
        lv_timer_handler();
        usleep(10000);  // 10ms
    }
    lv_refr_now(NULL);
    
    // 额外等待，确保LVGL刷新完成
    usleep(50000);  // 50ms
    
    // 在LVGL刷新完成后，再设置触摸绘图模式为激活（禁用LVGL刷新）
    touch_draw_running = true;
    printf("[触摸绘图] 触摸绘图模式已激活，LVGL刷新已禁用\n");
    
    // 然后清屏framebuffer为白色（只清屏60像素以下的区域，保留按钮和标题）
    int fb_fd = open(FRAMEBUFFER_DEV, O_RDWR);
    if (fb_fd >= 0) {
        struct fb_var_screeninfo vinfo;
        struct fb_fix_screeninfo finfo;
        
        if (ioctl(fb_fd, FBIOGET_VSCREENINFO, &vinfo) == 0 &&
            ioctl(fb_fd, FBIOGET_FSCREENINFO, &finfo) == 0) {
            long int screensize = vinfo.yres * finfo.line_length;
            char *fbp = (char *)mmap(0, screensize, PROT_READ | PROT_WRITE, MAP_SHARED, fb_fd, 0);
            
            if ((intptr_t)fbp != -1) {
                // 清屏为白色（只清屏顶部、底部和右侧工具栏以外的区域）
                uint32_t* fb_ptr = (uint32_t*)fbp;
                int top_bar = 60;      // 顶部区域
                int bottom_bar = 80;   // 底部工具栏
                int right_bar = 80;    // 右侧工具栏
                int right_start_x = vinfo.xres - right_bar;
                // 只清空中间绘图区域（排除右侧工具栏）
                for (int y = top_bar; y < vinfo.yres - bottom_bar; y++) {
                    for (int x = 0; x < right_start_x; x++) {
                        int pixel_idx = y * vinfo.xres + x;
                        fb_ptr[pixel_idx] = COLOR_WHITE;
                    }
                }
                msync(fbp, screensize, MS_SYNC);
                munmap(fbp, screensize);
                printf("[触摸绘图] Framebuffer已清屏为白色（保留顶部区域）\n");
            }
        }
        close(fb_fd);
    }
    
    // 启动触摸绘图线程（线程会接管framebuffer管理）
    // 注意：touch_draw_running已经设置为true，所以直接创建线程
    // 确保之前的线程已完全退出
    if (touch_draw_thread != 0) {
        int thread_alive = pthread_kill(touch_draw_thread, 0);
        if (thread_alive == 0) {
            // 线程还在运行，先停止它
            printf("[触摸绘图] 检测到旧线程仍在运行，先停止\n");
            touch_draw_running = false;
            pthread_join(touch_draw_thread, NULL);
            touch_draw_thread = 0;
            // 清理资源
            if (touch_fd >= 0) {
                close(touch_fd);
                touch_fd = -1;
            }
            if (fb_info.fbp != MAP_FAILED && fb_info.fbp != NULL) {
                munmap(fb_info.fbp, fb_info.screensize);
                fb_info.fbp = NULL;
            }
            if (fb_info.fd >= 0) {
                close(fb_info.fd);
                fb_info.fd = -1;
            }
            touch_draw_running = true;  // 重新设置
        }
    }
    
    if (pthread_create(&touch_draw_thread, NULL, touch_draw_thread_func, NULL) != 0) {
        perror("[触摸绘图] Failed to create thread");
        touch_draw_running = false;
    } else {
        printf("[触摸绘图] 线程已启动\n");
    }
    
    // 等待线程初始化完成
    usleep(100000);  // 100ms，等待线程完成初始化
}

/**
 * @brief 隐藏触摸绘图窗口
 */
void touch_draw_win_hide(void) {
    if (touch_draw_window != NULL) {
        lv_obj_add_flag(touch_draw_window, LV_OBJ_FLAG_HIDDEN);
    }
    touch_draw_cleanup();
}

/**
 * @brief 初始化触摸绘图模块
 */
void touch_draw_init(void) {
    if (touch_draw_running) {
        return;
    }
    
    touch_draw_running = true;
    
    // 创建线程
    if (pthread_create(&touch_draw_thread, NULL, touch_draw_thread_func, NULL) != 0) {
        perror("[触摸绘图] Failed to create thread");
        touch_draw_running = false;
        return;
    }
    
    printf("[触摸绘图] 模块初始化完成\n");
}

/**
 * @brief 清理触摸绘图模块
 */
void touch_draw_cleanup(void) {
    if (!touch_draw_running && touch_draw_thread == 0) {
        // 已经清理过了，直接返回
        return;
    }
    
    printf("[触摸绘图] 正在停止...\n");
    touch_draw_running = false;
    
    // 等待线程退出
    if (touch_draw_thread != 0) {
        if (pthread_join(touch_draw_thread, NULL) != 0) {
            perror("[触摸绘图] Failed to join thread");
        }
        touch_draw_thread = 0;
    }
    
    // 确保资源已清理（线程退出时会清理，但这里再次确认）
    if (touch_fd >= 0) {
        close(touch_fd);
        touch_fd = -1;
    }
    if (fb_info.fbp != MAP_FAILED && fb_info.fbp != NULL) {
        munmap(fb_info.fbp, fb_info.screensize);
        fb_info.fbp = NULL;
    }
    if (fb_info.fd >= 0) {
        close(fb_info.fd);
        fb_info.fd = -1;
    }
    
    printf("[触摸绘图] 模块清理完成\n");
}

/**
 * @brief 检查触摸绘图模式是否激活
 */
bool touch_draw_is_active(void) {
    return touch_draw_running;
}
