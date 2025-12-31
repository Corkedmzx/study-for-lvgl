/**
 * @file common.c
 * @brief 公共定义和全局变量定义
 */

#include "common.h"
#include <unistd.h>  // for usleep
#include <fcntl.h>  // for open
#include <sys/mman.h>  // for mmap, msync, munmap
#include <sys/ioctl.h>  // for ioctl
#include <linux/fb.h>  // for framebuffer structures
#include <string.h>  // for memset

// 全局变量定义
lv_obj_t *main_screen = NULL;
lv_obj_t *image_screen = NULL;
lv_obj_t *player_screen = NULL;
lv_obj_t *video_container = NULL;
lv_obj_t *video_back_btn = NULL;
lv_obj_t *speed_label = NULL;
lv_obj_t *status_label = NULL;
bool should_exit = false;

// 图片显示相关变量
lv_obj_t *current_img_obj = NULL;
lv_obj_t *img_container = NULL;
lv_obj_t *img_info_label = NULL;
int current_img_index = 0;
bool is_gif_obj = false;
lv_color_t canvas_buf[680 * 280];

// 音频播放相关变量
int current_audio_index = 0;

// 视频播放相关变量
int current_video_index = 0;

/**
 * @brief 使用内存映射快速刷新主屏幕到framebuffer
 */
void fast_refresh_main_screen(void) {
    lv_disp_t *disp = lv_disp_get_default();
    if (!disp || !main_screen) {
        return;
    }
    
    // 1. 先让LVGL渲染整个屏幕
    lv_obj_invalidate(main_screen);
    
    // 2. 处理定时器，让LVGL完成渲染
    for (int i = 0; i < 20; i++) {
        lv_timer_handler();
        usleep(2000);  // 2ms，更频繁的处理
    }
    
    // 3. 强制立即刷新，触发所有flush_cb调用
    lv_refr_now(disp);
    
    // 4. 再次处理定时器，确保所有flush_cb完成
    for (int i = 0; i < 10; i++) {
        lv_timer_handler();
        usleep(2000);  // 2ms
    }
    
    // 5. 使用内存映射直接访问framebuffer，确保内容已写入
    int fb_fd = open("/dev/fb0", O_RDWR);
    if (fb_fd != -1) {
        struct fb_var_screeninfo vinfo;
        struct fb_fix_screeninfo finfo;
        if (ioctl(fb_fd, FBIOGET_VSCREENINFO, &vinfo) == 0 &&
            ioctl(fb_fd, FBIOGET_FSCREENINFO, &finfo) == 0) {
            long screensize = finfo.smem_len;
            char *fbp = (char *)mmap(0, screensize, PROT_READ | PROT_WRITE, MAP_SHARED, fb_fd, 0);
            if ((intptr_t)fbp != -1) {
                // 确保写入完成（通过msync）
                msync(fbp, screensize, MS_SYNC);
                munmap(fbp, screensize);
            }
        }
        close(fb_fd);
    }
    
    // 6. 最后一次强制刷新
    lv_refr_now(disp);
}

