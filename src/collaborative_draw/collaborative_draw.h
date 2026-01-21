/**
 * @file collaborative_draw.h
 * @brief 实时多人在线协作绘图系统
 * 
 * 支持多个用户通过网络实时协作绘图，使用多线程处理网络通信
 * 和图形绘制，确保绘图操作的流畅性。
 */

#ifndef COLLABORATIVE_DRAW_H
#define COLLABORATIVE_DRAW_H

#include <stdint.h>
#include <stdbool.h>

// 协作绘图配置
typedef struct {
    bool enabled;                   // 是否启用协作模式
    char server_host[256];          // 服务器地址（bemfa.com）
    uint16_t server_port;           // 服务器端口（8344）
    uint32_t user_id;               // 本地用户ID
    char room_id[64];               // 房间ID（保留，未使用）
    char device_name[128];          // 巴法云设备名称（主题名称）
    char private_key[128];          // 巴法云个人私钥（UID）
} collaborative_draw_config_t;

// 协作绘图状态
typedef enum {
    COLLAB_DRAW_STATE_DISCONNECTED = 0,
    COLLAB_DRAW_STATE_CONNECTING,
    COLLAB_DRAW_STATE_CONNECTED,
    COLLAB_DRAW_STATE_ERROR
} collaborative_draw_state_t;

/**
 * @brief 初始化协作绘图模块
 * @param config 配置参数
 * @return 成功返回0，失败返回-1
 */
int collaborative_draw_init(const collaborative_draw_config_t *config);

/**
 * @brief 启动协作绘图（连接服务器）
 * @return 成功返回0，失败返回-1
 */
int collaborative_draw_start(void);

/**
 * @brief 停止协作绘图（断开连接）
 */
void collaborative_draw_stop(void);

/**
 * @brief 发送绘图操作到服务器
 * @param x X坐标
 * @param y Y坐标
 * @param prev_x 上一个X坐标
 * @param prev_y 上一个Y坐标
 * @param pen_size 笔触大小
 * @param color 颜色（BGRA格式）
 * @param is_eraser 是否为橡皮擦
 * @return 成功返回0，失败返回-1
 */
int collaborative_draw_send_operation(uint16_t x, uint16_t y, 
                                      uint16_t prev_x, uint16_t prev_y,
                                      uint8_t pen_size, uint32_t color, 
                                      bool is_eraser);

/**
 * @brief 发送清屏操作
 * @return 成功返回0，失败返回-1
 */
int collaborative_draw_send_clear(void);

/**
 * @brief 获取当前连接状态
 * @return 连接状态
 */
collaborative_draw_state_t collaborative_draw_get_state(void);

/**
 * @brief 设置远程绘图回调（当收到其他用户的绘图操作时调用）
 * @param callback 回调函数
 * @param user_data 用户数据
 */
void collaborative_draw_set_remote_draw_callback(
    void (*callback)(uint16_t x, uint16_t y, uint16_t prev_x, uint16_t prev_y,
                     uint8_t pen_size, uint32_t color, bool is_eraser, void *user_data),
    void *user_data);

/**
 * @brief 清理协作绘图模块
 */
void collaborative_draw_cleanup(void);

#endif /* COLLABORATIVE_DRAW_H */
