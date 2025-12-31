/**
 * @file ui_callbacks.h
 * @brief UI回调函数声明
 */

#ifndef UI_CALLBACKS_H
#define UI_CALLBACKS_H

#include "lvgl/lvgl.h"

// 媒体播放相关回调函数
void play_audio_cb(lv_event_t * e);
void play_video_cb(lv_event_t * e);
void stop_cb(lv_event_t * e);
void slower_cb(lv_event_t * e);
void faster_cb(lv_event_t * e);
void reset_speed_cb(lv_event_t * e);
void volume_down_cb(lv_event_t * e);
void volume_up_cb(lv_event_t * e);
void prev_media_cb(lv_event_t * e);
void next_media_cb(lv_event_t * e);

#endif /* UI_CALLBACKS_H */

