/**
 * @file collaborative_draw.c
 * @brief 实时多人在线协作绘图系统实现
 * 
 * 使用巴法云TCP协议实现设备间通信
 */

#include "collaborative_draw.h"
#include "draw_protocol.h"
#include "bemfa_tcp_client.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <time.h>

// 协作绘图模块状态
static struct {
    collaborative_draw_config_t config;
    collaborative_draw_state_t state;
    bemfa_tcp_handle_t bemfa_tcp_handle;  // 巴法云TCP客户端句柄
    pthread_t recv_thread;
    pthread_t send_thread;
    bool threads_running;
    pthread_mutex_t send_mutex;
    void (*remote_draw_callback)(uint16_t x, uint16_t y, uint16_t prev_x, uint16_t prev_y,
                                 uint8_t pen_size, uint32_t color, bool is_eraser, void *user_data);
    void *remote_draw_user_data;
} g_collab_draw = {0};

// 二进制数据转十六进制字符串
static void bin_to_hex(const uint8_t *bin, int bin_len, char *hex, int hex_size) {
    int len = 0;
    for (int i = 0; i < bin_len && len < hex_size - 1; i++) {
        len += snprintf(hex + len, hex_size - len, "%02X", bin[i]);
    }
    hex[len] = '\0';
}

// 十六进制字符串转二进制数据
static int hex_to_bin(const char *hex, uint8_t *bin, int bin_size) {
    int hex_len = strlen(hex);
    if (hex_len % 2 != 0) {
        return -1;
    }
    
    int bin_len = 0;
    for (int i = 0; i < hex_len && bin_len < bin_size; i += 2) {
        char hex_byte[3] = {hex[i], hex[i+1], '\0'};
        bin[bin_len++] = (uint8_t)strtoul(hex_byte, NULL, 16);
    }
    return bin_len;
}

// 巴法云TCP消息处理器
static void bemfa_tcp_message_handler(const char *topic, const char *msg, size_t msg_len, void *user_data) {
    (void)user_data;
    (void)topic;
    
    // 将十六进制字符串解码为二进制数据
    uint8_t buffer[256];
    int bin_len = hex_to_bin(msg, buffer, sizeof(buffer));
    if (bin_len <= 0) {
        printf("[协作绘图] 解码消息失败\n");
        return;
    }
    
    // 解码绘图操作
    draw_operation_t op;
    if (draw_operation_decode(buffer, bin_len, &op) == 0) {
        // 调用回调函数绘制
        if (g_collab_draw.remote_draw_callback) {
            g_collab_draw.remote_draw_callback(
                op.x, op.y, op.prev_x, op.prev_y,
                op.pen_size, op.color, op.is_eraser,
                g_collab_draw.remote_draw_user_data);
        }
    }
}

// 网络接收线程函数
static void* network_recv_thread_func(void *arg) {
    (void)arg;
    
    printf("[协作绘图] 网络接收线程启动\n");
    
    while (g_collab_draw.threads_running) {
        if (g_collab_draw.state != COLLAB_DRAW_STATE_CONNECTED) {
            usleep(100000);  // 100ms
            continue;
        }
        
        // 使用bemfa_tcp_loop处理接收
        if (g_collab_draw.bemfa_tcp_handle) {
            int ret = bemfa_tcp_loop(g_collab_draw.bemfa_tcp_handle);
            if (ret < 0) {
                bemfa_tcp_state_t tcp_state = bemfa_tcp_get_state(g_collab_draw.bemfa_tcp_handle);
                if (tcp_state == BEMFA_TCP_STATE_DISCONNECTED || tcp_state == BEMFA_TCP_STATE_ERROR) {
                    printf("[协作绘图] 巴法云TCP连接已断开，更新状态\n");
                    g_collab_draw.state = COLLAB_DRAW_STATE_DISCONNECTED;
                }
            }
        } else {
            // TCP句柄无效，连接已断开
            g_collab_draw.state = COLLAB_DRAW_STATE_DISCONNECTED;
        }
        
        usleep(10000);  // 10ms
    }
    
    printf("[协作绘图] 网络接收线程退出\n");
    return NULL;
}

// 网络发送线程函数（用于定期发送心跳）
static void* network_send_thread_func(void *arg) {
    (void)arg;
    
    printf("[协作绘图] 网络发送线程启动\n");
    
    while (g_collab_draw.threads_running) {
        if (g_collab_draw.state != COLLAB_DRAW_STATE_CONNECTED) {
            usleep(100000);  // 100ms
            continue;
        }
        
        // 心跳由bemfa_tcp_loop自动处理（60秒一次）
        usleep(100000);  // 100ms
    }
    
    printf("[协作绘图] 网络发送线程退出\n");
    return NULL;
}

