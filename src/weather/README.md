# Weather 模块文档

## 模块概述

`weather` 模块负责从网络获取天气信息。该模块使用HTTP请求访问天气API，解析JSON响应，提取天气数据并返回格式化的字符串。

## 文件结构

- `weather.h` - 模块接口定义
- `weather.c` - 模块实现

## 主要功能

### 获取天气数据

#### `get_weather_data()`

从网络获取天气数据。

**函数签名：**
```c
char* get_weather_data(void);
```

**功能：**
- 连接到天气API服务器（wttr.in）
- 发送HTTP GET请求
- 接收并解析JSON响应
- 提取多天天气数据（最多6天）
- 返回格式化的天气信息字符串

**返回值：**
- 成功返回格式化的天气信息字符串（需要调用者释放）
- 失败返回错误信息字符串（需要调用者释放）
- 网络连接失败返回 "网络连接失败"

**调用位置：**
- `src/ui/weather_win.c:32` - 天气窗口获取天气数据

**实现细节：**

1. **网络连接**：
   - 使用 `socket()` 创建TCP socket
   - 使用 `getaddrinfo()` 解析域名（wttr.in）
   - 使用 `connect()` 连接到服务器（端口80）
   - 设置超时（10秒）

2. **HTTP请求**：
   ```http
   GET /Hezhou?format=j1&lang=zh HTTP/1.1
   Host: wttr.in
   User-Agent: curl
   Accept: */*
   Connection: close
   ```
   - 请求广西贺州市的天气（Hezhou）
   - 使用JSON格式（format=j1）
   - 使用中文（lang=zh）

3. **响应解析**：
   - 读取HTTP响应头，查找Content-Length
   - 读取完整响应体（最多64KB）
   - 查找JSON数据部分（跳过HTTP头）
   - 解析 `weather` 数组中的多天数据

4. **数据提取**：
   提取以下字段：
   - `date` - 日期
   - `avgtempC` - 平均温度（摄氏度）
   - `maxtempC` - 最高温度
   - `mintempC` - 最低温度
   - `windspeedKmph` - 风速（km/h）
   - `humidity` - 湿度（%）
   - `cloudcover` - 云量（%）
   - `condition` - 天气状况（优先使用中文 `lang_zh` 数组中的 `value`）

5. **格式化输出**：
   每行一个字段，天数之间用 `|` 分隔：
   ```
   日期
   最高温/最低温°C
   平均温度°C
   天气状况
   风速km/h
   湿度%
   云量%
   |  // 天数分隔符
   ```

**支持的天气API**：
- **wttr.in** - 免费天气API，无需API key
- 使用域名解析（静态链接可能有警告，但运行时通常能正常工作）

## 模块调用关系

### 被调用情况

1. **src/ui/weather_win.c**
   ```c
   char *data = get_weather_data();
   if (data) {
       // 使用天气数据
       // ...
       free(data);  // 释放内存
   }
   ```

### 依赖关系

- **标准C库**：`stdio.h`, `stdlib.h`, `string.h`
- **网络库**：`sys/socket.h`, `netinet/in.h`, `arpa/inet.h`, `netdb.h`
- **系统调用**：`unistd.h`, `sys/types.h`, `time.h`
- **不依赖其他项目模块**

## 使用示例

### 示例1：获取天气数据

```c
#include "src/weather/weather.h"

char *weather_data = get_weather_data();
if (weather_data) {
    printf("天气数据:\n%s\n", weather_data);
    free(weather_data);  // 必须释放内存
}
```

### 示例2：解析天气数据

```c
char *data = get_weather_data();
if (data && strcmp(data, "网络连接失败") != 0) {
    // 按行分割数据
    char *line = strtok(data, "\n");
    int day = 0;
    while (line && day < 6) {
        if (strcmp(line, "|") == 0) {
            day++;
        } else {
            printf("第%d天: %s\n", day + 1, line);
        }
        line = strtok(NULL, "\n");
    }
    free(data);
}
```

## 实现细节

### JSON解析

模块实现了简单的JSON字段提取函数 `extract_json_value()`：

1. **字符串值提取**：
   - 查找 `"key":` 模式
   - 提取引号内的值

2. **数字值提取**：
   - 查找 `"key":` 模式
   - 提取数字（支持整数、小数、科学计数法）

### 多天数据解析

1. **查找weather数组**：
   - 在JSON中查找 `"weather"` 字段
   - 定位到数组开始位置 `[`

2. **遍历数组元素**：
   - 解析每个对象（一天的数据）
   - 提取所需字段
   - 格式化并添加到结果字符串

3. **数组边界处理**：
   - 使用括号计数找到数组结束位置
   - 使用大括号计数找到对象结束位置
   - 跳过逗号和空格，找到下一个对象

### 错误处理

1. **网络错误**：
   - DNS解析失败：尝试下一个服务器
   - 连接失败：尝试下一个服务器
   - 所有服务器失败：返回 "网络连接失败"

2. **数据解析错误**：
   - 找不到weather字段：返回 "数据格式错误：未找到weather字段"
   - 无法解析数据：返回 "数据格式错误：无法解析天气数据"

3. **内存错误**：
   - 分配失败：返回NULL

## 注意事项

1. **内存管理**：返回的字符串需要调用者使用 `free()` 释放
2. **网络依赖**：需要网络连接，离线时返回错误信息
3. **API限制**：使用免费API（wttr.in），可能有请求频率限制
4. **城市名称**：当前硬编码为 "Hezhou"（贺州），如需修改需要修改源码
5. **时区**：返回的时间数据可能使用UTC时区，需要根据实际情况调整
6. **静态链接**：使用 `getaddrinfo()` 在静态链接时可能有警告，但运行时通常能正常工作
7. **超时设置**：网络超时设置为10秒，网络较慢时可能需要调整

## 相关文件

- `src/ui/weather_win.c` - 天气窗口，调用 `get_weather_data()` 并显示结果

## 扩展建议

1. **支持更多城市**：可以通过参数传入城市名称
2. **缓存机制**：缓存天气数据，避免频繁请求
3. **错误重试**：网络失败时自动重试
4. **更多字段**：提取更多天气信息（如降水概率、紫外线指数等）

