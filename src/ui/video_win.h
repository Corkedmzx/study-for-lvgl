#ifndef VIDEO_WIN_H
#define VIDEO_WIN_H

#include "lvgl/lvgl.h"

// 视频屏幕对象（供其他模块访问）
extern lv_obj_t *video_screen;

void video_win_show(void);
void video_win_show_with_file(const char *file_path);
void video_win_event_handler(lv_event_t *e);

#endif

