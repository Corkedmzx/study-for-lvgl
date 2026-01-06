/**
 * @file video_touch_control.c
 * @brief 视频播放触屏手势控制实现
 */

#include "video_touch_control.h"
#include "../media_player/simple_video_player.h"
#include "../ui/ui_screens.h"
#include "../common/touch_device.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <linux/input.h>
#include <errno.h>

// 屏幕尺寸（需要根据实际屏幕调整）
#define SCREEN_WIDTH 800
#define SCREEN_HEIGHT 480

// 控制区域大小（预留足够空间避免误操作）
#define CONTROL_AREA_SIZE 120

// 手势识别阈值
#define GESTURE_THRESHOLD 50  // 滑动最小距离

// 触屏控制状态
static bool control_active = false;
static pthread_t control_thread = 0;
static bool should_exit_control = false;

// 触摸状态
static bool touch_pressed = false;
static int touch_start_x = 0;
static int touch_start_y = 0;
static int touch_last_x = 0;
static int touch_last_y = 0;
static int touch_current_x = 0;
static int touch_current_y = 0;

// 注意：不再使用互斥锁，因为触摸状态变量只在触摸线程中使用，不需要保护
// 这样可以提高响应速度，避免锁竞争

/**
 * @brief 判断点是否在左上角控制区
 */
static bool is_top_left_area(int x, int y) {
    return (x < CONTROL_AREA_SIZE && y < CONTROL_AREA_SIZE);
}

/**
 * @brief 判断点是否在左下角控制区
 */
static bool is_bottom_left_area(int x, int y) {
    return (x < CONTROL_AREA_SIZE && y > (SCREEN_HEIGHT - CONTROL_AREA_SIZE));
}

/**
 * @brief 判断点是否在右下角控制区
 */
static bool is_bottom_right_area(int x, int y) {
    return (x > (SCREEN_WIDTH - CONTROL_AREA_SIZE) && y > (SCREEN_HEIGHT - CONTROL_AREA_SIZE));
}

/**
 * @brief 判断点是否在右上角控制区
 */
static bool is_top_right_area(int x, int y) {
    return (x > (SCREEN_WIDTH - CONTROL_AREA_SIZE) && y < CONTROL_AREA_SIZE);
}

/**
 * @brief 判断点是否在中间区域（用于滑动检测）
 */
static bool is_middle_area(int x, int y) {
    return (x >= CONTROL_AREA_SIZE && x <= (SCREEN_WIDTH - CONTROL_AREA_SIZE) &&
            y >= CONTROL_AREA_SIZE && y <= (SCREEN_HEIGHT - CONTROL_AREA_SIZE));
}

/**
 * @brief 处理点击事件
 */
static void handle_click(int x, int y) {
    printf("[触屏控制] 处理点击: (%d, %d)\n", x, y);
    
    if (is_top_left_area(x, y)) {
        // 左上：返回主页
        printf("[触屏控制] 左上角点击: 返回主页\n");
        // 先停止触屏控制线程（避免继续处理事件）
        video_touch_control_stop();
        // 等待触屏控制线程完全停止
        usleep(150000);  // 150ms
        
        // 检查视频是否还在播放，如果还在播放才需要停止
        extern bool simple_video_is_playing(void);
        if (simple_video_is_playing()) {
            // 立即强制停止mplayer（使用SIGTERM，参考video.c的简单方式）
            extern void simple_video_force_stop(void);
            simple_video_force_stop();
            // 等待MPlayer完全退出和framebuffer恢复（增加等待时间，确保framebuffer完全恢复）
            usleep(500000);  // 500ms，确保framebuffer已完全恢复
        } else {
            printf("[触屏控制] 视频已停止，无需再次停止\n");
            // 即使视频已停止，也等待一下确保framebuffer稳定
            usleep(200000);  // 200ms
        }
        
        // 设置标志，让主线程处理返回主页（不在触屏控制线程中操作LVGL，避免线程安全问题）
        extern bool need_return_to_main;
        need_return_to_main = true;
    } else if (is_bottom_left_area(x, y)) {
        // 左下：上一首（即使视频已结束也可以切换）
        printf("[触屏控制] 左下角点击: 上一首\n");
        // 检查视频是否在播放，如果已停止则直接切换
        extern bool simple_video_is_playing(void);
        if (!simple_video_is_playing()) {
            printf("[触屏控制] 当前视频已结束，切换到上一首\n");
        }
        simple_video_prev();
    } else if (is_bottom_right_area(x, y)) {
        // 右下：下一首视频（即使视频已结束也可以切换）
        printf("[触屏控制] 右下角点击: 下一首视频\n");
        // 检查视频是否在播放，如果已停止则直接切换
        extern bool simple_video_is_playing(void);
        if (!simple_video_is_playing()) {
            printf("[触屏控制] 当前视频已结束，切换到下一首\n");
        }
        simple_video_next();
    } else if (is_top_right_area(x, y)) {
        // 右上：暂停/恢复（只有视频播放时才有效）
        printf("[触屏控制] 右上角点击: 暂停/恢复\n");
        extern bool simple_video_is_playing(void);
        if (simple_video_is_playing()) {
            simple_video_toggle_pause();
        } else {
            printf("[触屏控制] 视频已停止，无法暂停/恢复\n");
        }
    } else {
        printf("[触屏控制] 点击位置不在控制区域内\n");
    }
}

