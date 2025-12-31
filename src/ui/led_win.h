#ifndef LED_WIN_H
#define LED_WIN_H

#include <stdbool.h>
#include "lvgl/lvgl.h"

void show_led_window(void);
void led_win_event_handler(lv_event_t *e);

#endif