int collaborative_draw_init(const collaborative_draw_config_t *config) {
    if (!config) {
        return -1;
    }
    
    memset(&g_collab_draw, 0, sizeof(g_collab_draw));
    memcpy(&g_collab_draw.config, config, sizeof(collaborative_draw_config_t));
    g_collab_draw.state = COLLAB_DRAW_STATE_DISCONNECTED;
    g_collab_draw.bemfa_tcp_handle = NULL;
    pthread_mutex_init(&g_collab_draw.send_mutex, NULL);
    
    printf("[协作绘图] 模块初始化完成\n");
    return 0;
}

int collaborative_draw_start(void) {
    // 如果已经连接，先停止并清理（允许重新连接）
    if (g_collab_draw.state == COLLAB_DRAW_STATE_CONNECTED) {
        collaborative_draw_stop();
    }
    
    // 确保所有状态都被重置（清理之前可能的残留状态）
    g_collab_draw.threads_running = false;
    g_collab_draw.recv_thread = 0;
    g_collab_draw.send_thread = 0;
    
    // 清理之前的TCP连接
    if (g_collab_draw.bemfa_tcp_handle) {
        bemfa_tcp_cleanup(g_collab_draw.bemfa_tcp_handle);
        g_collab_draw.bemfa_tcp_handle = NULL;
    }
    
    g_collab_draw.state = COLLAB_DRAW_STATE_CONNECTING;
    
    // 巴法云TCP协议模式
    bemfa_tcp_config_t tcp_config = {0};
    strncpy(tcp_config.server_host, g_collab_draw.config.server_host, sizeof(tcp_config.server_host) - 1);
    tcp_config.server_port = g_collab_draw.config.server_port;
    strncpy(tcp_config.uid, g_collab_draw.config.private_key, sizeof(tcp_config.uid) - 1);
    strncpy(tcp_config.topic, g_collab_draw.config.device_name, sizeof(tcp_config.topic) - 1);
    
    g_collab_draw.bemfa_tcp_handle = bemfa_tcp_init(&tcp_config);
    if (!g_collab_draw.bemfa_tcp_handle) {
        printf("[协作绘图] 巴法云TCP客户端初始化失败\n");
        g_collab_draw.state = COLLAB_DRAW_STATE_DISCONNECTED;
        return -1;
    }
    
    // 设置消息回调
    bemfa_tcp_set_message_callback(g_collab_draw.bemfa_tcp_handle, bemfa_tcp_message_handler, NULL);
    
    // 连接服务器
    if (bemfa_tcp_connect(g_collab_draw.bemfa_tcp_handle) != 0) {
        printf("[协作绘图] 巴法云TCP连接失败\n");
        bemfa_tcp_cleanup(g_collab_draw.bemfa_tcp_handle);
        g_collab_draw.bemfa_tcp_handle = NULL;
        g_collab_draw.state = COLLAB_DRAW_STATE_DISCONNECTED;
        return -1;
    }
    
    // 订阅主题（使用设备名称作为主题）
    char topic[128];
    snprintf(topic, sizeof(topic), "%s", g_collab_draw.config.device_name);
    
    if (bemfa_tcp_subscribe(g_collab_draw.bemfa_tcp_handle, topic) != 0) {
        printf("[协作绘图] 订阅主题失败: %s\n", topic);
        bemfa_tcp_disconnect(g_collab_draw.bemfa_tcp_handle);
        bemfa_tcp_cleanup(g_collab_draw.bemfa_tcp_handle);
        g_collab_draw.bemfa_tcp_handle = NULL;
        g_collab_draw.state = COLLAB_DRAW_STATE_DISCONNECTED;
        return -1;
    }
    
    printf("[协作绘图] 订阅主题成功: %s (TCP协议)\n", topic);
    
    // 启动接收和发送线程
    g_collab_draw.threads_running = true;
    pthread_create(&g_collab_draw.recv_thread, NULL, network_recv_thread_func, NULL);
    pthread_create(&g_collab_draw.send_thread, NULL, network_send_thread_func, NULL);
    
    g_collab_draw.state = COLLAB_DRAW_STATE_CONNECTED;
    printf("[协作绘图] 连接成功\n");
    
    return 0;
}

