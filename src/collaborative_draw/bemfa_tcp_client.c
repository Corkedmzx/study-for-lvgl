/**
 * @file bemfa_tcp_client.c
 * @brief 巴法云TCP协议客户端实现
 */

#include "bemfa_tcp_client.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>

// 巴法云TCP客户端结构
typedef struct {
    bemfa_tcp_config_t config;
    int socket_fd;
    bemfa_tcp_state_t state;
    bemfa_tcp_message_callback_t msg_callback;
    void *msg_user_data;
    bemfa_tcp_state_callback_t state_callback;
    void *state_user_data;
    time_t last_ping_time;
} bemfa_tcp_client_t;

bemfa_tcp_handle_t bemfa_tcp_init(const bemfa_tcp_config_t *config) {
    if (!config) {
        return NULL;
    }
    
    bemfa_tcp_client_t *client = (bemfa_tcp_client_t *)malloc(sizeof(bemfa_tcp_client_t));
    if (!client) {
        return NULL;
    }
    
    memset(client, 0, sizeof(bemfa_tcp_client_t));
    memcpy(&client->config, config, sizeof(bemfa_tcp_config_t));
    client->socket_fd = -1;
    client->state = BEMFA_TCP_STATE_DISCONNECTED;
    client->last_ping_time = 0;
    
    return (bemfa_tcp_handle_t)client;
}

