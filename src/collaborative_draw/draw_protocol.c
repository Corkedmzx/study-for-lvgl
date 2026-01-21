/**
 * @file draw_protocol.c
 * @brief 协作绘图协议实现
 */

#include "draw_protocol.h"
#include <string.h>
#include <stdio.h>

/**
 * @brief 编码绘图操作为二进制数据
 * @param op 绘图操作
 * @param buffer 输出缓冲区
 * @param buffer_size 缓冲区大小
 * @return 编码后的数据长度，失败返回-1
 */
int draw_operation_encode(const draw_operation_t *op, uint8_t *buffer, size_t buffer_size) {
    if (!op || !buffer || buffer_size < sizeof(draw_operation_t)) {
        return -1;
    }
    
    // 直接内存拷贝（结构体已对齐）
    memcpy(buffer, op, sizeof(draw_operation_t));
    
    return sizeof(draw_operation_t);
}

/**
 * @brief 解码二进制数据为绘图操作
 * @param buffer 输入缓冲区
 * @param buffer_size 缓冲区大小
 * @param op 输出绘图操作
 * @return 成功返回0，失败返回-1
 */
int draw_operation_decode(const uint8_t *buffer, size_t buffer_size, draw_operation_t *op) {
    if (!buffer || !op || buffer_size < sizeof(draw_operation_t)) {
        return -1;
    }
    
    // 直接内存拷贝
    memcpy(op, buffer, sizeof(draw_operation_t));
    
    return 0;
}

/**
 * @brief 获取绘图操作的数据大小
 * @param op 绘图操作
 * @return 数据大小（字节）
 */
size_t draw_operation_get_size(const draw_operation_t *op) {
    (void)op;  // 未使用参数
    return sizeof(draw_operation_t);
}
