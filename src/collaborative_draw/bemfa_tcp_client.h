/**
 * @file bemfa_tcp_client.h
 * @brief 巴法云TCP协议客户端
 * 
 * 实现巴法云TCP协议的连接、订阅、发布功能
 * 协议文档：https://cloud.bemfa.com/docs/src/tcp_protocol.html
 */

#ifndef BEMFA_TCP_CLIENT_H
#define BEMFA_TCP_CLIENT_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// 巴法云TCP客户端状态
typedef enum {
    BEMFA_TCP_STATE_DISCONNECTED = 0,
    BEMFA_TCP_STATE_CONNECTING,
    BEMFA_TCP_STATE_CONNECTED,
    BEMFA_TCP_STATE_ERROR
} bemfa_tcp_state_t;

// 巴法云TCP配置
typedef struct {
    char server_host[256];    // 服务器地址（bemfa.com）
    uint16_t server_port;     // 服务器端口（8344）
    char uid[128];            // 用户私钥
    char topic[128];          // 主题名称
} bemfa_tcp_config_t;

// 消息回调函数
typedef void (*bemfa_tcp_message_callback_t)(const char *topic, const char *msg, size_t msg_len, void *user_data);
typedef void (*bemfa_tcp_state_callback_t)(bemfa_tcp_state_t state, void *user_data);

// 巴法云TCP客户端句柄
typedef void* bemfa_tcp_handle_t;

/**
 * @brief 初始化巴法云TCP客户端
 * @param config 配置参数
 * @return 客户端句柄，失败返回NULL
 */
bemfa_tcp_handle_t bemfa_tcp_init(const bemfa_tcp_config_t *config);

/**
 * @brief 连接巴法云TCP服务器
 * @param handle 客户端句柄
 * @return 成功返回0，失败返回-1
 */
int bemfa_tcp_connect(bemfa_tcp_handle_t handle);

/**
 * @brief 断开连接
 * @param handle 客户端句柄
 */
void bemfa_tcp_disconnect(bemfa_tcp_handle_t handle);

/**
 * @brief 订阅主题
 * @param handle 客户端句柄
 * @param topic 主题名称（可以多个，用逗号分隔）
 * @return 成功返回0，失败返回-1
 */
int bemfa_tcp_subscribe(bemfa_tcp_handle_t handle, const char *topic);

/**
 * @brief 发布消息
 * @param handle 客户端句柄
 * @param topic 主题名称（加/set表示推送，加/up表示只更新云端）
 * @param msg 消息内容
 * @return 成功返回0，失败返回-1
 */
int bemfa_tcp_publish(bemfa_tcp_handle_t handle, const char *topic, const char *msg);

/**
 * @brief 发送心跳
 * @param handle 客户端句柄
 * @return 成功返回0，失败返回-1
 */
int bemfa_tcp_ping(bemfa_tcp_handle_t handle);

/**
 * @brief 处理接收循环（需要在单独线程中调用）
 * @param handle 客户端句柄
 * @return 成功返回0，失败返回-1
 */
int bemfa_tcp_loop(bemfa_tcp_handle_t handle);

/**
 * @brief 获取连接状态
 * @param handle 客户端句柄
 * @return 连接状态
 */
bemfa_tcp_state_t bemfa_tcp_get_state(bemfa_tcp_handle_t handle);

/**
 * @brief 设置消息回调
 * @param handle 客户端句柄
 * @param callback 回调函数
 * @param user_data 用户数据
 */
void bemfa_tcp_set_message_callback(bemfa_tcp_handle_t handle, 
                                    bemfa_tcp_message_callback_t callback, 
                                    void *user_data);

/**
 * @brief 设置状态回调
 * @param handle 客户端句柄
 * @param callback 回调函数
 * @param user_data 用户数据
 */
void bemfa_tcp_set_state_callback(bemfa_tcp_handle_t handle,
                                  bemfa_tcp_state_callback_t callback,
                                  void *user_data);

/**
 * @brief 清理客户端资源
 * @param handle 客户端句柄
 */
void bemfa_tcp_cleanup(bemfa_tcp_handle_t handle);

#endif /* BEMFA_TCP_CLIENT_H */
