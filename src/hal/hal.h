/**
 * @file hal.h
 * @brief 硬件抽象层头文件
 */

#ifndef HAL_H
#define HAL_H

#include <stdint.h>

/**
 * @brief 初始化硬件抽象层
 * 包括文件系统、显示驱动、输入设备等
 */
void hal_init(void);

/**
 * @brief 获取系统时间戳（毫秒）
 * @return 从程序启动开始经过的毫秒数
 */
uint32_t custom_tick_get(void);

#endif /* HAL_H */

