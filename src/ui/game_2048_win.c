/**
 * @file game_2048_win.c
 * @brief 2048游戏窗口实现
 */

#include "game_2048_win.h"
#include "../game_2048/game_2048.h"
#include "../common/common.h"
#include "../common/touch_device.h"
#include "lvgl/lvgl.h"
#include "lvgl/src/misc/lv_timer.h"  // For LVGL timer
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <linux/input.h>
#include <pthread.h>
#include <errno.h>
#include <sys/time.h>  // For gettimeofday (参考03touch.cpp)
#include <math.h>      // For sqrt (参考03touch.cpp)
#include <time.h>      // For time functions
#include <sys/stat.h>  // For file operations

// 游戏窗口和控件
static lv_obj_t *game_window = NULL;
static lv_obj_t *game_grid[4][4] = {NULL};  // 4x4网格按钮
static lv_obj_t *score_label = NULL;
static lv_obj_t *game_over_label = NULL;
static lv_obj_t *restart_btn = NULL;
static lv_obj_t *history_btn = NULL;
static lv_obj_t *history_window = NULL;  // 历史记录窗口
static lv_timer_t *timer_update_timer = NULL;  // 定时更新计时器的定时器
static lv_obj_t *start_game_btn = NULL;  // 开始游戏按钮

// 屏幕尺寸
#define SCREEN_WIDTH 800
#define SCREEN_HEIGHT 480

// 游戏区域坐标（右侧游戏区）
#define GAME_AREA_X 300
#define GAME_AREA_Y 30
#define GAME_AREA_WIDTH 420
#define GAME_AREA_HEIGHT 420

// 触摸屏坐标范围（需要根据实际触摸屏调整，参考03touch.cpp）
#define TOUCH_MAX_X 1024
#define TOUCH_MAX_Y 600

// 游戏状态
static game_2048_t game_state;

// 历史记录数据结构
#define MAX_HISTORY_RECORDS 100
#define HISTORY_FILE_PATH "/tmp/2048_history.txt"

typedef struct {
    int score;
    time_t timestamp;  // 完成时间戳
    int game_time;    // 游戏时间（秒）
} history_record_t;

static history_record_t history_records[MAX_HISTORY_RECORDS];
static int history_count = 0;
static int last_saved_index = -1;  // 最近一次保存的记录索引（用于高亮显示）
static bool game_over_saved = false;  // 标记当前游戏是否已保存历史记录

// 计时功能
static time_t game_start_time = 0;  // 游戏开始时间
static time_t game_elapsed_time = 0;  // 游戏已用时间（秒）
static bool game_timer_running = false;  // 计时器是否运行
static bool game_started = false;  // 游戏是否已开始（点击开始按钮后才开始）

// 触摸控制（参考03touch.cpp的滑动检测逻辑）
static pthread_t touch_thread = 0;
static bool touch_thread_running = false;
static int touch_x = 0, touch_y = 0;  // 当前触摸坐标（参考03touch.cpp）
static int touch_start_x = 0, touch_start_y = 0;  // 滑动开始坐标
static int touch_end_x = 0, touch_end_y = 0;  // 滑动结束坐标
static bool touch_pressed = false;
static struct timeval swipe_start_time, swipe_end_time;  // 滑动时间记录（参考03touch.cpp）
static bool tracking_swipe = false;  // 是否正在跟踪滑动（参考03touch.cpp）

// 滑动检测阈值（参考03touch.cpp的滑动逻辑）
#define SWIPE_THRESHOLD 30        // 滑动最小距离阈值(像素) - 短距离滑动
#define SWIPE_TIME_THRESHOLD 300000  // 滑动最大时间阈值(微秒) - 300ms（参考03touch.cpp）

// 函数前向声明
static void save_history_record(int score, int game_time);
static void format_game_time_string(char *buf, size_t buf_size, int seconds);
static void load_history_records(void);
static void clear_history_records(void);
static void show_history_window(void);
static void history_back_btn_cb(lv_event_t *e);
static void history_btn_cb(lv_event_t *e);
static void clear_history_btn_cb(lv_event_t *e);
static void timer_update_cb(lv_timer_t *timer);
static void start_game_btn_cb(lv_event_t *e);
static bool is_in_game_area(int x, int y);

// 颜色映射（根据数字值）
static lv_color_t get_tile_color(int value) {
    switch (value) {
        case 0:    return lv_color_hex(0xCDC1B4);  // 空方块
        case 2:    return lv_color_hex(0xEEE4DA);
        case 4:    return lv_color_hex(0xEDE0C8);
        case 8:    return lv_color_hex(0xF2B179);
        case 16:   return lv_color_hex(0xF59563);
        case 32:   return lv_color_hex(0xF67C5F);
        case 64:   return lv_color_hex(0xF65E3B);
        case 128:  return lv_color_hex(0xEDCF72);
        case 256:  return lv_color_hex(0xEDCC61);
        case 512:  return lv_color_hex(0xEDC850);
        case 1024: return lv_color_hex(0xEDC53F);
        case 2048: return lv_color_hex(0xEDC22E);
        default:   return lv_color_hex(0x3C3A32);
    }
}

// 文字颜色（根据数字值）
static lv_color_t get_text_color(int value) {
    if (value <= 4) {
        return lv_color_hex(0x776E65);  // 深色文字
    } else {
        return lv_color_hex(0xF9F6F2);  // 浅色文字
    }
}

/**
 * @brief 映射触摸屏坐标到屏幕坐标（参考03touch.cpp）
 */
