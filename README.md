# LVGL for Frame Buffer Device

基于 LVGL 的嵌入式 Linux GUI 应用程序，配置为在 Linux 系统上使用 `/dev/fb0` 帧缓冲设备运行。

## 项目简介

本项目是基于 [lv_port_linux](https://github.com/lvgl/lv_port_linux) 开发的嵌入式图形界面应用，针对 **粤嵌 GEC6818** 开发板进行了优化和功能扩展。项目使用 LVGL v8.2 图形库，通过 Linux framebuffer 驱动实现图形显示，支持触摸屏交互。

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
- 支持 AVI、MKV 等格式
- 基于 MPlayer + framebuffer 实现
- 触摸屏控制（播放/暂停、进度调节等）
- **注意**: 视频播放存在卡顿问题，正在处理中

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

## 配置说明

### 媒体文件目录

默认媒体文件目录配置在 `src/common/common.h` 中：

```c
#define IMAGE_DIR "/mdata"    // 图片目录路径
#define MEDIA_DIR "/mdata"    // 音视频目录路径
```

如需修改，请编辑该文件后重新编译。

### 编译选项

编译配置在 `Makefile` 中：

- **编译器**: `arm-linux-gnueabihf-gcc`
- **链接方式**: 静态链接 (`-static`) 以避免 GLIBC 版本不匹配问题
- **优化级别**: `-O3 -g0`

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

### 架构特点

- 模块化设计，各功能独立实现
- 使用硬件抽象层（HAL）隔离硬件细节
- 主线程处理 UI 渲染，后台线程处理媒体播放
- 内存映射优化，提升显示刷新速度

## 参考资源

- LVGL 官方博客教程: [Linux Framebuffer](https://blog.lvgl.io/2018-01-03/linux_fb)
- LVGL 官方文档: [https://docs.lvgl.io/](https://docs.lvgl.io/)

## 已知问题

1. **视频播放卡顿**: 视频播放功能存在卡顿问题，正在处理中
2. **天气地址固定**: 天气获取功能使用固定地址，修改需从源码修改

## 注意事项

### 许可证

- 此仓库创建时未设置许可证（使用默认）
- 请根据 LVGL 官方的许可声明使用该仓库代码
- 如有侵权请及时联系删除
- **此仓库代码仅供学习，不做任何商业用途**

### 开发环境

- 本项目针对 ARM Linux 平台编译
- 需要在交叉编译环境中构建
- 确保目标设备已安装必要的系统库和 MPlayer

## 贡献

欢迎提交 Issue 和 Pull Request 来改进项目。

## 更新日志

- **v8.2**: 基于 LVGL v8.2 版本
- 添加了多个功能模块（相册、播放器、游戏等）
- 优化了视频播放和 UI 交互