/**
 * @brief 处理滑动事件
 */
static void handle_swipe(int start_x, int start_y, int end_x, int end_y) {
    int dx = end_x - start_x;
    int dy = end_y - start_y;
    int abs_dx = abs(dx);
    int abs_dy = abs(dy);
    
    printf("[触屏控制] 处理滑动: 从 (%d, %d) 到 (%d, %d), 偏移: (%d, %d)\n", 
           start_x, start_y, end_x, end_y, dx, dy);
    
    // 对于垂直滑动（音量控制），放宽区域限制，只要滑动距离足够大就处理
    // 这样可以支持从屏幕边缘开始滑动
    bool is_vertical_swipe = (abs_dy > GESTURE_THRESHOLD && abs_dy > abs_dx * 2);
    bool is_horizontal_swipe = (abs_dx > GESTURE_THRESHOLD && abs_dx > abs_dy * 2);
    
    // 水平滑动（速度控制）需要起点在中间区域
    if (is_horizontal_swipe && !is_middle_area(start_x, start_y)) {
        printf("[触屏控制] 水平滑动起点不在中间区域，忽略\n");
        return;
    }
    
    // 垂直滑动（音量控制）只要滑动距离足够大就处理，不限制起点位置
    // 这样可以支持从屏幕任何位置开始滑动来调节音量
    
    // 判断滑动方向（需要单向滑动，主要方向距离必须大于次要方向的2倍）
    if (abs_dx > GESTURE_THRESHOLD && abs_dx > abs_dy * 2) {
        // 水平滑动（水平距离是垂直距离的2倍以上）
        if (dx > 0) {
            // 右滑：加速
            printf("[触屏控制] 右滑: 加速\n");
            simple_video_speed_up();
        } else {
            // 左滑：减速
            printf("[触屏控制] 左滑: 减速\n");
            simple_video_speed_down();
        }
    } else if (abs_dy > GESTURE_THRESHOLD && abs_dy > abs_dx * 2) {
        // 垂直滑动（垂直距离是水平距离的2倍以上）
        if (dy < 0) {
            // 上划：加音量
            printf("[触屏控制] 上划: 加音量\n");
            simple_video_volume_up();
        } else {
            // 下划：减音量
            printf("[触屏控制] 下划: 减音量\n");
            simple_video_volume_down();
        }
    } else {
        printf("[触屏控制] 滑动距离不足或斜向滑动，忽略 (abs_dx=%d, abs_dy=%d, threshold=%d)\n", 
               abs_dx, abs_dy, GESTURE_THRESHOLD);
    }
}

/**
 * @brief 触屏控制线程（直接读取/dev/input/event0）
 */
