# 配置说明

本文档说明如何配置项目中的敏感信息。

## 概述

为了保护敏感信息（如API密钥、密码等），项目使用配置文件的方式管理这些信息。配置文件已添加到 `.gitignore`，不会被提交到代码仓库。

## 配置文件列表

### 1. 协作绘图配置

**模板文件**: `src/collaborative_draw/collaborative_draw_config.h.example`  
**配置文件**: `src/collaborative_draw/collaborative_draw_config.h`（需要创建）

**配置步骤**：

1. 复制模板文件：
   ```bash
   cp src/collaborative_draw/collaborative_draw_config.h.example \
      src/collaborative_draw/collaborative_draw_config.h
   ```

2. 编辑配置文件，替换占位符：
   ```c
   // 巴法云设备配置
   #define COLLAB_DEVICE_NAME "your_device_name"      // 替换为您的设备名称
   #define COLLAB_PRIVATE_KEY "your_private_key_here"  // 替换为您的巴法云私钥
   ```

3. 在 `src/touch_draw/touch_draw.c` 中，取消注释配置文件的包含行：
   ```c
   // 取消注释下面这行以使用配置文件（创建配置文件后）
   #include "../collaborative_draw/collaborative_draw_config.h"
   ```

3. **重要提示**：
   - 使用前请先注册巴法云账号并创建TCP设备
   - 设备名称：在巴法云控制台创建的TCP设备名称（作为主题使用）
   - 个人私钥：在巴法云控制台获取的私钥（作为UID使用）
   - 配置文件已添加到 `.gitignore`，不会被提交到仓库

### 2. 登录密码配置

**模板文件**: `src/ui/login_config.h.example`  
**配置文件**: `src/ui/login_config.h`（需要创建）

**配置步骤**：

1. 复制模板文件：
   ```bash
   cp src/ui/login_config.h.example src/ui/login_config.h
   ```

2. 编辑配置文件，替换占位符：
   ```c
   // 登录密码（最大32个字符）
   #define CORRECT_PASSWORD "your_password_here"  // 替换为您的登录密码
   #define MAX_PASSWORD_LEN 32
   ```

3. 在 `src/ui/login_win.c` 中，取消注释配置文件的包含行：
   ```c
   // 取消注释下面这行以使用配置文件（创建配置文件后）
   #include "login_config.h"
   ```

3. **重要提示**：
   - 密码最大长度为32个字符
   - 配置文件已添加到 `.gitignore`，不会被提交到仓库

## 默认行为

如果配置文件不存在，代码将使用默认占位符值：
- 协作绘图：`COLLAB_DEVICE_NAME = "your_device_name"`，`COLLAB_PRIVATE_KEY = "your_private_key_here"`
- 登录密码：`CORRECT_PASSWORD = "your_password_here"`

**注意**：使用默认占位符时，功能将无法正常工作。必须创建并配置正确的配置文件。

## 安全检查

- ✅ 所有配置文件已添加到 `.gitignore`
- ✅ 模板文件（`.example`）不包含真实敏感信息
- ✅ 代码使用条件编译，如果配置文件不存在则使用默认占位符
- ✅ 文档中不包含真实敏感信息

## 故障排除

### 问题：编译错误，提示找不到配置文件

**解决方案**：这是正常的。如果配置文件不存在，代码会使用默认占位符。创建配置文件后重新编译即可。

### 问题：功能无法正常工作

**可能原因**：
1. 配置文件未创建
2. 配置文件中的占位符未替换为实际值
3. 配置值不正确（如设备名称或私钥错误）

**解决方案**：
1. 检查配置文件是否存在
2. 确认配置文件中的占位符已替换为实际值
3. 验证配置值是否正确（如巴法云设备名称和私钥）

## 相关文档

- 协作绘图详细说明：`src/collaborative_draw/README.md`
- 主项目README：`README.md`