int bemfa_tcp_connect(bemfa_tcp_handle_t handle) {
    bemfa_tcp_client_t *client = (bemfa_tcp_client_t *)handle;
    if (!client) {
        return -1;
    }
    
    client->state = BEMFA_TCP_STATE_CONNECTING;
    if (client->state_callback) {
        client->state_callback(client->state, client->state_user_data);
    }
    
    // 创建socket
    client->socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (client->socket_fd < 0) {
        printf("[BemfaTCP] 创建socket失败: %s\n", strerror(errno));
        client->state = BEMFA_TCP_STATE_ERROR;
        if (client->state_callback) {
            client->state_callback(client->state, client->state_user_data);
        }
        return -1;
    }
    
    // 解析服务器地址
    struct hostent *he = gethostbyname(client->config.server_host);
    if (!he) {
        printf("[BemfaTCP] 解析服务器地址失败: %s\n", client->config.server_host);
        close(client->socket_fd);
        client->socket_fd = -1;
        client->state = BEMFA_TCP_STATE_ERROR;
        if (client->state_callback) {
            client->state_callback(client->state, client->state_user_data);
        }
        return -1;
    }
    
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(client->config.server_port);
    memcpy(&server_addr.sin_addr, he->h_addr_list[0], he->h_length);
    
    printf("[BemfaTCP] 连接服务器: %s:%d\n", client->config.server_host, client->config.server_port);
    
    // 连接服务器
    if (connect(client->socket_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        printf("[BemfaTCP] 连接失败: %s\n", strerror(errno));
        close(client->socket_fd);
        client->socket_fd = -1;
        client->state = BEMFA_TCP_STATE_ERROR;
        if (client->state_callback) {
            client->state_callback(client->state, client->state_user_data);
        }
        return -1;
    }
    
    // 设置为非阻塞模式
    int flags = fcntl(client->socket_fd, F_GETFL, 0);
    fcntl(client->socket_fd, F_SETFL, flags | O_NONBLOCK);
    
    client->state = BEMFA_TCP_STATE_CONNECTED;
    client->last_ping_time = time(NULL);
    
    if (client->state_callback) {
        client->state_callback(client->state, client->state_user_data);
    }
    
    printf("[BemfaTCP] 连接成功\n");
    return 0;
}

void bemfa_tcp_disconnect(bemfa_tcp_handle_t handle) {
    bemfa_tcp_client_t *client = (bemfa_tcp_client_t *)handle;
    if (!client) {
        return;
    }
    
    if (client->socket_fd >= 0) {
        close(client->socket_fd);
        client->socket_fd = -1;
    }
    
    client->state = BEMFA_TCP_STATE_DISCONNECTED;
    if (client->state_callback) {
        client->state_callback(client->state, client->state_user_data);
    }
    
    printf("[BemfaTCP] 已断开连接\n");
}

int bemfa_tcp_subscribe(bemfa_tcp_handle_t handle, const char *topic) {
    bemfa_tcp_client_t *client = (bemfa_tcp_client_t *)handle;
    if (!client || !topic || client->state != BEMFA_TCP_STATE_CONNECTED) {
        return -1;
    }
    
    // 构建订阅命令: cmd=1&uid=xxx&topic=xxx\r\n
    char cmd[512];
    int len = snprintf(cmd, sizeof(cmd), "cmd=1&uid=%s&topic=%s\r\n", 
                       client->config.uid, topic);
    if (len >= sizeof(cmd)) {
        printf("[BemfaTCP] 订阅命令过长\n");
        return -1;
    }
    
    printf("[BemfaTCP] 订阅主题: %s\n", topic);
    printf("[BemfaTCP] 发送命令: %s", cmd);
    
    // 发送订阅命令
    int sent = send(client->socket_fd, cmd, len, 0);
    if (sent != len) {
        printf("[BemfaTCP] 发送订阅命令失败: sent=%d, expected=%d\n", sent, len);
        return -1;
    }
    
    return 0;
}

int bemfa_tcp_publish(bemfa_tcp_handle_t handle, const char *topic, const char *msg) {
    bemfa_tcp_client_t *client = (bemfa_tcp_client_t *)handle;
    if (!client || !topic || !msg || client->state != BEMFA_TCP_STATE_CONNECTED) {
        return -1;
    }
    
    // 构建发布命令: cmd=2&uid=xxx&topic=xxx&msg=xxx\r\n
    char cmd[1024];
    int len = snprintf(cmd, sizeof(cmd), "cmd=2&uid=%s&topic=%s&msg=%s\r\n", 
                       client->config.uid, topic, msg);
    
    if (len >= sizeof(cmd)) {
        printf("[BemfaTCP] 发布消息过长\n");
        return -1;
    }
    
    printf("[BemfaTCP] 发布消息: topic=%s, msg_len=%zu\n", topic, strlen(msg));
    
    // 发送发布命令
    int sent = send(client->socket_fd, cmd, len, 0);
    if (sent != len) {
        printf("[BemfaTCP] 发送发布命令失败: sent=%d, expected=%d\n", sent, len);
        if (errno == ECONNRESET || errno == EPIPE) {
            client->state = BEMFA_TCP_STATE_DISCONNECTED;
            if (client->state_callback) {
                client->state_callback(client->state, client->state_user_data);
            }
        }
        return -1;
    }
    
    return 0;
}

int bemfa_tcp_ping(bemfa_tcp_handle_t handle) {
    bemfa_tcp_client_t *client = (bemfa_tcp_client_t *)handle;
    if (!client || client->state != BEMFA_TCP_STATE_CONNECTED) {
        return -1;
    }
    
    // 发送心跳: ping\r\n
    const char *ping_cmd = "ping\r\n";
    int sent = send(client->socket_fd, ping_cmd, strlen(ping_cmd), 0);
    if (sent == strlen(ping_cmd)) {
        client->last_ping_time = time(NULL);
        return 0;
    } else {
        printf("[BemfaTCP] 发送心跳失败: sent=%d\n", sent);
        return -1;
    }
}

// 解析TCP响应
static int parse_tcp_response(bemfa_tcp_client_t *client, const char *buf, int len) {
    if (len < 3) {
        return -1;
    }
    
    // 复制到临时缓冲区并添加结束符
    char temp[1024];
    if (len >= sizeof(temp)) {
        len = sizeof(temp) - 1;
    }
    memcpy(temp, buf, len);
    temp[len] = '\0';
    
    // 移除\r\n
    while (len > 0 && (temp[len-1] == '\n' || temp[len-1] == '\r')) {
        temp[--len] = '\0';
    }
    
    printf("[BemfaTCP] 收到响应: %s\n", temp);
    
    // 解析响应
    if (strncmp(temp, "cmd=0&res=1", 11) == 0) {
        // 心跳响应
        printf("[BemfaTCP] 收到心跳响应\n");
        return 0;
    } else if (strncmp(temp, "cmd=1&res=1", 11) == 0) {
        // 订阅成功响应
        printf("[BemfaTCP] 订阅成功\n");
        return 0;
    } else if (strncmp(temp, "cmd=2&res=1", 11) == 0) {
        // 发布成功响应
        printf("[BemfaTCP] 发布成功\n");
        return 0;
    } else if (strncmp(temp, "cmd=", 4) == 0) {
        // 消息格式: cmd=1&uid=xxx&topic=xxx&msg=xxx 或 cmd=9&uid=xxx&topic=xxx&msg=xxx
        // 解析消息
        char *topic = NULL;
        char *msg = NULL;
        char *p = temp;
        
        // 查找topic=
        char *topic_start = strstr(p, "&topic=");
        if (topic_start) {
            topic_start += 7;  // 跳过"&topic="
            char *topic_end = strchr(topic_start, '&');
            if (topic_end) {
                int topic_len = topic_end - topic_start;
                topic = (char *)malloc(topic_len + 1);
                memcpy(topic, topic_start, topic_len);
                topic[topic_len] = '\0';
                p = topic_end + 1;
            } else {
                // topic可能是最后一个字段
                topic = strdup(topic_start);
            }
        }
        
        // 查找msg=
        char *msg_start = strstr(temp, "&msg=");
        if (msg_start) {
            msg_start += 5;  // 跳过"&msg="
            msg = strdup(msg_start);
        }
        
        if (topic && msg && client->msg_callback) {
            client->msg_callback(topic, msg, strlen(msg), client->msg_user_data);
        }
        
        if (topic) free(topic);
        if (msg) free(msg);
        
        return 0;
    }
    
    return -1;
}

int bemfa_tcp_loop(bemfa_tcp_handle_t handle) {
    bemfa_tcp_client_t *client = (bemfa_tcp_client_t *)handle;
    if (!client || client->state != BEMFA_TCP_STATE_CONNECTED) {
        return -1;
    }
    
    // 检查心跳（建议60秒发送一次）
    time_t now = time(NULL);
    if (now - client->last_ping_time >= 60) {
        bemfa_tcp_ping(handle);
    }
    
    // 接收数据
    char buf[2048];
    int received = recv(client->socket_fd, buf, sizeof(buf) - 1, MSG_DONTWAIT);
    
    if (received > 0) {
        buf[received] = '\0';
        // 可能有多个响应，按\r\n分割
        char *line = buf;
        char *next;
        while ((next = strstr(line, "\r\n")) != NULL) {
            *next = '\0';
            parse_tcp_response(client, line, next - line);
            line = next + 2;
            if (line >= buf + received) break;
        }
        // 处理最后一行（如果没有\r\n结尾）
        if (line < buf + received && strlen(line) > 0) {
            parse_tcp_response(client, line, strlen(line));
        }
        return 0;
    } else if (received < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            // 没有数据，正常
            return 0;
        } else if (errno == ECONNRESET || errno == EPIPE || errno == ENOTCONN) {
            printf("[BemfaTCP] 连接已断开: %s\n", strerror(errno));
            client->state = BEMFA_TCP_STATE_DISCONNECTED;
            if (client->state_callback) {
                client->state_callback(client->state, client->state_user_data);
            }
            return -1;
        } else {
            printf("[BemfaTCP] 接收数据错误: %s\n", strerror(errno));
            return -1;
        }
    } else {
        // received == 0，连接关闭
        printf("[BemfaTCP] 连接已关闭\n");
        client->state = BEMFA_TCP_STATE_DISCONNECTED;
        if (client->state_callback) {
            client->state_callback(client->state, client->state_user_data);
        }
        return -1;
    }
}

