/**
 * @file touch_device.h
 * @brief 触摸屏设备统一管理
 */

#ifndef TOUCH_DEVICE_H
#define TOUCH_DEVICE_H

#include <stdbool.h>

/**
 * @brief 初始化触摸屏设备（在程序启动时调用）
 * @return 成功返回0，失败返回-1
 */
int touch_device_init(void);

/**
 * @brief 关闭触摸屏设备（在程序退出时调用）
 */
void touch_device_deinit(void);

/**
 * @brief 获取触摸屏设备文件描述符
 * @return 设备文件描述符，失败返回-1
 */
int touch_device_get_fd(void);

/**
 * @brief 检查触摸屏设备是否已初始化
 * @return 已初始化返回true，否则返回false
 */
bool touch_device_is_initialized(void);

#endif // TOUCH_DEVICE_H

