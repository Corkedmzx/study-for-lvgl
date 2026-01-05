#define _DEFAULT_SOURCE
#include "weather_win.h"
#include "video_win.h"  // 引入video_screen
#include "../weather/weather.h"
#include "../common/common.h"
#include "ui_screens.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include "lvgl/lvgl.h"
#include "lvgl/src/font/lv_font.h"

/* 声明SourceHanSansSC_VF字体（定义在bin/SourceHanSansSC_VF.c中） */
#if LV_FONT_SOURCE_HAN_SANS_SC_VF
extern const lv_font_t SourceHanSansSC_VF;
#endif

// 返回事件处理函数（前向声明）
static void weather_back_handler(lv_event_t *e);

// 天气窗口独立屏幕（全局变量，供其他模块访问）
lv_obj_t *weather_window = NULL;

// 更新天气显示
static void update_weather_display(lv_obj_t *cont) {
    // 清空原有内容
    lv_obj_clean(cont);
    
    printf("调用get_weather_data()...\n");
    char *data = get_weather_data();
    
    if (!data) {
        printf("get_weather_data()返回NULL\n");
        lv_obj_t *label = lv_label_create(cont);
        lv_label_set_text(label, "获取天气数据失败\n请检查网络连接");
        lv_obj_set_style_text_font(label, &SourceHanSansSC_VF, 0);
        lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);
        return;
    }
    
    printf("get_weather_data()返回数据长度: %zu\n", strlen(data));
    printf("天气数据前100字符: %.100s\n", data);
    
    // 检查数据格式（可能是"网络连接失败"或"数据格式错误"）
    if (strstr(data, "网络连接失败") != NULL || strstr(data, "数据格式错误") != NULL) {
        lv_obj_t *label = lv_label_create(cont);
        lv_label_set_text(label, data);
        lv_obj_set_style_text_font(label, &SourceHanSansSC_VF, 0);
        lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);
        free(data);
        return;
    }
    
    // 解析数据（格式：天数1\n信息\n信息|天数2\n信息\n信息|...）
    char *day_data[6] = {0};
    int day_count = 0;
    char *data_copy = strdup(data);
    if (!data_copy) {
        lv_obj_t *label = lv_label_create(cont);
        lv_label_set_text(label, "内存分配失败");
        lv_obj_set_style_text_font(label, &SourceHanSansSC_VF, 0);
        lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);
        free(data);
        return;
    }
    
    char *token = strtok(data_copy, "|");
    
    while (token && day_count < 6) {
        day_data[day_count++] = token;
        token = strtok(NULL, "|");
    }
    
    printf("解析到 %d 天的天气数据\n", day_count);
    
    if (day_count == 0) {
        lv_obj_t *label = lv_label_create(cont);
        lv_label_set_text(label, "未找到天气数据\n请检查网络");
        lv_obj_set_style_text_font(label, &SourceHanSansSC_VF, 0);
        lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);
        free(data_copy);
        free(data);
        return;
    }
    
    // 创建横向排列的容器
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(cont, 10, 0);
    
    // 为每一天创建面板
    for (int i = 0; i < day_count; i++) {
        lv_obj_t *day_panel = lv_obj_create(cont);
        lv_obj_set_size(day_panel, 180, 200);
        lv_obj_set_style_border_width(day_panel, 1, 0);
        lv_obj_set_style_pad_all(day_panel, 10, 0);
        
        // 解析每天信息（格式：日期\n最高/最低温\n平均温度\n天气\n风力\n湿度\n云量）
        char *lines[7] = {0};
        int line_count = 0;
        char *day_copy = strdup(day_data[i]);
        if (!day_copy) continue;
        
        char *line = strtok(day_copy, "\n");
        
        while (line && line_count < 7) {
            lines[line_count++] = line;
            line = strtok(NULL, "\n");
        }
        
        printf("第%d天数据，解析到 %d 行\n", i + 1, line_count);
        
        // 调整面板大小以容纳更多信息
        lv_obj_set_size(day_panel, 180, 320);
        
        // 添加天气信息到面板
        int y_offset = 5;
        if (line_count > 0) {
            // 日期
            lv_obj_t *label = lv_label_create(day_panel);
            lv_label_set_text(label, lines[0] ? lines[0] : "日期");
        lv_obj_set_style_text_font(label, &SourceHanSansSC_VF, 0);
            lv_obj_set_style_text_color(label, lv_color_hex(0x0066CC), 0);
            lv_obj_align(label, LV_ALIGN_TOP_MID, 0, y_offset);
            y_offset += 30;
            
            if (line_count > 1) {
                // 最高/最低温度
                lv_obj_t *temp_range = lv_label_create(day_panel);
                char temp_text[64];
                snprintf(temp_text, sizeof(temp_text), "%s", lines[1] ? lines[1] : "温度");
                lv_label_set_text(temp_range, temp_text);
                lv_obj_set_style_text_font(temp_range, &SourceHanSansSC_VF, 0);
                lv_obj_set_style_text_color(temp_range, lv_color_hex(0xFF6600), 0);
                lv_obj_align(temp_range, LV_ALIGN_TOP_MID, 0, y_offset);
                y_offset += 25;
            }
            
            if (line_count > 2) {
                // 平均温度
                lv_obj_t *temp = lv_label_create(day_panel);
                char temp_text[64];
                snprintf(temp_text, sizeof(temp_text), "平均: %s", lines[2] ? lines[2] : "--");
                lv_label_set_text(temp, temp_text);
                lv_obj_set_style_text_font(temp, &SourceHanSansSC_VF, 0);
                lv_obj_align(temp, LV_ALIGN_TOP_MID, 0, y_offset);
                y_offset += 30;
            }
                
            if (line_count > 3) {
                    // 天气状况
                    lv_obj_t *weather = lv_label_create(day_panel);
                lv_label_set_text(weather, lines[3] ? lines[3] : "天气");
                lv_obj_set_style_text_font(weather, &SourceHanSansSC_VF, 0);
                lv_obj_set_style_text_color(weather, lv_color_hex(0x009900), 0);
                lv_obj_align(weather, LV_ALIGN_TOP_MID, 0, y_offset);
                y_offset += 30;
            }
                    
            if (line_count > 4) {
                        // 风力
                        lv_obj_t *wind = lv_label_create(day_panel);
                char wind_text[64];
                snprintf(wind_text, sizeof(wind_text), "风力: %s", lines[4] ? lines[4] : "--");
                lv_label_set_text(wind, wind_text);
                lv_obj_set_style_text_font(wind, &SourceHanSansSC_VF, 0);
                lv_obj_align(wind, LV_ALIGN_TOP_MID, 0, y_offset);
                y_offset += 25;
            }
                        
            if (line_count > 5) {
                // 湿度
                lv_obj_t *humidity = lv_label_create(day_panel);
                char hum_text[64];
                snprintf(hum_text, sizeof(hum_text), "湿度: %s", lines[5] ? lines[5] : "--");
                lv_label_set_text(humidity, hum_text);
                lv_obj_set_style_text_font(humidity, &SourceHanSansSC_VF, 0);
                lv_obj_align(humidity, LV_ALIGN_TOP_MID, 0, y_offset);
                y_offset += 25;
            }
            
            if (line_count > 6) {
                // 云量
                lv_obj_t *cloud = lv_label_create(day_panel);
                char cloud_text[64];
                snprintf(cloud_text, sizeof(cloud_text), "云量: %s", lines[6] ? lines[6] : "--");
                lv_label_set_text(cloud, cloud_text);
                lv_obj_set_style_text_font(cloud, &SourceHanSansSC_VF, 0);
                lv_obj_align(cloud, LV_ALIGN_TOP_MID, 0, y_offset);
            }
        }
        
        free(day_copy);
    }
    
    free(data_copy);
    free(data);
}