static void map_touch_to_screen(int *x, int *y) {
    float x_scale = (float)SCREEN_WIDTH / TOUCH_MAX_X;
    float y_scale = (float)SCREEN_HEIGHT / TOUCH_MAX_Y;
    
    *x = *x * x_scale;
    *y = *y * y_scale;
    
    if (*x < 0) *x = 0;
    if (*x >= SCREEN_WIDTH) *x = SCREEN_WIDTH - 1;
    if (*y < 0) *y = 0;
    if (*y >= SCREEN_HEIGHT) *y = SCREEN_HEIGHT - 1;
}

/**
 * @brief 检查坐标是否在游戏区域内（右侧游戏区）
 */
static bool is_in_game_area(int x, int y) {
    // 先映射触摸坐标到屏幕坐标
    map_touch_to_screen(&x, &y);
    
    return (x >= GAME_AREA_X && x < (GAME_AREA_X + GAME_AREA_WIDTH) &&
            y >= GAME_AREA_Y && y < (GAME_AREA_Y + GAME_AREA_HEIGHT));
}

/**
 * @brief 定时器回调：定期更新计时器显示
 */
static void timer_update_cb(lv_timer_t *timer) {
    (void)timer;
    
    if (!game_window || !game_timer_running || game_state.game_over || !game_started) {
        return;
    }
    
    // 更新计时器
    game_elapsed_time = time(NULL) - game_start_time;
    
    // 更新分数和时间显示
    if (score_label) {
        char time_str[32];
        format_game_time_string(time_str, sizeof(time_str), game_elapsed_time);
        char score_text[64];
        snprintf(score_text, sizeof(score_text), "分数: %d\n时间: %s", game_state.score, time_str);
        lv_label_set_text(score_label, score_text);
    }
}

/**
 * @brief 更新游戏显示（必须在主线程中调用）
 */
static void update_game_display(void) {
    if (!game_window) return;
    
    // 检查游戏是否刚刚结束（需要立即保存记录）
    bool just_game_over = false;
    if (game_state.game_over && !game_over_saved && game_state.score > 0) {
        just_game_over = true;
    }
    
    // 更新计时器（如果游戏正在进行）
    if (game_timer_running && !game_state.game_over) {
        game_elapsed_time = time(NULL) - game_start_time;
    }
    
    // 显示游戏结束提示
    if (game_state.game_over) {
        if (game_over_label) {
            lv_obj_clear_flag(game_over_label, LV_OBJ_FLAG_HIDDEN);
        }
        // 停止计时器并计算最终时间
        if (game_timer_running) {
            game_timer_running = false;
            game_elapsed_time = time(NULL) - game_start_time;
        } else if (game_start_time > 0) {
            // 如果计时器已经停止，但游戏时间还没计算，计算最终时间
            game_elapsed_time = time(NULL) - game_start_time;
        }
        
        // 暂停定时器
        if (timer_update_timer) {
            lv_timer_pause(timer_update_timer);
        }
        
        // 游戏结束时立即保存历史记录（只保存一次）
        if (game_state.score > 0 && !game_over_saved) {
            // 确保时间已计算（多重保险）
            if (game_start_time > 0) {
                if (game_elapsed_time == 0 || game_timer_running) {
                    game_elapsed_time = time(NULL) - game_start_time;
                }
            }
            // 确保时间不为0（至少1秒）
            if (game_elapsed_time == 0) {
                game_elapsed_time = 1;
            }
            save_history_record(game_state.score, game_elapsed_time);
            game_over_saved = true;
            printf("[2048] 游戏结束，保存历史记录: 分数=%d, 时间=%d秒, game_start_time=%ld, 当前时间=%ld\n", 
                   game_state.score, game_elapsed_time, game_start_time, time(NULL));
        }
        
        // 游戏结束后，重置游戏开始标志，显示开始按钮
        game_started = false;
        if (start_game_btn) {
            lv_obj_clear_flag(start_game_btn, LV_OBJ_FLAG_HIDDEN);
        }
    } else {
        if (game_over_label) {
            lv_obj_add_flag(game_over_label, LV_OBJ_FLAG_HIDDEN);
        }
        // 注意：不要在这里重置game_over_saved，因为：
        // 1. 如果游戏正在进行中，不应该重置
        // 2. game_over_saved只应该在游戏真正重新开始时重置（在game_2048_win_show或restart_btn_cb中）
    }
    
    // 更新分数和时间显示
    if (score_label) {
        char time_str[32];
        format_game_time_string(time_str, sizeof(time_str), game_elapsed_time);
        char score_text[64];
        snprintf(score_text, sizeof(score_text), "分数: %d\n时间: %s", game_state.score, time_str);
        lv_label_set_text(score_label, score_text);
    }
    
    // 更新网格
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            if (game_grid[i][j]) {
                int value = game_state.grid[i][j];
                
                // 设置背景颜色
                lv_obj_set_style_bg_color(game_grid[i][j], get_tile_color(value), 0);
                
                // 更新文字
                lv_obj_t *label = lv_obj_get_child(game_grid[i][j], 0);
                if (label) {
                    if (value == 0) {
                        lv_label_set_text(label, "");
                    } else {
                        char text[16];
                        snprintf(text, sizeof(text), "%d", value);
                        lv_label_set_text(label, text);
                        lv_obj_set_style_text_color(label, get_text_color(value), 0);
                    }
                }
            }
        }
    }
}

/**
 * @brief 触摸控制线程
 */