static void *control_thread_func(void *arg) {
    struct input_event touch;
    int ret;
    
    // 使用统一的触摸屏设备（已在程序启动时打开）
    int touch_fd = touch_device_get_fd();
    if (touch_fd == -1) {
        printf("[触屏控制] 错误: 触摸屏设备未初始化\n");
        control_active = false;
        return NULL;
    }
    
    printf("[触屏控制] 触摸屏设备打开成功: /dev/input/event0\n");
    
    // 启动时清空输入缓冲区中的残留事件（避免读取到启动前的触摸事件）
    // 例如用户点击播放按钮时的触摸事件
    int flush_count = 0;
    while (flush_count < 50) {  // 最多读取50个事件来清空缓冲区
        ret = read(touch_fd, &touch, sizeof(struct input_event));
        if (ret == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break;  // 缓冲区已清空
            }
            break;
        }
        if (ret != sizeof(struct input_event)) {
            break;
        }
        flush_count++;
    }
    
    // 重置触摸状态（确保启动时没有残留的触摸状态）
    touch_pressed = false;
    touch_start_x = 0;
    touch_start_y = 0;
    touch_last_x = 0;
    touch_last_y = 0;
    touch_current_x = 0;
    touch_current_y = 0;
    
    // 添加短暂延迟，等待用户释放手指（如果正在点击播放按钮）
    usleep(200000);  // 200ms延迟
    
    // 循环处理触屏事件（即使视频播放结束，也继续运行以允许用户操作）
    // 只有用户点击返回主页时才会退出
    while (!should_exit_control) {
        // 读取输入事件（参考01touch.cpp）
        ret = read(touch_fd, &touch, sizeof(struct input_event));
        if (ret == -1) {
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
                printf("[触屏控制] 读取触摸屏数据失败: %s\n", strerror(errno));
                break;
            }
            usleep(10000);  // 10ms
            continue;
        }
        
        if (ret != sizeof(struct input_event)) {
            continue;
        }
        
        // 移除互斥锁，因为触摸状态变量只在触摸线程中使用，不需要保护
        // 这样可以提高响应速度，避免锁竞争
        
        // 解析X轴坐标
        if (touch.type == EV_ABS && touch.code == ABS_X) {
            touch_current_x = touch.value;
        }
        
        // 解析Y轴坐标
        if (touch.type == EV_ABS && touch.code == ABS_Y) {
            touch_current_y = touch.value;
        }
        
        // 解析触摸状态
        if (touch.type == EV_KEY && touch.code == BTN_TOUCH) {
            if (touch.value > 0) {
                // 按下
                if (!touch_pressed) {
                    touch_start_x = touch_current_x;
                    touch_start_y = touch_current_y;
                    touch_last_x = touch_current_x;
                    touch_last_y = touch_current_y;
                    touch_pressed = true;
                    printf("[触屏控制] 按下: (%d, %d)\n", touch_current_x, touch_current_y);
                }
            } else {
                // 释放
                if (touch_pressed) {
                    touch_last_x = touch_current_x;
                    touch_last_y = touch_current_y;
                    int dx = touch_last_x - touch_start_x;
                    int dy = touch_last_y - touch_start_y;
                    
                    printf("[触屏控制] 释放: (%d, %d), 移动: (%d, %d)\n", 
                           touch_current_x, touch_current_y, dx, dy);
                    
                    // 如果移动距离很小，认为是点击
                    if (abs(dx) < 10 && abs(dy) < 10) {
                        // 无论视频是否在播放，都处理点击事件
                        // 这样可以支持视频结束后切换视频或返回主页
                        handle_click(touch_start_x, touch_start_y);
                    } else {
                        // 否则是滑动
                        // 只有视频播放时才处理滑动事件（音量、速度控制）
                        if (simple_video_is_playing()) {
                            handle_swipe(touch_start_x, touch_start_y, touch_last_x, touch_last_y);
                        } else {
                            printf("[触屏控制] 视频已停止，忽略滑动事件（可点击左上角返回或左下/右下切换视频）\n");
                        }
                    }
                    
                    touch_pressed = false;
                }
            }
        }
        
        // 同步事件，更新最后位置
        if (touch.type == EV_SYN && touch.code == SYN_REPORT) {
            if (touch_pressed) {
                touch_last_x = touch_current_x;
                touch_last_y = touch_current_y;
            }
        }
        
        // 如果设置了退出标志，立即退出
        if (should_exit_control) {
            break;
        }
    }
    
    // 注意：不关闭触摸屏设备，因为它是全局共享的
    // 设备由 touch_device 模块统一管理
    
    printf("[触屏控制] 触屏控制线程退出\n");
    control_active = false;
    return NULL;
}

/**
 * @brief 初始化触屏控制
 */
void video_touch_control_init(void) {
    // 不需要初始化，触摸屏设备由 touch_device 模块统一管理
}

/**
 * @brief 启动触屏控制线程
 */
void video_touch_control_start(void) {
    if (control_active) {
        return;
    }
    
    should_exit_control = false;
    control_active = true;
    
    if (pthread_create(&control_thread, NULL, control_thread_func, NULL) != 0) {
        control_active = false;
    }
}

/**
 * @brief 停止触屏控制线程
 */
void video_touch_control_stop(void) {
    if (!control_active) {
        return;
    }
    
    should_exit_control = true;
    
    // 注意：不关闭触摸屏设备，因为它是全局共享的
    // 只需要等待线程退出即可
    
    if (control_thread != 0) {
        pthread_join(control_thread, NULL);
        control_thread = 0;
    }
    
    control_active = false;
    printf("[触屏控制] 触屏控制已停止\n");
}

/**
 * @brief 处理触屏事件（保留此函数以兼容LVGL事件，但主要使用直接读取方式）
 */
void video_touch_control_handle_event(int x, int y, bool pressed) {
    // 此函数保留用于兼容，但实际触屏事件由control_thread_func直接读取
    // 如果需要，可以在这里添加额外的处理逻辑
    (void)x;
    (void)y;
    (void)pressed;
}
