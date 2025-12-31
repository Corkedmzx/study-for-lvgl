#include "exit_win.h"
#include <fcntl.h>
#include <linux/fb.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

void exit_application(void) {
    // 使用内存映射快速刷黑屏（参考lcdc.cpp）
    int fb = open("/dev/fb0", O_RDWR);
    if(fb >= 0) {
        struct fb_var_screeninfo vinfo;
        struct fb_fix_screeninfo finfo;
        
        if (ioctl(fb, FBIOGET_VSCREENINFO, &vinfo) == 0 &&
            ioctl(fb, FBIOGET_FSCREENINFO, &finfo) == 0) {
            
            long int screensize = vinfo.yres_virtual * finfo.line_length;
            char *fbp = (char*)mmap(0, screensize, 
                                   PROT_READ | PROT_WRITE, 
                                   MAP_SHARED, fb, 0);
            
            if ((long)fbp != -1) {
                // 使用memset快速填充黑色
                memset(fbp, 0, screensize);
                // 确保写入完成
                msync(fbp, screensize, MS_SYNC);
                munmap(fbp, screensize);
            }
        }
        close(fb);
    }
    
    // 确保程序退出
    _exit(0);
}