static void *touch_thread_func(void *arg) {
    struct input_event ev;
    int ret;
    
    // 使用统一的触摸屏设备（已在程序启动时打开）
    int touch_fd = touch_device_get_fd();
    if (touch_fd == -1) {
        printf("[2048] 错误: 触摸屏设备未初始化\n");
        touch_thread_running = false;
        return NULL;
    }
    
    printf("[2048] 使用触摸屏设备 (fd=%d)\n", touch_fd);
    
    while (touch_thread_running) {
        ret = read(touch_fd, &ev, sizeof(struct input_event));
        if (ret != sizeof(struct input_event)) {
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
                break;
            }
            usleep(10000);
            continue;
        }
        
        // 处理坐标事件（参考03touch.cpp：持续更新当前坐标）
        if (ev.type == EV_ABS) {
            if (ev.code == ABS_X) {
                touch_x = ev.value;  // 持续更新当前X坐标
            } else if (ev.code == ABS_Y) {
                touch_y = ev.value;  // 持续更新当前Y坐标
            }
        } else if (ev.type == EV_KEY && ev.code == BTN_TOUCH) {
            if (ev.value > 0) {
                // 按下（参考03touch.cpp）
                touch_pressed = true;
                touch_start_x = touch_x;  // 保存按下时的坐标
                touch_start_y = touch_y;
                gettimeofday(&swipe_start_time, NULL);  // 记录开始时间
                tracking_swipe = true;  // 开始跟踪滑动
            } else {
                // 释放（参考03touch.cpp的滑动检测逻辑）
                if (touch_pressed && tracking_swipe) {
                    touch_end_x = touch_x;  // 使用当前坐标作为结束坐标
                    touch_end_y = touch_y;
                    gettimeofday(&swipe_end_time, NULL);  // 记录结束时间
                    
                    // 检查滑动是否在游戏区域内，且游戏已开始
                    bool start_in_game = is_in_game_area(touch_start_x, touch_start_y);
                    bool end_in_game = is_in_game_area(touch_end_x, touch_end_y);
                    
                    // 只有在游戏区域内滑动且游戏已开始才处理
                    if (!game_started || !start_in_game || !end_in_game) {
                        tracking_swipe = false;
                        touch_pressed = false;
                        continue;
                    }
                    
                    // 计算滑动距离和时间（参考03touch.cpp）
                    int dx = touch_end_x - touch_start_x;
                    int dy = touch_end_y - touch_start_y;
                    long duration = (swipe_end_time.tv_sec - swipe_start_time.tv_sec) * 1000000 + 
                                   (swipe_end_time.tv_usec - swipe_start_time.tv_usec);
                    
                    // 计算总滑动距离（参考03touch.cpp：使用sqrt计算）
                    float distance = sqrt(dx * dx + dy * dy);
                    
                    // 滑动检测：距离和时间都必须满足阈值（参考03touch.cpp）
                    if (distance >= SWIPE_THRESHOLD && duration <= SWIPE_TIME_THRESHOLD) {
                        int abs_dx = abs(dx);
                        int abs_dy = abs(dy);
                        
                        // 判断滑动方向（参考03touch.cpp：使用abs(dx) > abs(dy)判断水平/垂直）
                        if (abs_dx > abs_dy) {
                            // 水平滑动
                            if (dx > 0) {
                                // 右滑
                                game_2048_move_right(&game_state);
                            } else {
                                // 左滑
                                game_2048_move_left(&game_state);
                            }
                            
                            // 设置标志，让主线程更新显示（不在触摸线程中操作LVGL）
                            extern bool need_update_2048_display;
                            need_update_2048_display = true;
                        } else {
                            // 垂直滑动
                            if (dy > 0) {
                                // 下滑
                                game_2048_move_down(&game_state);
                            } else {
                                // 上滑
                                game_2048_move_up(&game_state);
                            }
                            
                            // 设置标志，让主线程更新显示（不在触摸线程中操作LVGL）
                            extern bool need_update_2048_display;
                            need_update_2048_display = true;
                        }
                    }
                    // 如果距离不足或时间过长，则不处理（参考03touch.cpp）
                    
                    tracking_swipe = false;
                    touch_pressed = false;
                }
            }
        }
    }
    
    // 注意：不关闭触摸屏设备，因为它是全局共享的
    // 设备由 touch_device 模块统一管理
    
    printf("[2048] 触摸控制线程退出\n");
    return NULL;
}

/**
 * @brief 保存历史记录到文件
 */
static void save_history_record(int score, int game_time) {
    // 重要：在保存新记录前，先重新加载文件中的历史记录，避免覆盖
    // 因为可能在其他地方（如程序重启）已经保存了记录
    load_history_records();
    
    printf("[2048] save_history_record: 开始保存记录, score=%d, game_time=%d, 当前记录数=%d\n", 
           score, game_time, history_count);
    
    if (history_count >= MAX_HISTORY_RECORDS) {
        // 如果记录已满，删除最旧的记录（分数最低的）
        int min_score = history_records[0].score;
        int min_index = 0;
        for (int i = 1; i < history_count; i++) {
            if (history_records[i].score < min_score) {
                min_score = history_records[i].score;
                min_index = i;
            }
        }
        // 删除最旧的记录，将后面的记录前移
        for (int i = min_index; i < history_count - 1; i++) {
            history_records[i] = history_records[i + 1];
        }
        history_count--;
        printf("[2048] save_history_record: 记录已满，删除最低分记录，剩余记录数=%d\n", history_count);
    }
    
    // 添加新记录
    history_records[history_count].score = score;
    history_records[history_count].timestamp = time(NULL);
    history_records[history_count].game_time = game_time;
    last_saved_index = history_count;
    history_count++;
    
    printf("[2048] save_history_record: 已添加记录到内存, 新记录数=%d, last_saved_index=%d\n", 
           history_count, last_saved_index);
    
    // 保存到文件
    FILE *fp = fopen(HISTORY_FILE_PATH, "w");
    if (fp) {
        int written = 0;
        for (int i = 0; i < history_count; i++) {
            fprintf(fp, "%d %ld %d\n", history_records[i].score, history_records[i].timestamp, history_records[i].game_time);
            written++;
        }
        fclose(fp);
        printf("[2048] save_history_record: 已写入文件 %d 条记录到 %s\n", written, HISTORY_FILE_PATH);
    } else {
        printf("[2048] save_history_record: 错误：无法打开文件 %s 进行写入\n", HISTORY_FILE_PATH);
    }
}

