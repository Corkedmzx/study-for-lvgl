#ifndef WEATHER_WIN_H
#define WEATHER_WIN_H

#include "lvgl/lvgl.h"
#include "album_win.h"  // 包含相册窗口头文件以获取事件处理器声明

// 天气窗口对象（供其他模块访问）
extern lv_obj_t *weather_window;

void show_weather_window(void);

#endif

