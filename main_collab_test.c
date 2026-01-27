/**
 * @file main_collab_test.c
 * @brief 协作绘图测试程序（简化版，仅用于测试协作绘图功能）
 */

#include "lvgl/lvgl.h"
#include "src/hal/hal.h"
#include "src/touch_draw/touch_draw.h"
#include "src/common/common.h"
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <stdbool.h>

extern bool should_exit;  // 在common.c中定义

void signal_handler(int sig) {
    (void)sig;
    should_exit = true;
}

int main(void) {
    /* 设置信号处理 */
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    printf("========================================\n");
    printf("协作绘图测试程序（简化版）\n");
    printf("========================================\n");

    /* 初始化LVGL */
    lv_init();

    /* 初始化硬件抽象层（包括显示驱动、输入设备等） */
    hal_init();

    printf("\n初始化完成，显示触摸绘图界面...\n");
    printf("操作说明：\n");
    printf("  1. 点击\"连接协作\"按钮创建协作房间（主机模式）\n");
    printf("  2. 点击\"加入协作\"按钮加入他人的协作房间（客机模式）\n");
    printf("  3. 连接成功后，可以测试网络连接和数据同步\n");
    printf("  4. 按 Ctrl+C 退出程序\n\n");

    /* 直接显示触摸绘图窗口（跳过屏保和密码锁） */
    touch_draw_win_show();

    /* 主循环：处理LVGL任务 */
    while(!should_exit) {
        lv_timer_handler();
        usleep(5000);  // 5ms
    }

    printf("\n程序退出，清理资源...\n");
    touch_draw_cleanup();

    return 0;
}