/**
 * @brief 从文件加载历史记录
 */
static void load_history_records(void) {
    history_count = 0;
    last_saved_index = -1;
    
    FILE *fp = fopen(HISTORY_FILE_PATH, "r");
    if (fp) {
        time_t latest_timestamp = 0;  // 用于找到最新的记录
        
        while (history_count < MAX_HISTORY_RECORDS) {
            int score;
            time_t timestamp;
            int game_time = 0;
            // 尝试读取新格式（包含游戏时间）
            int ret = fscanf(fp, "%d %ld %d\n", &score, &timestamp, &game_time);
            if (ret >= 2) {
                // 验证数据有效性：时间戳应该合理（1970年之后，2100年之前）
                if (timestamp > 0 && timestamp < 4102444800) {
                    history_records[history_count].score = score;
                    history_records[history_count].timestamp = timestamp;
                    history_records[history_count].game_time = (ret == 3) ? game_time : 0;  // 兼容旧格式
                    
                    // 找到时间戳最大的记录（最新的记录）
                    if (timestamp > latest_timestamp) {
                        latest_timestamp = timestamp;
                        last_saved_index = history_count;
                    }
                    
                    history_count++;
                } else {
                    // 跳过无效记录
                    printf("[2048] 跳过无效历史记录: score=%d, timestamp=%ld\n", score, timestamp);
                }
            } else {
                break;
            }
        }
        fclose(fp);
        
        printf("[2048] load_history_records: 加载了 %d 条记录, last_saved_index=%d\n", 
               history_count, last_saved_index);
    }
}

/**
 * @brief 清空历史记录
 */
static void clear_history_records(void) {
    history_count = 0;
    last_saved_index = -1;
    
    // 删除文件
    unlink(HISTORY_FILE_PATH);
    
    printf("[2048] 历史记录已清空\n");
}

/**
 * @brief 比较函数：按分数从高到低排序
 */
static int compare_records(const void *a, const void *b) {
    const history_record_t *ra = (const history_record_t *)a;
    const history_record_t *rb = (const history_record_t *)b;
    return rb->score - ra->score;  // 降序排列
}

/**
 * @brief 格式化时间字符串
 */
static void format_time_string(char *buf, size_t buf_size, time_t timestamp) {
    struct tm *tm_info = localtime(&timestamp);
    if (tm_info) {
        strftime(buf, buf_size, "%Y-%m-%d %H:%M:%S", tm_info);
    } else {
        snprintf(buf, buf_size, "未知时间");
    }
}

/**
 * @brief 格式化游戏时间字符串（秒转换为 MM:SS 或 HH:MM:SS）
 */
static void format_game_time_string(char *buf, size_t buf_size, int seconds) {
    int hours = seconds / 3600;
    int minutes = (seconds % 3600) / 60;
    int secs = seconds % 60;
    
    if (hours > 0) {
        snprintf(buf, buf_size, "%02d:%02d:%02d", hours, minutes, secs);
    } else {
        snprintf(buf, buf_size, "%02d:%02d", minutes, secs);
    }
}

/**
 * @brief 显示历史记录窗口
 */
