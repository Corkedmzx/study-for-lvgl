# Ubuntu虚拟机编译和测试指南

本指南说明如何在Ubuntu虚拟机上编译和测试GEC6818设备的LVGL应用程序，特别是协作绘图功能。

## 目录

1. [环境准备](#环境准备)
2. [编译项目](#编译项目)
3. [运行测试](#运行测试)
4. [协作绘图测试](#协作绘图测试)
5. [常见问题](#常见问题)

## 环境准备

### 1. 安装必要的依赖

在Ubuntu虚拟机上安装以下依赖：

```bash
# 更新软件包列表
sudo apt update

# 安装编译工具链
sudo apt install -y build-essential gcc make

# 安装SDL2开发库（用于显示和输入）
sudo apt install -y libsdl2-dev

# 安装Git（如果还没有）
sudo apt install -y git
```

### 2. 克隆项目并初始化子模块

```bash
# 克隆项目（如果还没有）
git clone <repository-url>
cd lv_port_linux-release-v8.2

# 初始化子模块（重要！）
git submodule update --init --recursive
```

### 3. 配置SDL驱动

编辑 `lv_drv_conf.h`，启用SDL驱动：

```c
/* SDL based drivers for display, mouse, mousewheel and keyboard*/
#ifndef USE_SDL
# define USE_SDL 1  // 改为1以启用SDL
#endif

#if USE_SDL || USE_SDL_GPU
#  define SDL_HOR_RES     800  // 改为800以匹配GEC6818分辨率
#  define SDL_VER_RES     480  // 改为480以匹配GEC6818分辨率
#  define SDL_ZOOM        1
#  define SDL_DOUBLE_BUFFERED 0
#  define SDL_INCLUDE_PATH    <SDL2/SDL.h>
#  define SDL_DUAL_DISPLAY    0
#endif
```

## 编译项目

### 使用Ubuntu版本的Makefile

```bash
# 清理之前的编译文件
make -f Makefile.ubuntu clean

# 编译项目
make -f Makefile.ubuntu

# 如果编译成功，会生成 demo_ubuntu 可执行文件
```

### 编译选项说明

- **编译器**: `gcc` (x86_64)
- **显示驱动**: SDL2 (窗口显示)
- **输入设备**: SDL鼠标（模拟触摸）
- **链接库**: `-lSDL2 -lpthread -lm`

## 运行测试

### 基本运行

```bash
# 运行程序
./demo_ubuntu
```

程序会打开一个SDL窗口（800x480），显示LVGL界面。你可以使用鼠标点击和拖拽来模拟触摸操作。

### 注意事项

1. **触摸绘图功能限制**: 
   - 在Ubuntu虚拟机上，触摸绘图功能会尝试访问 `/dev/fb0` 和 `/dev/input/event0`
   - 如果这些设备不存在，触摸绘图功能可能无法正常工作
   - 但UI界面和协作绘图功能（网络部分）可以正常测试

2. **鼠标操作**:
   - 鼠标左键点击 = 触摸按下
   - 鼠标移动 = 触摸移动
   - 鼠标左键释放 = 触摸释放

3. **窗口关闭**:
   - 点击窗口的关闭按钮或按 `Ctrl+C` 退出程序

## 协作绘图测试

### 测试场景

你可以使用以下两种方式测试协作绘图功能：

#### 方式1：虚拟机 + GEC6818设备

1. **在Ubuntu虚拟机上运行**:
   ```bash
   ./demo_ubuntu
   ```

2. **在GEC6818设备上运行**:
   ```bash
   ./demo
   ```

3. **测试步骤**:
   - 在虚拟机中点击"连接协作"按钮（主机模式）
   - 在GEC6818设备上点击"加入协作"按钮（客机模式）
   - 在任一设备上绘制，观察另一设备是否同步显示

#### 方式2：两个虚拟机实例

1. **打开两个终端窗口**

2. **在第一个终端运行**:
   ```bash
   ./demo_ubuntu
   ```

3. **在第二个终端运行**:
   ```bash
   ./demo_ubuntu
   ```

4. **测试步骤**:
   - 在第一个虚拟机中点击"连接协作"按钮
   - 在第二个虚拟机中点击"加入协作"按钮
   - 在任一虚拟机中绘制，观察另一虚拟机是否同步显示

### 配置巴法云

确保两个设备使用相同的巴法云配置：

1. **设备名称**: 在 `src/touch_draw/touch_draw.c` 中配置
   ```c
   .device_name = "GEC6818",  // 两个设备使用相同的设备名称
   ```

2. **私钥**: 在 `src/touch_draw/touch_draw.c` 中配置
   ```c
   .private_key = "your_password",  // 两个设备使用相同的私钥
   ```

### 测试检查清单

- [ ] 虚拟机程序可以正常启动并显示UI
- [ ] 可以点击按钮和导航界面
- [ ] 可以进入触摸绘图界面
- [ ] 点击"连接协作"按钮后，显示"连接中..."
- [ ] 连接成功后，显示"已连接"
- [ ] 两个设备可以成功建立连接
- [ ] 在一个设备上绘制，另一个设备可以同步显示（如果触摸绘图功能可用）

## 常见问题

### 1. 编译错误：找不到SDL2

**错误信息**:
```
fatal error: SDL2/SDL.h: No such file or directory
```

**解决方法**:
```bash
sudo apt install -y libsdl2-dev
```

### 2. 链接错误：找不到SDL库

**错误信息**:
```
/usr/bin/ld: cannot find -lSDL2
```

**解决方法**:
```bash
sudo apt install -y libsdl2-dev
```

### 3. 运行时错误：无法打开显示

**错误信息**:
```
SDL_Init failed: No available video device
```

**解决方法**:
- 确保你在图形界面环境中运行（不是SSH无头模式）
- 如果使用SSH，需要启用X11转发：
  ```bash
  ssh -X user@host
  ```

### 4. 触摸绘图功能无法使用

**原因**: Ubuntu虚拟机通常没有 `/dev/fb0` 和 `/dev/input/event0` 设备。

**解决方法**:
- 这是正常的，触摸绘图功能需要直接访问framebuffer设备
- 在虚拟机上主要测试UI界面和协作绘图的网络功能
- 完整的触摸绘图功能需要在GEC6818设备上测试

### 5. 协作绘图连接失败

**可能原因**:
1. 网络连接问题
2. 巴法云配置错误（设备名称或私钥不匹配）
3. 防火墙阻止了TCP连接（端口8344）

**解决方法**:
- 检查网络连接：`ping bemfa.com`
- 检查巴法云配置是否正确
- 检查防火墙设置：
  ```bash
  sudo ufw allow 8344/tcp
  ```

### 6. 程序运行后窗口无响应

**可能原因**: SDL事件循环问题

**解决方法**:
- 确保程序在图形界面环境中运行
- 检查是否有其他程序占用了SDL资源
- 尝试重启程序

## 调试技巧

### 1. 查看编译输出

```bash
make -f Makefile.ubuntu 2>&1 | tee build.log
```

### 2. 使用GDB调试

```bash
# 编译时添加调试信息（修改Makefile.ubuntu中的CFLAGS，将-O2改为-O0 -g）
make -f Makefile.ubuntu clean
make -f Makefile.ubuntu

# 使用GDB运行
gdb ./demo_ubuntu
```

### 3. 查看网络连接

```bash
# 查看TCP连接
netstat -an | grep 8344

# 或使用ss
ss -an | grep 8344
```

### 4. 查看程序日志

程序会输出详细的日志信息，包括：
- 网络连接状态
- 协作绘图状态
- 错误信息

## 性能优化

### 1. 减少编译时间

```bash
# 使用多核编译
make -f Makefile.ubuntu -j$(nproc)
```

### 2. 优化运行性能

- 关闭不必要的后台程序
- 确保虚拟机有足够的内存和CPU资源
- 使用硬件加速（如果支持）

## 下一步

完成虚拟机测试后，你可以：

1. **在GEC6818设备上测试**: 将ARM版本的程序传输到设备上运行
2. **优化代码**: 根据测试结果优化代码
3. **添加功能**: 在虚拟机环境中开发和测试新功能

## 参考资源

- [LVGL官方文档](https://docs.lvgl.io/)
- [SDL2文档](https://wiki.libsdl.org/)
- [巴法云TCP协议文档](https://cloud.bemfa.com/docs/src/tcp_protocol.html)
