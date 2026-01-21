/**
 * @file draw_protocol.h
 * @brief 协作绘图协议定义
 * 
 * 定义客户端与服务器之间的通信协议，包括绘图操作的数据结构
 * 和消息格式，支持线条绘制、图形填充等操作。
 */

#ifndef DRAW_PROTOCOL_H
#define DRAW_PROTOCOL_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// 协议版本
#define DRAW_PROTOCOL_VERSION 1

// 消息类型
typedef enum {
    MSG_TYPE_DRAW_LINE = 1,      // 绘制线条
    MSG_TYPE_DRAW_POINT = 2,     // 绘制点
    MSG_TYPE_CLEAR = 3,          // 清屏
    MSG_TYPE_ERASE = 4,           // 橡皮擦
    MSG_TYPE_USER_JOIN = 10,      // 用户加入
    MSG_TYPE_USER_LEAVE = 11,     // 用户离开
    MSG_TYPE_SYNC_REQUEST = 20,   // 同步请求
    MSG_TYPE_SYNC_RESPONSE = 21,  // 同步响应
} draw_msg_type_t;

// 绘图操作数据结构
typedef struct {
    uint32_t user_id;            // 用户ID（唯一标识）
    uint32_t timestamp;          // 时间戳（毫秒）
    uint16_t x;                  // X坐标
    uint16_t y;                  // Y坐标
    uint16_t prev_x;             // 上一个X坐标（用于画线）
    uint16_t prev_y;             // 上一个Y坐标（用于画线）
    uint8_t pen_size;            // 笔触大小（1-3）
    uint32_t color;              // 颜色（BGRA格式）
    uint8_t msg_type;            // 消息类型
    bool is_eraser;              // 是否为橡皮擦模式
} draw_operation_t;

// 消息头（固定大小，便于解析）
typedef struct {
    uint8_t version;             // 协议版本
    uint8_t msg_type;            // 消息类型
    uint16_t data_len;           // 数据长度
    uint32_t user_id;            // 用户ID
    uint32_t timestamp;          // 时间戳
} draw_msg_header_t;

// 完整消息结构
typedef struct {
    draw_msg_header_t header;
    uint8_t data[];              // 可变长度数据
} draw_msg_t;

// 函数声明
int draw_operation_encode(const draw_operation_t *op, uint8_t *buffer, size_t buffer_size);
int draw_operation_decode(const uint8_t *buffer, size_t buffer_size, draw_operation_t *op);
size_t draw_operation_get_size(const draw_operation_t *op);

#endif /* DRAW_PROTOCOL_H */
