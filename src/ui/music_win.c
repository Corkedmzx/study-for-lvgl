#include "music_win.h"
#include "ui_screens.h"
#include "video_win.h"  // 引入video_screen
#include "../media_player/audio_player.h"
#include "../media_player/simple_video_player.h"
#include "../file_scanner/file_scanner.h"
#include "../common/common.h"
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include "lvgl/lvgl.h"

static lv_obj_t *music_win = NULL;

static void update_music_label() {
    if (!music_win) return;
    
    lv_obj_t *label = lv_obj_get_child(music_win, 0);
    if (!label) return;
    
    extern char **audio_files;
    extern int audio_count;
    extern int current_audio_index;
    
    if (audio_count == 0 || audio_files == NULL || current_audio_index < 0 || current_audio_index >= audio_count) {
        lv_label_set_text(label, "音乐: 无文件");
        return;
    }
    
    const char *file_path = audio_files[current_audio_index];
    const char *filename = strrchr(file_path, '/');
    filename = filename ? filename + 1 : file_path;
    
    char label_text[256];
    snprintf(label_text, sizeof(label_text), "音乐: %s", filename);
    lv_label_set_text(label, label_text);
}

static void music_control_handler(lv_event_t *e) {
    if(lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    
    lv_obj_t *btn = lv_event_get_target(e);
    extern int current_audio_index;
    extern char **audio_files;
    extern int audio_count;
    
    // 判断是上一首还是下一首按钮
    lv_obj_t *prev_btn = lv_obj_get_child(music_win, 2);
    
    if(btn == prev_btn) { // 上一首
        current_audio_index--;
        if (current_audio_index < 0) {
            current_audio_index = audio_count - 1;
        }
    } else { // 下一首
        current_audio_index++;
        if (current_audio_index >= audio_count) {
            current_audio_index = 0;
        }
    }
    
    // 停止当前播放并播放新的
    if (audio_player_is_playing()) {
        audio_player_stop();
    }
    
    if (audio_files && audio_files[current_audio_index]) {
        audio_player_play(audio_files[current_audio_index]);
    }
    
    update_music_label();
}

void music_win_show(void) {
    // 先停止任何正在播放的媒体和其他模块
    extern bool simple_video_is_playing(void);
    extern void simple_video_stop(void);
    extern bool audio_player_is_playing(void);
    extern void audio_player_stop(void);
    
    // 清理其他模块
    extern lv_obj_t *image_screen;
    if (image_screen) {
        lv_obj_add_flag(image_screen, LV_OBJ_FLAG_HIDDEN);
    }
    if (video_screen) {  // 通过video_win.h引入
        lv_obj_add_flag(video_screen, LV_OBJ_FLAG_HIDDEN);
    }
    
    if (simple_video_is_playing()) {
        simple_video_stop();
    }
    if (audio_player_is_playing()) {
        audio_player_stop();
    }
    
    // 直接使用player_screen，它已经有完整的功能
    extern lv_obj_t *player_screen;
    if (!player_screen) {
        extern void create_player_screen(void);
        create_player_screen();
    }
    
    // 确保player_screen显示
    if (player_screen) {
        lv_obj_clear_flag(player_screen, LV_OBJ_FLAG_HIDDEN);
    }
    
    // 确保主屏幕隐藏
    extern lv_obj_t *main_screen;
    if (main_screen) {
        lv_obj_add_flag(main_screen, LV_OBJ_FLAG_HIDDEN);
    }
    
    // 使用音频文件列表
    extern char **audio_files;
    extern int audio_count;
    extern int current_audio_index;
    
    // 找到第一个音频文件并播放
    if (audio_count > 0 && audio_files != NULL) {
        current_audio_index = 0;
        const char *file = audio_files[0];
        if (file) {
            audio_player_play(file);
        }
    } else {
        printf("警告: 未找到音频文件\n");
    }
    
    // 切换到播放器屏幕
    lv_scr_load(player_screen);
    
    // 隐藏主窗口
    if (main_screen) {
        lv_obj_add_flag(main_screen, LV_OBJ_FLAG_HIDDEN);
    }
    
    // 强制刷新
    lv_refr_now(NULL);
    
}

void music_win_event_handler(lv_event_t *e) {
    if(lv_event_get_code(e) == LV_EVENT_CLICKED) {
        if (audio_player_is_playing()) {
            audio_player_stop();
            // 等待音频播放器完全停止
            usleep(100000);  // 100ms
        }
        
        if (music_win) {
            lv_obj_del(music_win);
            music_win = NULL;
        }
        
        extern lv_obj_t *main_screen;
        if (main_screen) {
            // 确保主屏幕可见
            lv_obj_clear_flag(main_screen, LV_OBJ_FLAG_HIDDEN);
            // 切换到主屏幕
            lv_scr_load(main_screen);
            
            // 使用快速刷新函数强制刷新整个屏幕
            extern void fast_refresh_main_screen(void);
            fast_refresh_main_screen();
            
            // 额外等待确保显示稳定
            usleep(50000);  // 50ms
            // 再次处理定时器和刷新
            lv_timer_handler();
            lv_refr_now(NULL);
        }
    }
}

