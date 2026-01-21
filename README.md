# LVGL for Frame Buffer Device

基于 LVGL 的嵌入式 Linux GUI 应用程序，配置为在 Linux 系统上使用 `/dev/fb0` 帧缓冲设备运行。

## 项目简介

本项目是基于 [lv_port_linux](https://github.com/lvgl/lv_port_linux) 开发的嵌入式图形界面应用，针对 **粤嵌 GEC6818** 开发板进行了优化和功能扩展。项目使用 LVGL v8.2 图形库，通过 Linux framebuffer 驱动实现图形显示，支持触摸屏交互。

**注意**：本项目包含协作绘图功能，使用巴法云TCP协议。如需使用该功能，请在代码中配置您的巴法云设备名称和私钥（详情参见 `src/collaborative_draw/README.md`）。

## 系统要求

- **目标平台**: ARM Linux (ARMv7)
- **开发板**: 粤嵌 GEC6818
- **编译器**: `arm-linux-gnueabihf-gcc`
- **显示设备**: `/dev/fb0` (framebuffer)
- **输入设备**: 触摸屏
- **依赖**: MPlayer (用于音视频播放)

## 快速开始

### 1. 克隆仓库

```bash
git clone <repository-url>
cd lv_port_linux-release-v8.2
```

### 2. 初始化子模块

**重要**: 克隆仓库后，必须下载子模块，否则将缺少关键组件。

```bash
git submodule update --init --recursive
```

### 3. 编译项目

在项目根目录执行：

```bash
make clean && make
```

或者使用提供的重建脚本：

```bash
chmod +x rebuild.sh
./rebuild.sh
```

编译完成后，可执行文件 `demo` 将生成在项目根目录。

### 4. 运行

将编译好的 `demo` 文件传输到目标设备（GEC6818），然后运行：

```bash
./demo
```

## 项目结构

```
.
├── src/                    # 源代码目录
│   ├── common/            # 公共模块（通用定义、触摸设备）
│   ├── hal/               # 硬件抽象层
│   ├── file_scanner/      # 文件扫描模块
│   ├── image_viewer/      # 图片查看器
│   ├── media_player/      # 媒体播放器（音频/视频）
│   ├── weather/           # 天气获取模块
│   ├── time_sync/         # 时间同步模块
│   ├── game_2048/         # 2048 游戏逻辑
│   ├── touch_draw/        # 触摸绘图模块
│   ├── collaborative_draw/# 协作绘图模块（巴法云TCP协议）
│   └── ui/                # UI 界面模块
│       ├── ui_screens.c   # 主界面创建
│       ├── album_win.c    # 相册窗口
│       ├── music_win.c    # 音乐播放窗口
│       ├── video_win.c    # 视频播放窗口
│       ├── led_win.c      # LED/蜂鸣器控制窗口
│       ├── weather_win.c  # 天气窗口
│       ├── login_win.c    # 密码锁窗口
│       ├── screensaver_win.c  # 屏保窗口
│       ├── timer_win.c    # 定时器窗口
│       ├── clock_win.c    # 时钟窗口
│       └── game_2048_win.c   # 2048 游戏窗口
├── bin/                   # 资源文件（字体、测试媒体文件）
├── lvgl/                  # LVGL 图形库（子模块）
├── lv_drivers/            # LVGL 驱动（子模块）
├── main.c                 # 程序入口
├── Makefile               # 构建配置
└── README.md              # 本文件
```

## 主要功能

### 1. 相册 (Album)
- 浏览 `/mdata` 目录下的图片文件
- 支持 BMP、GIF 等格式
- 图片缩放和切换功能

### 2. 音频播放器 (Music Player)
- 播放 `/mdata` 目录下的音频文件
- 支持 MP3 等格式
- 基于 MPlayer 实现
- 播放列表管理

### 3. 视频播放器 (Video Player)
- 播放 `/mdata` 目录下的视频文件
- 支持 AVI、MKV、MP4、MOV、FLV、WMV 等格式
- 基于 MPlayer + framebuffer 实现
- **触屏手势控制**：
  - **左上角点击**：返回主页
  - **左下角点击**：上一首视频
  - **右下角点击**：下一首视频
  - **右上角点击**：暂停/恢复播放
  - **上划**：增加音量
  - **下划**：减少音量
  - **右划**：加速播放
  - **左划**：减速播放
- **性能优化**：
  - 使用硬件加速解码参数（跳过非参考帧、循环滤波等）
  - 8线程解码加速
  - 视频缩放和格式转换优化
  - 丢帧机制减少卡顿
- **智能状态管理**：
  - 自动检测视频播放结束
  - 视频播放结束后仍可通过触控切换视频或返回主页
  - 进程状态自动同步

### 4. LED/蜂鸣器控制 (LED/Buzzer Control)
- 控制开发板上的 LED 灯
- 控制蜂鸣器

### 5. 天气获取 (Weather)
- 获取天气信息
- **注意**: 天气地址为固定配置，如需修改请从源码中修改

### 6. 密码锁 (Password Lock)
- 系统启动时显示密码锁界面
- **默认密码**: `114514`
- 解锁后进入主界面

### 7. 屏保 (Screensaver)
- 系统启动时先显示屏保
- 触摸解锁后进入密码锁界面

### 8. 时钟 (Clock)
- 显示当前时间
- 自动同步系统时间（时区：Asia/Shanghai UTC+8）

### 9. 定时器 (Timer)
- 定时器功能

### 10. 2048 游戏 (Game 2048)
- 经典 2048 数字游戏
- 触摸屏操作

### 11. 触摸绘图 (Touch Draw)
- 直接在屏幕上进行绘图
- 支持多种笔触大小
- 支持多种颜色选择
- 橡皮擦功能
- 清屏功能
- 实时绘制到 framebuffer

### 12. 协作绘图 (Collaborative Draw)
- 基于巴法云TCP协议的多设备实时协作绘图
- 支持多人在线同时绘图
- 实时同步绘图操作到所有连接的设备
- **功能特点**：
  - 主机模式：创建协作房间
  - 客机模式：加入他人的协作房间
  - 实时同步：绘图操作实时同步到所有参与者
  - 自动断线检测和重连
- **配置说明**：
  - 需要在 `src/touch_draw/touch_draw.c` 中配置巴法云设备名称和私钥
  - 设备名称：`your_device_name`（作为TCP协议的主题）
  - 私钥：`your_password`（作为TCP协议的UID）
  - 详细配置和使用说明请参考 `src/collaborative_draw/README.md`

## 配置说明

### 媒体文件目录

默认媒体文件目录配置在 `src/common/common.h` 中：

```c
#define IMAGE_DIR "/mdata"    // 图片目录路径
#define MEDIA_DIR "/mdata"    // 音视频目录路径
```

如需修改，请编辑该文件后重新编译。

### 协作绘图配置

协作绘图功能需要在 `src/touch_draw/touch_draw.c` 中配置：

```c
collaborative_draw_config_t collab_config = {
    .enabled = true,
    .server_host = "bemfa.com",
    .server_port = 8344,
    .device_name = "your_device_name",    // 替换为您的设备名称
    .private_key = "your_password"        // 替换为您的巴法云私钥
};
```

**重要提示**：
- 使用前请先注册巴法云账号并创建TCP设备
- 将代码中的占位符替换为实际的设备名称和私钥
- 不要将包含真实密钥的代码提交到公开仓库

详细配置说明请参考 `src/collaborative_draw/README.md`。

### 编译选项

编译配置在 `Makefile` 中：

- **编译器**: `arm-linux-gnueabihf-gcc`
- **链接方式**: 静态链接 (`-static`) 以避免 GLIBC 版本不匹配问题
- **优化级别**: `-O3 -g0`
- **OpenSSL**: 已禁用（项目不支持OpenSSL）

### 字体

项目包含以下字体：

- **SourceHanSansSC_VF**: 思源黑体（中文字体）
- **FA-solid-900**: FontAwesome 图标字体

字体文件位于 `bin/` 目录。

## 技术细节

### 核心组件

- **LVGL v8.2**: 轻量级图形库
- **Linux Framebuffer**: 显示驱动
- **MPlayer**: 音视频播放后端
- **触摸屏驱动**: 支持触摸输入
- **巴法云TCP协议**: 协作绘图通信协议

### 架构特点

- 模块化设计，各功能独立实现
- 使用硬件抽象层（HAL）隔离硬件细节
- 主线程处理 UI 渲染，后台线程处理媒体播放和触屏控制
- 内存映射优化，提升显示刷新速度
- 多线程触屏控制，响应速度快
- 智能进程状态检测，避免资源泄漏
- 网络异步通信，支持多设备实时协作

### 网络通信

- **协议**: 巴法云TCP协议
- **服务器**: bemfa.com:8344
- **通信模式**: 发布/订阅（Pub/Sub）
- **消息格式**: 键值对格式，字段用 `&` 分隔，以 `\r\n` 结尾
- **数据编码**: 二进制数据编码为十六进制字符串传输
- **心跳机制**: 60秒发送一次心跳，超过65秒未发送会断线

## 参考资源

- LVGL 官方博客教程: [Linux Framebuffer](https://blog.lvgl.io/2018-01-03/linux_fb)
- LVGL 官方文档: [https://docs.lvgl.io/](https://docs.lvgl.io/)
- 巴法云TCP协议文档: [https://cloud.bemfa.com/docs/src/tcp_protocol.html](https://cloud.bemfa.com/docs/src/tcp_protocol.html)

## 已知问题

1. **天气地址固定**: 天气获取功能使用固定地址，修改需从源码修改

## 使用说明

### 视频播放器操作

1. **进入视频播放**：从主界面点击"视频"按钮
2. **触屏控制**：
   - 视频播放时，可通过触屏手势控制播放
   - 视频播放结束后，触屏控制仍保持激活，可切换视频或返回主页
3. **返回主页**：点击屏幕左上角区域即可返回主页

### 协作绘图操作

1. **进入绘图界面**：从主界面点击"绘图"按钮
2. **连接协作**：
   - 点击"连接协作"按钮创建协作房间（主机模式）
   - 点击"加入协作"按钮加入他人的协作房间（客机模式）
   - 连接成功后，所有参与者的绘图操作将实时同步
3. **绘图功能**：
   - 选择笔触大小和颜色
   - 直接在屏幕上绘制
   - 使用橡皮擦功能擦除
   - 使用清屏功能清除所有内容
4. **结束协作**：点击"结束协作"按钮断开连接

详细使用说明请参考 `src/touch_draw/README.md` 和 `src/collaborative_draw/README.md`。

## 注意事项

### 许可证

- 请根据 LVGL 官方的许可声明使用该仓库代码
- 如有侵权请及时联系删除
- **此仓库代码仅供学习，不做任何商业用途**

### 开发环境

- 本项目针对 ARM Linux 平台编译
- 需要在交叉编译环境中构建
- 确保目标设备已安装必要的系统库和 MPlayer

### 安全提示

- **不要将包含真实密钥的代码提交到公开仓库**
- 协作绘图功能需要配置巴法云设备信息，请妥善保管您的私钥
- 建议使用环境变量或配置文件（不纳入版本控制）来管理敏感信息

## 贡献

欢迎提交 Issue 和 Pull Request 来改进项目。