// 返回事件处理函数实现 - 确保稳定运行
static void weather_back_handler(lv_event_t *e) {
    // 检查事件类型，确保是点击事件
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    
    // 隐藏天气窗口
    if (weather_window) {
        lv_obj_add_flag(weather_window, LV_OBJ_FLAG_HIDDEN);
    }
    
    // 恢复主屏幕显示
    extern lv_obj_t *main_screen;
    if (main_screen) {
        lv_obj_clear_flag(main_screen, LV_OBJ_FLAG_HIDDEN);
        lv_scr_load(main_screen);
        lv_refr_now(NULL);  // 强制刷新显示
        usleep(100000);  // 等待100ms确保刷新完成
        lv_refr_now(NULL);  // 再次刷新
    }
}

// 显示天气窗口
void show_weather_window(void) {
    // 停止其他模块
    extern bool simple_video_is_playing(void);
    extern void simple_video_stop(void);
    extern bool audio_player_is_playing(void);
    extern void audio_player_stop(void);
    
    // 清理其他模块
    extern lv_obj_t *image_screen;
    extern lv_obj_t *player_screen;
    if (image_screen) {
        lv_obj_add_flag(image_screen, LV_OBJ_FLAG_HIDDEN);
    }
    if (player_screen) {
        lv_obj_add_flag(player_screen, LV_OBJ_FLAG_HIDDEN);
    }
    if (video_screen) {
        lv_obj_add_flag(video_screen, LV_OBJ_FLAG_HIDDEN);
    }
    
    if (simple_video_is_playing()) {
        simple_video_stop();
    }
    if (audio_player_is_playing()) {
        audio_player_stop();
    }
    
    // 创建或显示天气窗口
    if (weather_window == NULL) {
        weather_window = lv_obj_create(NULL);
        lv_obj_set_size(weather_window, 800, 480);
        lv_obj_set_style_bg_color(weather_window, lv_color_white(), 0);
        lv_obj_clear_flag(weather_window, LV_OBJ_FLAG_SCROLLABLE);
    } else {
        lv_obj_clean(weather_window);  // 清理旧内容
        lv_obj_clear_flag(weather_window, LV_OBJ_FLAG_HIDDEN);  // 确保窗口可见
    }
    
    // 确保主屏幕隐藏
    extern lv_obj_t *main_screen;
    if (main_screen) {
        lv_obj_add_flag(main_screen, LV_OBJ_FLAG_HIDDEN);
    }
    
    lv_obj_t *win = weather_window;
    
    // 标题
    lv_obj_t *title = lv_label_create(win);
    lv_label_set_text(title, "天气预报");
    lv_obj_set_style_text_font(title, &SourceHanSansSC_VF, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 20);
    
    // 区域显示（在标题下方）
    lv_obj_t *location_label = lv_label_create(win);
    lv_label_set_text(location_label, "广西贺州市");
    lv_obj_set_style_text_font(location_label, &SourceHanSansSC_VF, 0);
    lv_obj_set_style_text_color(location_label, lv_color_hex(0x666666), 0);
    lv_obj_align(location_label, LV_ALIGN_TOP_MID, 0, 55);
    
    // 返回按钮（返回主窗口）- 先创建，确保在最上层
    lv_obj_t *btn = lv_btn_create(win);
    lv_obj_set_size(btn, 100, 50);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x0000FF), 0);
    lv_obj_align(btn, LV_ALIGN_TOP_RIGHT, -20, 20);
    lv_obj_move_foreground(btn);  // 确保在最上层
    lv_obj_t *btn_label = lv_label_create(btn);
    lv_label_set_text(btn_label, "返回");
    lv_obj_set_style_text_font(btn_label, &SourceHanSansSC_VF, 0);
    lv_obj_center(btn_label);
    
    // 返回按钮事件处理 - 必须设置
    lv_obj_add_event_cb(btn, weather_back_handler, LV_EVENT_CLICKED, NULL);
    
    // 天气数据容器
    lv_obj_t *cont = lv_obj_create(win);
    lv_obj_set_size(cont, 780, 360);
    lv_obj_align(cont, LV_ALIGN_CENTER, 0, 30);
    lv_obj_set_style_border_width(cont, 0, 0);
    lv_obj_set_style_pad_all(cont, 10, 0);
    lv_obj_set_style_bg_opa(cont, LV_OPA_0, 0);
    
    // 先显示加载提示
    lv_obj_t *loading_label = lv_label_create(cont);
    lv_label_set_text(loading_label, "正在加载天气数据...");
    lv_obj_set_style_text_font(loading_label, &SourceHanSansSC_VF, 0);
    lv_obj_align(loading_label, LV_ALIGN_CENTER, 0, 0);
    lv_refr_now(NULL);  // 立即刷新显示加载提示
    
    // 更新天气显示（在后台线程或延迟执行）
    update_weather_display(cont);
    
    // 切换到天气屏幕
    lv_scr_load(win);
    lv_refr_now(NULL);
}

