#include "lvgl/lvgl.h"
#include "src/hal/hal.h"
#include "src/ui/ui_screens.h"
#include "src/ui/login_win.h"
#include "src/ui/screensaver_win.h"
#include "src/common/common.h"
#include "src/common/touch_device.h"
#include "src/file_scanner/file_scanner.h"
#include "src/media_player/simple_video_player.h"
#include "src/media_player/audio_player.h"
#include "src/ui/video_touch_control.h"
#include "src/time_sync/time_sync.h"
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <stdlib.h>
#include <signal.h>

int main(void)
{
    /* 初始化LVGL */
    lv_init();

    /* 初始化硬件抽象层（包括显示驱动、输入设备等） */
    hal_init();

    /* 设置系统时区为Asia/Shanghai (UTC+8) */
    setenv("TZ", "Asia/Shanghai", 1);
    tzset();
    printf("系统时区已设置为: Asia/Shanghai (UTC+8)\n");
    
    /* 同步系统时间（开发板重启后时间会被重置） */
    printf("正在同步系统时间...\n");
    if (sync_system_time() == 0) {
        printf("系统时间同步成功\n");
    } else {
        printf("系统时间同步失败，继续运行\n");
    }

    /* 初始化触摸屏设备（在程序启动时统一打开） */
    if (touch_device_init() != 0) {
        printf("警告: 触摸屏设备初始化失败，某些功能可能无法使用\n");
    }

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

    /* 先显示屏保（在密码锁之前） */
    screensaver_win_show();

    /* 主循环：处理LVGL任务 */
    while(!should_exit) {
        lv_timer_handler();
        
        // 检查屏保是否解锁
        extern void screensaver_win_check_unlock(void);
        screensaver_win_check_unlock();
        
        // 检查密码锁是否需要切换到主屏幕
        extern void login_win_check_show_main(void);
        login_win_check_show_main();
        
        // 检查是否需要返回主页（从视频播放退出）
        extern bool need_return_to_main;
        if (need_return_to_main) {
            need_return_to_main = false;
            
            // 优化退出逻辑：使用内存映射快速刷新
            extern lv_obj_t* get_main_page1_screen(void);
            extern lv_obj_t *video_screen;
            
            // 获取主页第一页screen
            lv_obj_t *main_page_screen = get_main_page1_screen();
            if (!main_page_screen) {
                continue;
            }
            
            // 隐藏视频屏幕
            if (video_screen) {
                lv_obj_add_flag(video_screen, LV_OBJ_FLAG_HIDDEN);
            }
            
            // 直接切换到主屏幕第一页
            lv_obj_clear_flag(main_page_screen, LV_OBJ_FLAG_HIDDEN);
            lv_scr_load(main_page_screen);
            
            // 先处理几次定时器，确保LVGL完成渲染
            for (int i = 0; i < 10; i++) {
                lv_timer_handler();
                usleep(10000);  // 10ms
            }
            
            // 使用内存映射快速刷新函数（立即同步framebuffer）
            // 这样可以减少时序窗口，避免触摸事件到达时framebuffer还没更新
            extern void fast_refresh_main_screen(void);
            fast_refresh_main_screen();
            
            // 额外等待确保显示稳定
            usleep(100000);  // 100ms
            // 再次处理定时器和刷新
            lv_timer_handler();
            lv_refr_now(NULL);
            
            // 完成，使用内存映射确保framebuffer立即更新
        }
        
        // 检查是否需要更新2048游戏显示（从触摸线程请求）
        extern bool need_update_2048_display;
        if (need_update_2048_display) {
            need_update_2048_display = false;
            // 在主线程中安全地更新游戏显示
            extern void game_2048_win_update_display(void);
            game_2048_win_update_display();
        }
        
        usleep(5000);
    }

    /* 程序退出时关闭触摸屏设备 */
    touch_device_deinit();

    return 0;
}
