# 实时多人在线协作绘图系统

## 模块概述

本模块实现了一个允许多个用户通过网络实时协作绘图的系统。使用巴法云TCP协议进行设备间通信，使用多线程处理网络数据收发，确保绘图操作的流畅性。

## 架构设计

### 核心组件

1. **绘图协议模块** (`draw_protocol.h/c`)
   - 定义客户端与服务器之间的通信协议
   - 支持线条绘制、点绘制、清屏、橡皮擦等操作
   - 数据编码/解码（二进制格式）

2. **巴法云TCP客户端模块** (`bemfa_tcp_client.h/c`)
   - 实现与巴法云IoT平台的TCP连接
   - 支持订阅/发布模式
   - 消息编码/解码（十六进制字符串格式）
   - 心跳机制（60秒一次）

3. **协作绘图主模块** (`collaborative_draw.h/c`)
   - 管理网络连接和线程
   - 接收远程绘图操作并绘制到本地画布
   - 发送本地绘图操作到服务器
   - 二进制数据与十六进制字符串的转换

### 多线程架构

- **主线程**：LVGL UI线程，处理用户界面
- **网络接收线程**：接收服务器推送的其他用户绘图操作
- **网络发送线程**：处理心跳和状态维护
- **绘图线程**：触摸绘图线程（已存在）

### 数据流

```
本地用户绘图操作
    ↓
触摸绘图线程捕获
    ↓
协作绘图模块编码（二进制）
    ↓
转换为十六进制字符串
    ↓
网络发送线程 → 巴法云TCP服务器 → 其他客户端
                                    ↓
                            网络接收线程
                                    ↓
                            接收十六进制字符串
                                    ↓
                            转换为二进制数据
                                    ↓
                            解码绘图操作
                                    ↓
                            绘制到本地画布
```

## 巴法云TCP协议集成

### 连接参数

- **设备名称**: `your_device_name`（作为主题名称）
- **个人私钥**: `your_password`（作为UID）
- **服务器地址**: `bemfa.com`
- **端口**: `8344`
- **协议**: `TCP`（巴法云自定义TCP协议）

### 协议格式

巴法云TCP协议使用键值对格式，字段间用 `&` 分隔，每条指令以 `\r\n` 结尾。

#### 订阅主题

```
cmd=1&uid=xxxxxxxx&topic=xxx\r\n
```

响应：
```
cmd=1&res=1\r\n
```

#### 发布消息

推送模式（向所有订阅者推送，推送者自己不接收）：
```
cmd=2&uid=xxxxxxxx&topic=xxx/set&msg=xxx\r\n
```

响应：
```
cmd=2&res=1\r\n
```

#### 心跳

```
ping\r\n
```

响应：
```
cmd=0&res=1\r\n
```

### 消息编码

- **发送**：将绘图操作的二进制数据编码为十六进制字符串发送
- **接收**：将接收到的十六进制字符串解码为二进制数据

### 心跳机制

- 建议每60秒发送一次心跳
- 超过65秒未发送心跳会断线
- 心跳消息：`ping\r\n` 或任意以 `\r\n` 结尾的数据

## 使用说明

### 配置

1. **创建配置文件**：
   ```bash
   cp collaborative_draw_config.h.example collaborative_draw_config.h
   ```

2. **编辑配置文件**：
   打开 `collaborative_draw_config.h`，替换占位符：
   ```c
   #define COLLAB_DEVICE_NAME "your_device_name"    // 替换为您的设备名称
   #define COLLAB_PRIVATE_KEY "your_private_key"    // 替换为您的巴法云私钥
   ```

3. **重要提示**：
   - 配置文件 `collaborative_draw_config.h` 已添加到 `.gitignore`，不会被提交到仓库
   - 不要将包含真实密钥的配置文件提交到公开仓库
   - 如果配置文件不存在，代码将使用默认占位符（不会正常工作）

### 初始化

配置完成后，代码会自动从配置文件中读取设备名称和私钥。初始化过程在 `touch_draw.c` 中自动完成，无需手动调用。

### 发送绘图操作

```c
collaborative_draw_send_operation(x, y, prev_x, prev_y, pen_size, color, is_eraser);
```

### 发送清屏操作

```c
collaborative_draw_send_clear();
```

### 接收远程绘图

通过回调函数接收：

```c
void remote_draw_callback(uint16_t x, uint16_t y, uint16_t prev_x, uint16_t prev_y,
                         uint8_t pen_size, uint32_t color, bool is_eraser, void *user_data) {
    // 绘制远程用户的绘图操作
    draw_line(...);
}

collaborative_draw_set_remote_draw_callback(remote_draw_callback, NULL);
```

### 连接状态管理

```c
collaborative_draw_state_t state = collaborative_draw_get_state();
// 状态：DISCONNECTED, CONNECTING, CONNECTED, ERROR

// 停止连接
collaborative_draw_stop();

// 清理资源
collaborative_draw_cleanup();
```

## 配置说明

### 协作绘图配置结构

```c
typedef struct {
    bool enabled;                   // 是否启用协作模式
    char server_host[256];          // 服务器地址（bemfa.com）
    uint16_t server_port;           // 服务器端口（8344）
    uint32_t user_id;               // 本地用户ID
    char room_id[64];               // 房间ID（保留，未使用）
    char device_name[128];          // 巴法云设备名称（主题名称）
    char private_key[128];          // 巴法云个人私钥（UID）
} collaborative_draw_config_t;
```

## 协议文档参考

详细的TCP协议文档请参考：https://cloud.bemfa.com/docs/src/tcp_protocol.html

## 数据优化策略

1. **二进制编码**：绘图操作使用紧凑的二进制格式
2. **十六进制传输**：在TCP协议层使用十六进制字符串传输，避免二进制数据的特殊字符问题
3. **增量更新**：只发送变化的坐标点
4. **推送模式**：使用 `/set` 后缀推送消息，避免发送者接收自己的消息

## 待实现功能

- [ ] 房间管理和用户列表
- [ ] 绘图历史同步
- [ ] 断线重连机制（自动重连）
- [ ] 网络状态监控
- [ ] 数据压缩（可选）