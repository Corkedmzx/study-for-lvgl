# Time Sync 模块文档

## 模块概述

`time_sync` 模块负责通过网络同步系统时间。该模块通过HTTP请求获取网络时间，解析HTTP响应头中的Date字段，然后设置系统时间。

## 文件结构

- `time_sync.h` - 模块接口定义
- `time_sync.c` - 模块实现

## 主要功能

### 同步系统时间

#### `sync_system_time()`

通过网络同步系统时间。

**函数签名：**
```c
int sync_system_time(void);
```

**功能：**
- 连接到时间服务器（HTTP服务器）
- 发送HTTP HEAD请求
- 解析响应头中的Date字段
- 设置系统时间（UTC+8，中国时区）
- 将时间写入硬件时钟（RTC）

**返回值：**
- 成功返回0
- 失败返回-1

**调用位置：**
- `main.c:34` - 程序启动时同步系统时间

**实现细节：**

1. **时间服务器列表**：
   ```c
   static const char *time_servers[] = {
       "www.baidu.com",         // 百度（主服务器，国内访问快）
       "www.qq.com",            // 腾讯（备用）
       "www.sina.com.cn",       // 新浪（备用）
       "www.google.com",        // Google（备用，国外）
       NULL
   };
   ```

2. **网络连接**：
   - 使用 `socket()` 创建TCP socket
   - 使用 `getaddrinfo()` 解析域名
   - 使用 `connect()` 连接到服务器（端口80）
   - 设置超时（10秒）

3. **HTTP请求**：
   ```http
   HEAD / HTTP/1.1
   Host: www.baidu.com
   User-Agent: curl
   Connection: close
   ```
   - 使用HEAD请求（只需要响应头，不需要响应体）
   - 减少数据传输量

4. **时间解析**：
   - 从HTTP响应头中提取 `Date:` 字段
   - 解析RFC 822格式：`Mon, 01 Jan 2025 12:00:00 GMT`
   - 或带时区偏移格式：`Mon, 01 Jan 2025 12:00:00 +0800`
   - 转换为UTC时间戳

5. **时区处理**：
   - HTTP Date字段通常是UTC时间
   - 加上8小时得到中国时区时间
   - 使用 `date` 命令设置系统时间

6. **硬件时钟同步**：
   - 使用 `hwclock -w` 将时间写入硬件时钟（RTC）
   - 防止重启后时间丢失

7. **备用方案**：
   - 如果 `date` 命令失败，使用 `settimeofday()` 系统调用
   - 需要root权限

### 辅助函数

#### `parse_utc_time()`

将UTC时间转换为time_t（避免使用timegm的兼容性问题）。

**函数签名：**
```c
static time_t parse_utc_time(struct tm *tm);
```

**功能：**
- 临时设置时区为UTC
- 使用 `mktime()` 转换为time_t
- 恢复原始时区

#### `parse_date_header()`

从HTTP响应头中解析Date字段。

**函数签名：**
```c
static time_t parse_date_header(const char *header);
```

**功能：**
- 查找 `Date:` 或 `date:` 字段
- 解析RFC 822格式的时间字符串
- 支持GMT和时区偏移格式
- 返回UTC时间戳

#### `parse_time_from_response()`

从HTTP响应中提取时间戳（支持多种API格式，当前未使用）。

**函数签名：**
```c
static time_t parse_time_from_response(const char *response);
```

**功能：**
- 支持多种时间API格式
- 提取JSON格式的时间字段
- 支持Unix时间戳和ISO 8601格式

## 模块调用关系

### 被调用情况

1. **main.c**
   ```c
   // 设置系统时区
   setenv("TZ", "Asia/Shanghai", 1);
   tzset();
   
   // 同步系统时间
   if (sync_system_time() == 0) {
       printf("系统时间同步成功\n");
   } else {
       printf("系统时间同步失败，继续运行\n");
   }
   ```

### 依赖关系

- **标准C库**：`stdio.h`, `stdlib.h`, `string.h`
- **网络库**：`sys/socket.h`, `netinet/in.h`, `arpa/inet.h`, `netdb.h`
- **系统调用**：`unistd.h`, `sys/time.h`, `time.h`
- **不依赖其他项目模块**

## 使用示例

### 示例1：同步系统时间

```c
#include "src/time_sync/time_sync.h"

// 设置时区
setenv("TZ", "Asia/Shanghai", 1);
tzset();

// 同步时间
if (sync_system_time() == 0) {
    printf("时间同步成功\n");
} else {
    printf("时间同步失败\n");
}
```

### 示例2：检查时间同步结果

```c
#include "src/time_sync/time_sync.h"
#include <time.h>

sync_system_time();

// 验证设置后的时间
time_t now = time(NULL);
struct tm *tm_now = localtime(&now);
char time_str[64];
strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm_now);
printf("当前系统时间: %s\n", time_str);
```

## 实现细节

### 时间解析算法

1. **RFC 822格式解析**：
   ```
   Mon, 01 Jan 2025 12:00:00 GMT
   ```
   - 使用 `sscanf()` 解析各个字段
   - 月份名称转换为数字（Jan=0, Feb=1, ...）
   - 年份减去1900（tm结构要求）
   - 使用 `parse_utc_time()` 转换为UTC时间戳

2. **时区偏移处理**：
   ```
   Mon, 01 Jan 2025 12:00:00 +0800
   ```
   - 解析时区偏移（+0800或-0500）
   - 转换为秒数（小时*3600 + 分钟*60）
   - 从时间戳中减去偏移，得到UTC时间

### 系统时间设置

1. **使用date命令**（首选）：
   ```c
   char date_cmd[256];
   snprintf(date_cmd, sizeof(date_cmd), "date -s \"%s\"", china_str);
   system(date_cmd);
   ```
   - 格式：`date -s "YYYY-MM-DD HH:MM:SS"`
   - 需要root权限

2. **使用settimeofday系统调用**（备用）：
   ```c
   struct timeval tv;
   tv.tv_sec = remote_time;  // UTC时间戳
   tv.tv_usec = 0;
   settimeofday(&tv, NULL);
   ```
   - 需要root权限
   - 设置UTC时间，然后通过时区环境变量显示本地时间

### 硬件时钟同步

```c
system("hwclock -w");
```

- 将系统时间写入硬件时钟（RTC）
- 防止重启后时间丢失
- 如果系统没有 `hwclock` 命令，会失败但不影响程序运行

## 注意事项

1. **权限要求**：设置系统时间需要root权限，否则会失败
2. **网络依赖**：需要网络连接，离线时无法同步
3. **时区设置**：程序假设使用中国时区（UTC+8），硬编码加8小时
4. **服务器选择**：优先使用国内服务器（百度、腾讯、新浪），访问速度快
5. **超时设置**：网络超时设置为10秒，网络较慢时可能需要调整
6. **静态链接**：使用 `getaddrinfo()` 在静态链接时可能有警告，但运行时通常能正常工作
7. **时间精度**：HTTP Date字段精度为秒，不包含毫秒
8. **硬件时钟**：如果系统没有 `hwclock` 命令，硬件时钟同步会失败，但不影响系统时间设置

## 相关文件

- `main.c` - 程序启动时调用 `sync_system_time()`

## 扩展建议

1. **NTP协议支持**：使用标准的NTP协议同步时间，精度更高
2. **自动重试**：网络失败时自动重试多次
3. **时间校准**：计算网络延迟，校准时间
4. **定期同步**：定期自动同步时间，保持准确性

