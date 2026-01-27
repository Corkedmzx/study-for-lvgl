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
    
    printf("[协作绘图] 收到消息: topic=%s, msg_len=%zu, state=%d, threads_running=%d\n", 
           topic ? topic : "NULL", msg_len, g_collab_draw.state, g_collab_draw.threads_running);
    
    // 检查状态，如果正在清理或已断开，忽略消息
    if (g_collab_draw.state != COLLAB_DRAW_STATE_CONNECTED || !g_collab_draw.threads_running) {
        printf("[协作绘图] 状态检查失败，忽略消息: state=%d, threads_running=%d\n", 
               g_collab_draw.state, g_collab_draw.threads_running);
        return;
    }
    
    // 检查回调函数是否有效
    if (!g_collab_draw.remote_draw_callback) {
        printf("[协作绘图] 警告：remote_draw_callback未设置\n");
        return;
    }
    
    // 检查消息是否有效
    if (!msg || msg_len == 0) {
        printf("[协作绘图] 消息无效: msg=%p, msg_len=%zu\n", msg, msg_len);
        return;
    }
    
    printf("[协作绘图] 开始解码消息: %s\n", msg);
    
    // 将十六进制字符串解码为二进制数据
    uint8_t buffer[256];
    int bin_len = hex_to_bin(msg, buffer, sizeof(buffer));
    if (bin_len <= 0) {
        printf("[协作绘图] 解码消息失败: hex_to_bin返回%d\n", bin_len);
        return;
    }
    
    printf("[协作绘图] 十六进制解码成功: bin_len=%d\n", bin_len);
    
    // 打印原始二进制数据（前32字节，用于调试）
    printf("[协作绘图] 原始二进制数据（前32字节）: ");
    for (int i = 0; i < bin_len && i < 32; i++) {
        printf("%02X ", buffer[i]);
    }
    printf("\n");
    
    // 解码绘图操作
    draw_operation_t op;
    if (draw_operation_decode(buffer, bin_len, &op) == 0) {
        printf("[协作绘图] 解码绘图操作成功: user_id=0x%08X, timestamp=%u, msg_type=%d, x=%d, y=%d, prev_x=%d, prev_y=%d, pen_size=%d, color=0x%08X, is_eraser=%d\n",
               op.user_id, op.timestamp, op.msg_type, op.x, op.y, op.prev_x, op.prev_y, op.pen_size, op.color, op.is_eraser);
        
        // 检查消息类型
        if (op.msg_type == MSG_TYPE_CLEAR) {
            // 清屏操作：通过回调函数传递特殊参数（pen_size=0表示清屏）
            printf("[协作绘图] 收到清屏操作\n");
            if (g_collab_draw.state == COLLAB_DRAW_STATE_CONNECTED && 
                g_collab_draw.threads_running && 
                g_collab_draw.remote_draw_callback) {
                // 调用回调函数，使用pen_size=0和color=0xFFFFFFFF（白色）表示清屏
                g_collab_draw.remote_draw_callback(
                    0, 0, 0, 0,
                    0, 0xFFFFFFFF, false,  // pen_size=0表示清屏，color=白色
                    g_collab_draw.remote_draw_user_data);
                printf("[协作绘图] 清屏操作已处理\n");
            }
            return;
        }
        
        // 检查pen_size是否有效（pen_size=0表示无效数据）
        if (op.pen_size == 0) {
            printf("[协作绘图] 警告：pen_size=0，跳过绘制（可能是无效数据）\n");
            return;
        }
        
        // 再次检查状态和回调（防止在解码过程中状态改变）
        if (g_collab_draw.state == COLLAB_DRAW_STATE_CONNECTED && 
            g_collab_draw.threads_running && 
            g_collab_draw.remote_draw_callback) {
            printf("[协作绘图] 调用remote_draw_callback\n");
            // 调用回调函数绘制（使用try-catch保护，但C中需要手动检查）
            g_collab_draw.remote_draw_callback(
                op.x, op.y, op.prev_x, op.prev_y,
                op.pen_size, op.color, op.is_eraser,
                g_collab_draw.remote_draw_user_data);
            printf("[协作绘图] remote_draw_callback调用完成\n");
        } else {
            printf("[协作绘图] 状态检查失败（解码后）: state=%d, threads_running=%d, callback=%p\n",
                   g_collab_draw.state, g_collab_draw.threads_running, g_collab_draw.remote_draw_callback);
        }
    } else {
        printf("[协作绘图] 解码绘图操作失败\n");
    }
}

