#include "lvgl/lvgl.h"
#include "src/hal/hal.h"
#include "src/ui/ui_screens.h"
#include "src/ui/login_win.h"
#include "src/common/common.h"
#include "src/file_scanner/file_scanner.h"
#include "src/media_player/simple_video_player.h"
#include "src/media_player/audio_player.h"
#include "src/ui/video_touch_control.h"
#include <stdio.h>
#include <unistd.h>

int main(void)
{
    /* 初始化LVGL */
    lv_init();

    /* 初始化硬件抽象层（包括显示驱动、输入设备等） */
    hal_init();

    /* 初始化音频播放器（独立模块） */
    audio_player_init();

    /* 初始化视频播放器和触屏控制 */
    simple_video_init();
    video_touch_control_init();

    /* 扫描媒体文件 */
    scan_image_directory(IMAGE_DIR);
    scan_audio_directory(MEDIA_DIR);
    scan_video_directory(MEDIA_DIR);

    /* 创建UI界面 */
    create_main_screen();
    create_image_screen();
    create_player_screen();

    /* 先显示登录界面（在主页之前） */
    login_win_show();

    /* 主循环：处理LVGL任务 */
    while(!should_exit) {
        lv_timer_handler();
        usleep(5000);
    }

    return 0;
}
