/**
 * @file touch_device.c
 * @brief 触摸屏设备统一管理实现
 */

#include "touch_device.h"
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <linux/input.h>

#define TOUCH_DEVICE_PATH "/dev/input/event0"

static int touch_fd = -1;
static bool initialized = false;

/**
 * @brief 初始化触摸屏设备（在程序启动时调用）
 */
int touch_device_init(void) {
    if (initialized) {
        return 0;  // 已经初始化
    }
    
    touch_fd = open(TOUCH_DEVICE_PATH, O_RDONLY | O_NONBLOCK);
    if (touch_fd == -1) {
        printf("[触摸屏] 错误: 无法打开触摸屏设备 %s: %s\n", TOUCH_DEVICE_PATH, strerror(errno));
        return -1;
    }
    
    initialized = true;
    printf("[触摸屏] 触摸屏设备打开成功: %s (fd=%d)\n", TOUCH_DEVICE_PATH, touch_fd);
    return 0;
}

/**
 * @brief 关闭触摸屏设备（在程序退出时调用）
 */
void touch_device_deinit(void) {
    if (touch_fd >= 0) {
        close(touch_fd);
        touch_fd = -1;
        printf("[触摸屏] 触摸屏设备已关闭\n");
    }
    initialized = false;
}

/**
 * @brief 获取触摸屏设备文件描述符
 */
int touch_device_get_fd(void) {
    if (!initialized || touch_fd < 0) {
        return -1;
    }
    return touch_fd;
}

/**
 * @brief 检查触摸屏设备是否已初始化
 */
bool touch_device_is_initialized(void) {
    return initialized && touch_fd >= 0;
}