bemfa_tcp_state_t bemfa_tcp_get_state(bemfa_tcp_handle_t handle) {
    bemfa_tcp_client_t *client = (bemfa_tcp_client_t *)handle;
    if (!client) {
        return BEMFA_TCP_STATE_DISCONNECTED;
    }
    return client->state;
}

void bemfa_tcp_set_message_callback(bemfa_tcp_handle_t handle,
                                    bemfa_tcp_message_callback_t callback,
                                    void *user_data) {
    bemfa_tcp_client_t *client = (bemfa_tcp_client_t *)handle;
    if (client) {
        client->msg_callback = callback;
        client->msg_user_data = user_data;
    }
}

void bemfa_tcp_set_state_callback(bemfa_tcp_handle_t handle,
                                  bemfa_tcp_state_callback_t callback,
                                  void *user_data) {
    bemfa_tcp_client_t *client = (bemfa_tcp_client_t *)handle;
    if (client) {
        client->state_callback = callback;
        client->state_user_data = user_data;
    }
}

void bemfa_tcp_cleanup(bemfa_tcp_handle_t handle) {
    bemfa_tcp_client_t *client = (bemfa_tcp_client_t *)handle;
    if (!client) {
        return;
    }
    
    bemfa_tcp_disconnect(handle);
    free(client);
}