// 网络接收线程函数
static void* network_recv_thread_func(void *arg) {
    (void)arg;
    
    printf("[协作绘图] 网络接收线程启动\n");
    
    while (g_collab_draw.threads_running) {
        // 检查状态，如果已断开或错误，退出循环
        if (g_collab_draw.state != COLLAB_DRAW_STATE_CONNECTED) {
            // 如果状态不是已连接，等待一段时间后退出
            usleep(100000);  // 100ms
            if (!g_collab_draw.threads_running) {
                break;  // 如果停止标志已设置，立即退出
            }
            continue;
        }
        
        // 使用bemfa_tcp_loop处理接收
        if (g_collab_draw.bemfa_tcp_handle) {
            int ret = bemfa_tcp_loop(g_collab_draw.bemfa_tcp_handle);
            if (ret < 0) {
                bemfa_tcp_state_t tcp_state = bemfa_tcp_get_state(g_collab_draw.bemfa_tcp_handle);
                if (tcp_state == BEMFA_TCP_STATE_DISCONNECTED || tcp_state == BEMFA_TCP_STATE_ERROR) {
                    printf("[协作绘图] 巴法云TCP连接已断开或错误，更新状态并停止线程\n");
                    g_collab_draw.state = COLLAB_DRAW_STATE_DISCONNECTED;
                    // 设置停止标志，让发送线程也退出
                    g_collab_draw.threads_running = false;
                    break;  // 退出循环
                }
            }
        } else {
            // TCP句柄无效，连接已断开
            printf("[协作绘图] TCP句柄无效，退出接收线程\n");
            g_collab_draw.state = COLLAB_DRAW_STATE_DISCONNECTED;
            g_collab_draw.threads_running = false;
            break;  // 退出循环
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
        // 检查状态，如果已断开或错误，退出循环
        if (g_collab_draw.state != COLLAB_DRAW_STATE_CONNECTED) {
            // 如果状态不是已连接，等待一段时间后退出
            usleep(100000);  // 100ms
            if (!g_collab_draw.threads_running) {
                break;  // 如果停止标志已设置，立即退出
            }
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
        printf("[协作绘图] 发送订阅命令失败: %s\n", topic);
        bemfa_tcp_disconnect(g_collab_draw.bemfa_tcp_handle);
        bemfa_tcp_cleanup(g_collab_draw.bemfa_tcp_handle);
        g_collab_draw.bemfa_tcp_handle = NULL;
        g_collab_draw.state = COLLAB_DRAW_STATE_DISCONNECTED;
        return -1;
    }
    
    // 启动接收和发送线程
    g_collab_draw.threads_running = true;
    pthread_create(&g_collab_draw.recv_thread, NULL, network_recv_thread_func, NULL);
    pthread_create(&g_collab_draw.send_thread, NULL, network_send_thread_func, NULL);
    
    // 注意：订阅响应是异步的，这里先设置为CONNECTED
    // 如果订阅失败（res=0），网络接收线程会检测到并更新状态
    g_collab_draw.state = COLLAB_DRAW_STATE_CONNECTED;
    printf("[协作绘图] 订阅命令已发送，等待服务器响应: %s (TCP协议)\n", topic);
    printf("[协作绘图] 注意：如果收到res=0，订阅将失败\n");
    
    return 0;
}

void collaborative_draw_stop(void) {
    // 注意：不清除回调函数，因为可能在重新连接时需要保持回调
    // 回调函数应该在真正清理模块时（collaborative_draw_cleanup）才清除
    // g_collab_draw.remote_draw_callback = NULL;
    // g_collab_draw.remote_draw_user_data = NULL;
    
    // 设置停止标志
    g_collab_draw.threads_running = false;
    
    // 等待接收线程退出（使用轮询方式，最多等待2秒）
    if (g_collab_draw.recv_thread != 0) {
        pthread_t recv_thread_id = g_collab_draw.recv_thread;  // 保存线程ID
        int wait_count = 0;
        int thread_alive = 1;
        while (wait_count < 20) {  // 最多等待2秒（20 * 100ms）
            int ret = pthread_kill(recv_thread_id, 0);
            if (ret != 0) {
                // 线程已退出（ESRCH = 线程不存在）
                thread_alive = 0;
                break;
            }
            usleep(100000);  // 100ms
            wait_count++;
        }
        if (thread_alive && wait_count >= 20) {
            printf("[协作绘图] 警告：接收线程退出超时，强制继续\n");
        } else if (thread_alive) {
            // 线程还存在，尝试join（使用保存的线程ID）
            int ret = pthread_join(recv_thread_id, NULL);
            if (ret != 0 && ret != ESRCH) {
                printf("[协作绘图] 警告：接收线程join失败: %s (ret=%d)\n", strerror(ret), ret);
            }
        }
        g_collab_draw.recv_thread = 0;  // 重置线程ID
    }
    
    // 等待发送线程退出（使用轮询方式，最多等待2秒）
    if (g_collab_draw.send_thread != 0) {
        pthread_t send_thread_id = g_collab_draw.send_thread;  // 保存线程ID
        int wait_count = 0;
        int thread_alive = 1;
        while (wait_count < 20) {  // 最多等待2秒（20 * 100ms）
            int ret = pthread_kill(send_thread_id, 0);
            if (ret != 0) {
                // 线程已退出
                thread_alive = 0;
                break;
            }
            usleep(100000);  // 100ms
            wait_count++;
        }
        if (thread_alive && wait_count >= 20) {
            printf("[协作绘图] 警告：发送线程退出超时，强制继续\n");
        } else if (thread_alive) {
            // 线程还存在，尝试join（使用保存的线程ID）
            int ret = pthread_join(send_thread_id, NULL);
            if (ret != 0 && ret != ESRCH) {
                printf("[协作绘图] 警告：发送线程join失败: %s (ret=%d)\n", strerror(ret), ret);
            }
        }
        g_collab_draw.send_thread = 0;  // 重置线程ID
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
    
    // 销毁互斥锁（如果已初始化）
    // 注意：如果互斥锁未初始化，pthread_mutex_destroy 可能失败
    // 但通常不会导致段错误，只是返回错误
    int ret = pthread_mutex_destroy(&g_collab_draw.send_mutex);
    if (ret != 0 && ret != EINVAL) {
        printf("[协作绘图] 警告：销毁互斥锁失败: %s\n", strerror(ret));
    }
    
    printf("[协作绘图] 模块清理完成\n");
}