static void show_history_window(void) {
    // 重要：每次显示历史记录窗口时，都重新加载历史记录（从文件读取最新记录）
    // 这样可以确保显示的是最新的记录，包括刚刚保存的记录
    load_history_records();
    
    // 如果窗口已存在，先删除旧窗口，然后重新创建（确保显示最新记录）
    if (history_window) {
        lv_obj_del(history_window);
        history_window = NULL;
    }
    
    // 创建历史记录窗口
    history_window = lv_obj_create(NULL);
    lv_obj_set_size(history_window, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_style_bg_color(history_window, lv_color_hex(0xFAF8EF), 0);
    lv_obj_set_style_border_width(history_window, 0, 0);
    lv_obj_set_style_pad_all(history_window, 0, 0);
    
    extern const lv_font_t SourceHanSansSC_VF;
    
    // 创建标题
    lv_obj_t *title = lv_label_create(history_window);
    lv_label_set_text(title, "历史记录");
    lv_obj_set_style_text_font(title, &SourceHanSansSC_VF, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0x776E65), 0);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(title, LV_HOR_RES);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 20);
    
    // 创建滚动列表容器
    lv_obj_t *list_container = lv_obj_create(history_window);
    lv_obj_set_size(list_container, LV_HOR_RES - 40, LV_VER_RES - 120);
    lv_obj_align(list_container, LV_ALIGN_CENTER, 0, 20);
    lv_obj_set_style_bg_color(list_container, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_border_width(list_container, 2, 0);
    lv_obj_set_style_border_color(list_container, lv_color_hex(0xBBADA0), 0);
    lv_obj_set_style_radius(list_container, 6, 0);
    lv_obj_set_style_pad_all(list_container, 10, 0);
    lv_obj_set_flex_flow(list_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scroll_dir(list_container, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(list_container, LV_SCROLLBAR_MODE_AUTO);
    
    // 排序历史记录（按分数从高到低）
    history_record_t sorted_records[MAX_HISTORY_RECORDS];
    memcpy(sorted_records, history_records, sizeof(history_record_t) * history_count);
    qsort(sorted_records, history_count, sizeof(history_record_t), compare_records);
    
    // 显示历史记录
    if (history_count == 0) {
        lv_obj_t *empty_label = lv_label_create(list_container);
        lv_label_set_text(empty_label, "暂无历史记录");
        lv_obj_set_style_text_font(empty_label, &SourceHanSansSC_VF, 0);
        lv_obj_set_style_text_color(empty_label, lv_color_hex(0x776E65), 0);
        lv_obj_set_style_text_align(empty_label, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_width(empty_label, LV_PCT(100));
    } else {
        for (int i = 0; i < history_count; i++) {
            // 创建记录项容器
            lv_obj_t *record_item = lv_obj_create(list_container);
            lv_obj_set_size(record_item, LV_PCT(100), 60);
            lv_obj_set_style_border_width(record_item, 1, 0);
            lv_obj_set_style_border_color(record_item, lv_color_hex(0xBBADA0), 0);
            lv_obj_set_style_radius(record_item, 4, 0);
            lv_obj_set_style_pad_all(record_item, 10, 0);
            lv_obj_set_flex_flow(record_item, LV_FLEX_FLOW_ROW);
            lv_obj_set_flex_align(record_item, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
            lv_obj_clear_flag(record_item, LV_OBJ_FLAG_SCROLLABLE);
            
            // 检查是否是最近一次记录（需要找到原始索引）
            bool is_latest = false;
            for (int j = 0; j < history_count; j++) {
                if (history_records[j].score == sorted_records[i].score &&
                    history_records[j].timestamp == sorted_records[i].timestamp &&
                    j == last_saved_index) {
                    is_latest = true;
                    break;
                }
            }
            
            // 如果是最近一次记录，高亮显示
            if (is_latest) {
                lv_obj_set_style_bg_color(record_item, lv_color_hex(0xEDCF72), 0);  // 高亮背景色
                lv_obj_set_style_bg_opa(record_item, LV_OPA_COVER, 0);
            } else {
                lv_obj_set_style_bg_color(record_item, lv_color_hex(0xFFFFFF), 0);
                lv_obj_set_style_bg_opa(record_item, LV_OPA_COVER, 0);
            }
            
            // 分数标签
            char score_text[32];
            snprintf(score_text, sizeof(score_text), "分数: %d", sorted_records[i].score);
            lv_obj_t *record_score_label = lv_label_create(record_item);
            lv_label_set_text(record_score_label, score_text);
            lv_obj_set_style_text_font(record_score_label, &SourceHanSansSC_VF, 0);
            lv_obj_set_style_text_color(record_score_label, lv_color_hex(0x776E65), 0);
            
            // 游戏时间标签
            char game_time_str[32];
            format_game_time_string(game_time_str, sizeof(game_time_str), sorted_records[i].game_time);
            lv_obj_t *game_time_label = lv_label_create(record_item);
            lv_label_set_text(game_time_label, game_time_str);
            lv_obj_set_style_text_font(game_time_label, &SourceHanSansSC_VF, 0);
            lv_obj_set_style_text_color(game_time_label, lv_color_hex(0x776E65), 0);
            
            // 完成时间标签
            char time_text[64];
            format_time_string(time_text, sizeof(time_text), sorted_records[i].timestamp);
            lv_obj_t *time_label = lv_label_create(record_item);
            lv_label_set_text(time_label, time_text);
            lv_obj_set_style_text_font(time_label, &SourceHanSansSC_VF, 0);
            lv_obj_set_style_text_color(time_label, lv_color_hex(0x776E65), 0);
        }
    }
    
    // 创建按钮容器（底部）
    lv_obj_t *btn_container = lv_obj_create(history_window);
    lv_obj_set_size(btn_container, LV_HOR_RES - 40, 60);
    lv_obj_align(btn_container, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_obj_set_style_bg_opa(btn_container, LV_OPA_0, 0);
    lv_obj_set_style_border_width(btn_container, 0, 0);
    lv_obj_set_flex_flow(btn_container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btn_container, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(btn_container, LV_OBJ_FLAG_SCROLLABLE);
    
    // 创建清空记录按钮
    lv_obj_t *clear_btn = lv_btn_create(btn_container);
    lv_obj_set_size(clear_btn, 150, 50);
    lv_obj_set_style_bg_color(clear_btn, lv_color_hex(0xFF5722), 0);  // 红色
    lv_obj_set_style_radius(clear_btn, 6, 0);
    lv_obj_set_style_border_width(clear_btn, 0, 0);
    lv_obj_t *clear_label = lv_label_create(clear_btn);
    lv_label_set_text(clear_label, "清空记录");
    lv_obj_set_style_text_font(clear_label, &SourceHanSansSC_VF, 0);
    lv_obj_set_style_text_color(clear_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(clear_label);
    lv_obj_add_event_cb(clear_btn, clear_history_btn_cb, LV_EVENT_CLICKED, NULL);
    
    // 创建返回按钮
    lv_obj_t *back_btn = lv_btn_create(btn_container);
    lv_obj_set_size(back_btn, 150, 50);
    lv_obj_set_style_bg_color(back_btn, lv_color_hex(0x8F7A66), 0);
    lv_obj_set_style_radius(back_btn, 6, 0);
    lv_obj_set_style_border_width(back_btn, 0, 0);
    lv_obj_t *back_label = lv_label_create(back_btn);
    lv_label_set_text(back_label, "返回游戏");
    lv_obj_set_style_text_font(back_label, &SourceHanSansSC_VF, 0);
    lv_obj_set_style_text_color(back_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(back_label);
    lv_obj_add_event_cb(back_btn, history_back_btn_cb, LV_EVENT_CLICKED, NULL);
    
    lv_scr_load(history_window);
}

/**
 * @brief 历史记录返回按钮回调
 */
static void history_back_btn_cb(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    
    if (history_window) {
        lv_obj_add_flag(history_window, LV_OBJ_FLAG_HIDDEN);
    }
    
    if (game_window) {
        lv_obj_clear_flag(game_window, LV_OBJ_FLAG_HIDDEN);
        lv_scr_load(game_window);
    }
}

/**
 * @brief 清空历史记录按钮回调
 */
static void clear_history_btn_cb(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    
    // 清空历史记录
    clear_history_records();
    
    // 重新加载历史记录窗口（会显示"暂无历史记录"）
    if (history_window) {
        lv_obj_del(history_window);
        history_window = NULL;
    }
    show_history_window();
}

/**
 * @brief 开始游戏按钮回调
 */
static void start_game_btn_cb(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    
    // 如果游戏已结束，先重置游戏状态（刷新新游戏）
    if (game_state.game_over) {
        // 如果游戏结束且未保存，保存历史记录
        if (game_state.score > 0 && !game_over_saved) {
            int final_time = game_timer_running ? (time(NULL) - game_start_time) : game_elapsed_time;
            if (final_time == 0 && game_start_time > 0) {
                final_time = time(NULL) - game_start_time;
            }
            if (final_time == 0) {
                final_time = 1;
            }
            save_history_record(game_state.score, final_time);
            game_over_saved = true;
            printf("[2048] 保存历史记录: 分数=%d, 时间=%d秒\n", game_state.score, final_time);
        }
        
        // 重置游戏状态（刷新新游戏）
        game_2048_reset(&game_state);
        game_over_saved = false;
    }
    
    // 隐藏开始按钮
    if (start_game_btn) {
        lv_obj_add_flag(start_game_btn, LV_OBJ_FLAG_HIDDEN);
    }
    
    // 开始游戏
    game_started = true;
    
    // 开始计时
    game_start_time = time(NULL);
    game_elapsed_time = 0;
    game_timer_running = true;
    
    // 恢复定时器
    if (timer_update_timer) {
        lv_timer_resume(timer_update_timer);
    }
    
    // 更新显示（确保新游戏状态被显示）
    update_game_display();
    
    printf("[2048] 游戏已开始\n");
}

/**
 * @brief 历史记录按钮回调
 */
static void history_btn_cb(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    
    show_history_window();
}

/**
 * @brief 重启按钮回调
 */
static void restart_btn_cb(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    
    // 如果游戏结束且未保存，保存历史记录
    if (game_state.game_over && game_state.score > 0 && !game_over_saved) {
        int final_time = game_timer_running ? (time(NULL) - game_start_time) : game_elapsed_time;
        save_history_record(game_state.score, final_time);
        game_over_saved = true;
        printf("[2048] 保存历史记录: 分数=%d, 时间=%d秒\n", game_state.score, final_time);
    }
    
    // 重置游戏
    game_2048_reset(&game_state);
    game_over_saved = false;
    game_started = false;  // 重置游戏开始标志
    
    // 停止计时器
    game_timer_running = false;
    game_elapsed_time = 0;
    
    // 暂停定时器
    if (timer_update_timer) {
        lv_timer_pause(timer_update_timer);
    }
    
    // 显示开始游戏按钮
    if (start_game_btn) {
        lv_obj_clear_flag(start_game_btn, LV_OBJ_FLAG_HIDDEN);
    }
    
    update_game_display();
}

/**
 * @brief 返回按钮回调
 */
static void back_btn_cb(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    
    // 如果游戏结束且未保存，保存历史记录
    if (game_state.game_over && game_state.score > 0 && !game_over_saved) {
        int final_time = game_timer_running ? (time(NULL) - game_start_time) : game_elapsed_time;
        save_history_record(game_state.score, final_time);
        game_over_saved = true;
        printf("[2048] 保存历史记录: 分数=%d, 时间=%d秒\n", game_state.score, final_time);
    }
    
    // 停止计时器
    if (game_timer_running) {
        game_timer_running = false;
    }
    
    // 暂停定时器
    if (timer_update_timer) {
        lv_timer_pause(timer_update_timer);
    }
    
    game_2048_win_hide();
}

/**
 * @brief 显示2048游戏窗口
 */
void game_2048_win_show(void) {
    // 如果窗口已存在，重置游戏并显示（刷新新游戏）
    if (game_window) {
        // 如果之前的游戏结束且未保存，保存历史记录
        if (game_state.game_over && game_state.score > 0 && !game_over_saved) {
            int final_time = game_timer_running ? (time(NULL) - game_start_time) : game_elapsed_time;
            save_history_record(game_state.score, final_time);
            game_over_saved = true;
            printf("[2048] 保存历史记录: 分数=%d, 时间=%d秒\n", game_state.score, final_time);
        }
        
        // 重置游戏状态
        game_2048_reset(&game_state);
        game_over_saved = false;
        game_started = false;  // 重置游戏开始标志，需要点击开始按钮
        
        // 停止计时器（不自动开始）
        game_timer_running = false;
        game_elapsed_time = 0;
        
        // 暂停定时器
        if (timer_update_timer) {
            lv_timer_pause(timer_update_timer);
        }
        
        // 显示开始游戏按钮（必须点击才能开始）
        if (start_game_btn) {
            lv_obj_clear_flag(start_game_btn, LV_OBJ_FLAG_HIDDEN);
        }
        
        lv_obj_clear_flag(game_window, LV_OBJ_FLAG_HIDDEN);
        lv_scr_load(game_window);
        lv_refr_now(NULL);
        
        // 如果触摸线程未运行，重新启动
        if (!touch_thread_running) {
            touch_thread_running = true;
            if (pthread_create(&touch_thread, NULL, touch_thread_func, NULL) != 0) {
                printf("[2048] 无法创建触摸控制线程\n");
                touch_thread_running = false;
            }
        }
        
        // 更新显示
        update_game_display();
        return;
    }
    
    // 加载历史记录
    load_history_records();
    
    // 初始化游戏
    srand(time(NULL));
    game_2048_init(&game_state);
    
    // 初始化游戏状态（不开始计时，等待点击开始按钮）
    game_start_time = 0;
    game_elapsed_time = 0;
    game_timer_running = false;
    game_over_saved = false;
    game_started = false;  // 游戏未开始，等待点击开始按钮
    
    // 创建定时器，每1秒更新一次计时器显示（初始为暂停状态）
    if (timer_update_timer == NULL) {
        timer_update_timer = lv_timer_create(timer_update_cb, 1000, NULL);  // 1000ms = 1秒
        lv_timer_pause(timer_update_timer);  // 初始暂停，等待游戏开始
    } else {
        lv_timer_pause(timer_update_timer);  // 暂停，等待游戏开始
    }
    
    // 创建游戏窗口
    game_window = lv_obj_create(NULL);
    lv_obj_set_size(game_window, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_style_bg_color(game_window, lv_color_hex(0xFAF8EF), 0);
    lv_obj_set_style_border_width(game_window, 0, 0);
    lv_obj_set_style_pad_all(game_window, 0, 0);
    
    extern const lv_font_t SourceHanSansSC_VF;
    
    // 创建左侧控制面板（宽度280，高度480，左侧）
    lv_obj_t *left_panel = lv_obj_create(game_window);
    lv_obj_set_size(left_panel, 280, 480);
    lv_obj_set_pos(left_panel, 0, 0);
    lv_obj_set_style_bg_color(left_panel, lv_color_hex(0xEEE4DA), 0);
    lv_obj_set_style_border_width(left_panel, 0, 0);
    lv_obj_set_style_pad_all(left_panel, 20, 0);
    lv_obj_set_style_radius(left_panel, 0, 0);
    
    // 创建标题
    lv_obj_t *title = lv_label_create(left_panel);
    lv_label_set_text(title, "2048");
    lv_obj_set_style_text_font(title, &SourceHanSansSC_VF, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0x776E65), 0);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(title, 240);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 20);
    
    // 创建分数显示区域（带背景框）
    lv_obj_t *score_panel = lv_obj_create(left_panel);
    lv_obj_set_size(score_panel, 240, 80);
    lv_obj_set_style_bg_color(score_panel, lv_color_hex(0xBBADA0), 0);
    lv_obj_set_style_border_width(score_panel, 0, 0);
    lv_obj_set_style_radius(score_panel, 6, 0);
    lv_obj_set_style_pad_all(score_panel, 10, 0);
    lv_obj_align(score_panel, LV_ALIGN_TOP_MID, 0, 80);
    lv_obj_clear_flag(score_panel, LV_OBJ_FLAG_SCROLLABLE);  // 取消滑动列表格式，固定格式
    
    score_label = lv_label_create(score_panel);
    char score_text[64];
    snprintf(score_text, sizeof(score_text), "分数: %d\n时间: 00:00", game_state.score);
    lv_label_set_text(score_label, score_text);
    lv_obj_set_style_text_font(score_label, &SourceHanSansSC_VF, 0);
    lv_obj_set_style_text_color(score_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_align(score_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(score_label, 220);
    lv_obj_center(score_label);
    
    // 创建重启按钮
    restart_btn = lv_btn_create(left_panel);
    lv_obj_set_size(restart_btn, 240, 60);
    lv_obj_set_style_bg_color(restart_btn, lv_color_hex(0x8F7A66), 0);
    lv_obj_set_style_radius(restart_btn, 6, 0);
    lv_obj_set_style_border_width(restart_btn, 0, 0);
    lv_obj_align(restart_btn, LV_ALIGN_TOP_MID, 0, 180);
    lv_obj_t *restart_label = lv_label_create(restart_btn);
    lv_label_set_text(restart_label, "重新开始");
    lv_obj_set_style_text_font(restart_label, &SourceHanSansSC_VF, 0);
    lv_obj_set_style_text_color(restart_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(restart_label);
    lv_obj_add_event_cb(restart_btn, restart_btn_cb, LV_EVENT_CLICKED, NULL);
    
    // 创建历史记录按钮
    history_btn = lv_btn_create(left_panel);
    lv_obj_set_size(history_btn, 240, 60);
    lv_obj_set_style_bg_color(history_btn, lv_color_hex(0x8F7A66), 0);
    lv_obj_set_style_radius(history_btn, 6, 0);
    lv_obj_set_style_border_width(history_btn, 0, 0);
    lv_obj_align(history_btn, LV_ALIGN_TOP_MID, 0, 260);
    lv_obj_t *history_label = lv_label_create(history_btn);
    lv_label_set_text(history_label, "历史记录");
    lv_obj_set_style_text_font(history_label, &SourceHanSansSC_VF, 0);
    lv_obj_set_style_text_color(history_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(history_label);
    lv_obj_add_event_cb(history_btn, history_btn_cb, LV_EVENT_CLICKED, NULL);
    
    // 创建返回按钮
    lv_obj_t *back_btn = lv_btn_create(left_panel);
    lv_obj_set_size(back_btn, 240, 60);
    lv_obj_set_style_bg_color(back_btn, lv_color_hex(0x8F7A66), 0);
    lv_obj_set_style_radius(back_btn, 6, 0);
    lv_obj_set_style_border_width(back_btn, 0, 0);
    lv_obj_align(back_btn, LV_ALIGN_TOP_MID, 0, 340);
    lv_obj_t *back_label = lv_label_create(back_btn);
    lv_label_set_text(back_label, "返回主页");
    lv_obj_set_style_text_font(back_label, &SourceHanSansSC_VF, 0);
    lv_obj_set_style_text_color(back_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(back_label);
    lv_obj_add_event_cb(back_btn, back_btn_cb, LV_EVENT_CLICKED, NULL);
    
    // 创建右侧游戏网格容器（420x420，确保内容不被折叠，无滚动条）
    lv_obj_t *grid_container = lv_obj_create(game_window);
    lv_obj_set_size(grid_container, 420, 420);
    lv_obj_set_pos(grid_container, 300, 30);
    lv_obj_set_style_bg_color(grid_container, lv_color_hex(0xBBADA0), 0);
    lv_obj_set_style_border_width(grid_container, 0, 0);
    lv_obj_set_style_pad_all(grid_container, 10, 0);
    lv_obj_set_style_radius(grid_container, 6, 0);
    lv_obj_clear_flag(grid_container, LV_OBJ_FLAG_SCROLLABLE);  // 禁用滚动
    
    // 创建4x4网格（每个方块90x90，间距10，确保不折叠）
    int tile_size = 90;
    int spacing = 10;
    int start_x = 10;
    int start_y = 10;
    
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            game_grid[i][j] = lv_btn_create(grid_container);
            lv_obj_set_size(game_grid[i][j], tile_size, tile_size);
            lv_obj_set_pos(game_grid[i][j], start_x + j * (tile_size + spacing), 
                          start_y + i * (tile_size + spacing));
            lv_obj_set_style_bg_color(game_grid[i][j], get_tile_color(0), 0);
            lv_obj_set_style_border_width(game_grid[i][j], 0, 0);
            lv_obj_set_style_radius(game_grid[i][j], 3, 0);
            lv_obj_clear_flag(game_grid[i][j], LV_OBJ_FLAG_CLICKABLE);
            
            // 创建文字标签
            lv_obj_t *label = lv_label_create(game_grid[i][j]);
            lv_label_set_text(label, "");
            lv_obj_set_style_text_font(label, &SourceHanSansSC_VF, 0);
            lv_obj_center(label);
        }
    }
    
    // 创建开始游戏按钮（在游戏区中央）
    start_game_btn = lv_btn_create(grid_container);
    lv_obj_set_size(start_game_btn, 200, 80);
    lv_obj_align(start_game_btn, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(start_game_btn, lv_color_hex(0x4CAF50), 0);  // 绿色
    lv_obj_set_style_radius(start_game_btn, 10, 0);
    lv_obj_set_style_border_width(start_game_btn, 0, 0);
    lv_obj_move_foreground(start_game_btn);  // 确保在最上层
    lv_obj_t *start_label = lv_label_create(start_game_btn);
    lv_label_set_text(start_label, "开始游戏");
    lv_obj_set_style_text_font(start_label, &SourceHanSansSC_VF, 0);
    lv_obj_set_style_text_color(start_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(start_label);
    lv_obj_add_event_cb(start_game_btn, start_game_btn_cb, LV_EVENT_CLICKED, NULL);
    
    // 创建游戏结束提示（在游戏区上方）
    game_over_label = lv_label_create(game_window);
    lv_label_set_text(game_over_label, "游戏结束！");
    lv_obj_set_style_text_font(game_over_label, &SourceHanSansSC_VF, 0);
    lv_obj_set_style_text_color(game_over_label, lv_color_hex(0x776E65), 0);
    lv_obj_set_pos(game_over_label, 300, 20);
    lv_obj_add_flag(game_over_label, LV_OBJ_FLAG_HIDDEN);
    
    // 更新显示
    update_game_display();
    
    // 切换到游戏窗口
    lv_scr_load(game_window);
    
    // 启动触摸控制线程
    touch_thread_running = true;
    if (pthread_create(&touch_thread, NULL, touch_thread_func, NULL) != 0) {
        printf("[2048] 无法创建触摸控制线程\n");
        touch_thread_running = false;
    }
}

/**
 * @brief 更新2048游戏显示（在主线程中调用，线程安全）
 */
void game_2048_win_update_display(void) {
    update_game_display();
}

/**
 * @brief 隐藏2048游戏窗口
 */
void game_2048_win_hide(void) {
    // 先停止触摸控制线程（参考clock_win_hide的实现）
    if (touch_thread_running) {
        touch_thread_running = false;
        
        // 注意：不关闭触摸屏设备，因为它是全局共享的
        // 只需要等待线程退出即可
        
        // 等待线程退出（参考clock_win_hide，直接join）
        if (touch_thread != 0) {
            pthread_join(touch_thread, NULL);
            touch_thread = 0;
        }
    }
    
    // 隐藏窗口而不是删除（参考clock_win_hide的实现）
    if (game_window) {
        lv_obj_add_flag(game_window, LV_OBJ_FLAG_HIDDEN);
    }
    
    // 切换到主屏幕（参考led_win_event_handler的实现）
    extern lv_obj_t *main_screen;
    if (main_screen) {
        lv_obj_clear_flag(main_screen, LV_OBJ_FLAG_HIDDEN);
        lv_scr_load(main_screen);
        lv_refr_now(NULL);  // 强制刷新显示
    }
}