void collaborative_draw_stop(void) {
    g_collab_draw.threads_running = false;
    
    if (g_collab_draw.recv_thread) {
        pthread_join(g_collab_draw.recv_thread, NULL);
    }
    if (g_collab_draw.send_thread) {
        pthread_join(g_collab_draw.send_thread, NULL);
    }
    
    if (g_collab_draw.bemfa_tcp_handle) {
        bemfa_tcp_disconnect(g_collab_draw.bemfa_tcp_handle);
    }
    
    g_collab_draw.state = COLLAB_DRAW_STATE_DISCONNECTED;
    printf("[协作绘图] 已断开连接\n");
}

int collaborative_draw_send_operation(uint16_t x, uint16_t y, 
                                      uint16_t prev_x, uint16_t prev_y,
                                      uint8_t pen_size, uint32_t color, 
                                      bool is_eraser) {
    // 检查状态，如果未连接则静默失败
    if (g_collab_draw.state != COLLAB_DRAW_STATE_CONNECTED) {
        return -1;
    }
    
    if (!g_collab_draw.bemfa_tcp_handle) {
        return -1;
    }
    
    draw_operation_t op = {0};
    op.user_id = g_collab_draw.config.user_id;
    op.timestamp = (uint32_t)(time(NULL) * 1000);  // 毫秒时间戳
    op.x = x;
    op.y = y;
    op.prev_x = prev_x;
    op.prev_y = prev_y;
    op.pen_size = pen_size;
    op.color = color;
    op.is_eraser = is_eraser;
    op.msg_type = is_eraser ? MSG_TYPE_ERASE : MSG_TYPE_DRAW_LINE;
    
    // 编码绘图操作
    uint8_t buffer[256];
    int encoded_len = draw_operation_encode(&op, buffer, sizeof(buffer));
    if (encoded_len <= 0) {
        return -1;
    }
    
    // 将二进制数据转换为十六进制字符串
    char hex_msg[512];
    bin_to_hex(buffer, encoded_len, hex_msg, sizeof(hex_msg));
    
    // 发布消息到主题/set（推送模式，向所有订阅者推送）
    char topic_set[128];
    snprintf(topic_set, sizeof(topic_set), "%s/set", g_collab_draw.config.device_name);
    
    return bemfa_tcp_publish(g_collab_draw.bemfa_tcp_handle, topic_set, hex_msg);
}

int collaborative_draw_send_clear(void) {
    // 检查状态，如果未连接则静默失败
    if (g_collab_draw.state != COLLAB_DRAW_STATE_CONNECTED) {
        return -1;
    }
    
    if (!g_collab_draw.bemfa_tcp_handle) {
        return -1;
    }
    
    draw_operation_t op = {0};
    op.user_id = g_collab_draw.config.user_id;
    op.timestamp = (uint32_t)(time(NULL) * 1000);
    op.msg_type = MSG_TYPE_CLEAR;
    
    uint8_t buffer[256];
    int encoded_len = draw_operation_encode(&op, buffer, sizeof(buffer));
    if (encoded_len <= 0) {
        return -1;
    }
    
    // 将二进制数据转换为十六进制字符串
    char hex_msg[512];
    bin_to_hex(buffer, encoded_len, hex_msg, sizeof(hex_msg));
    
    // 发布消息到主题/set（推送模式）
    char topic_set[128];
    snprintf(topic_set, sizeof(topic_set), "%s/set", g_collab_draw.config.device_name);
    
    return bemfa_tcp_publish(g_collab_draw.bemfa_tcp_handle, topic_set, hex_msg);
}

collaborative_draw_state_t collaborative_draw_get_state(void) {
    return g_collab_draw.state;
}

void collaborative_draw_set_remote_draw_callback(
    void (*callback)(uint16_t x, uint16_t y, uint16_t prev_x, uint16_t prev_y,
                     uint8_t pen_size, uint32_t color, bool is_eraser, void *user_data),
    void *user_data) {
    g_collab_draw.remote_draw_callback = callback;
    g_collab_draw.remote_draw_user_data = user_data;
}

void collaborative_draw_cleanup(void) {
    collaborative_draw_stop();
    
    if (g_collab_draw.bemfa_tcp_handle) {
        bemfa_tcp_cleanup(g_collab_draw.bemfa_tcp_handle);
        g_collab_draw.bemfa_tcp_handle = NULL;
    }
    
    pthread_mutex_destroy(&g_collab_draw.send_mutex);
    
    printf("[协作绘图] 模块清理完成\n");
}
