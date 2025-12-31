/**
 * @file hal.c
 * @brief 硬件抽象层实现
 */

#define _DEFAULT_SOURCE
#include <stdio.h>
#include <sys/time.h>
#include "lvgl/lvgl.h"
#include "lvgl/src/extra/libs/fsdrv/lv_fsdrv.h"
#include "lv_drivers/display/fbdev.h"
#include "lv_drivers/indev/evdev.h"
#include "hal.h"

#define DISP_BUF_SIZE (480 * 800)

uint32_t custom_tick_get(void)
{
    static uint64_t start_ms = 0;
    if(start_ms == 0) {
        struct timeval tv_start;
        gettimeofday(&tv_start, NULL);
        start_ms = (tv_start.tv_sec * 1000000 + tv_start.tv_usec) / 1000;
    }

    struct timeval tv_now;
    gettimeofday(&tv_now, NULL);
    uint64_t now_ms;
    now_ms = (tv_now.tv_sec * 1000000 + tv_now.tv_usec) / 1000;

    uint32_t time_ms = now_ms - start_ms;
    return time_ms;
}

void hal_init(void) {
    printf("初始化HAL...\n");

    /* 初始化POSIX文件系统驱动（用于GIF加载） */
    lv_fs_posix_init();
    printf("文件系统初始化完成（POSIX文件系统，驱动器: P:）\n");

    /* Linux frame buffer device init */
    fbdev_init();

    /* A small buffer for LittlevGL to draw the screen's content */
    static lv_color_t buf[DISP_BUF_SIZE];

    /* Initialize a descriptor for the buffer */
    static lv_disp_draw_buf_t disp_buf;
    lv_disp_draw_buf_init(&disp_buf, buf, NULL, DISP_BUF_SIZE);

    /* Initialize and register a display driver */
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.draw_buf   = &disp_buf;
    disp_drv.flush_cb   = fbdev_flush;
    disp_drv.hor_res    = 800;
    disp_drv.ver_res    = 480;
    lv_disp_drv_register(&disp_drv);
    printf("显示驱动初始化完成: 800x480\n");

    /* 初始化触摸屏输入设备 */
    evdev_init();
    static lv_indev_drv_t indev_drv_1;
    lv_indev_drv_init(&indev_drv_1);
    indev_drv_1.type = LV_INDEV_TYPE_POINTER;
    indev_drv_1.read_cb = evdev_read;
    lv_indev_t *touch_indev = lv_indev_drv_register(&indev_drv_1);
    (void)touch_indev;  // 避免未使用变量警告
    printf("触摸屏输入设备初始化完成\n");
}

