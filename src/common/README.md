# Common 模块文档

## 模块概述

`common` 模块提供项目中的公共定义、全局变量和通用功能函数。该模块是整个项目的基础，为其他模块提供共享的数据结构和工具函数。

## 文件结构

- `common.h` - 公共定义和全局变量声明
- `common.c` - 公共定义和全局变量定义，以及工具函数实现
- `touch_device.h` - 触摸屏设备管理接口
- `touch_device.c` - 触摸屏设备管理实现

## 主要功能

### 1. 目录路径定义

在 `common.h` 中定义了媒体文件的默认目录路径：

```c
#define IMAGE_DIR "/mdata"    // 图片目录路径
#define MEDIA_DIR "/mdata"    // 音视频目录路径
```

### 2. BMP文件结构定义

定义了BMP文件格式的数据结构，用于图片查看器模块：

- `struct bmp_header` - BMP文件头结构
- `struct bmp_info_header` - BMP信息头结构

### 3. 全局变量

模块定义了以下全局变量，供其他模块使用：

#### 屏幕对象
- `main_screen` - 主屏幕对象
- `image_screen` - 图片展示屏幕对象
- `player_screen` - 播放器屏幕对象
- `video_container` - 视频容器对象
- `video_back_btn` - 视频播放时的返回按钮

#### 播放列表相关
- `playlist_container` - 播放列表容器
- `playlist_list` - 播放列表滚动容器
- `speed_label` - 播放速度标签
- `status_label` - 播放状态标签

#### 图片显示相关
- `current_img_obj` - 当前图片对象
- `img_container` - 图片容器
- `img_info_label` - 图片信息标签
- `current_img_index` - 当前图片索引
- `is_gif_obj` - 是否为GIF对象标志
- `canvas_buf` - Canvas缓冲区（680 * 280）

#### 媒体播放相关
- `current_audio_index` - 当前音频索引
- `current_video_index` - 当前视频索引

#### 控制标志
- `should_exit` - 程序退出标志
- `need_return_to_main` - 需要返回主页标志（用于视频退出）
- `need_update_2048_display` - 需要更新2048游戏显示标志

### 4. 工具函数

#### `fast_refresh_main_screen()`

快速刷新主屏幕到framebuffer，使用内存映射优化。

**功能：**
- 使主屏幕及其所有子对象无效化
- 处理LVGL定时器，完成渲染
- 使用内存映射（mmap）强制同步framebuffer
- 在视频播放时不会执行mmap操作，避免与MPlayer冲突

**调用位置：**
- `main.c` - 视频退出后快速恢复主页显示
- `src/ui/ui_screens.c` - 返回主页时刷新屏幕

**实现细节：**
```c
void fast_refresh_main_screen(void) {
    // 1. 检查是否正在播放视频
    // 2. 使所有对象无效化
    // 3. 处理定时器（20次循环）
    // 4. 强制刷新
    // 5. 非视频播放时使用mmap同步framebuffer
    // 6. 最终强制刷新
}
```

#### `force_refresh_main_buttons()`

强制刷新主页所有按钮和控件，确保按钮背景和文字不透明。

**功能：**
- 确保主屏幕背景不透明
- 递归使所有子对象无效化
- 恢复退出按钮的红色背景和白色文字
- 确保所有文字不透明
- 多次处理定时器，确保重新渲染

**调用位置：**
- 主要用于修复视频播放后返回主页时按钮显示异常的问题

### 5. 触摸屏设备管理

#### `touch_device_init()`

初始化触摸屏设备。

**功能：**
- 打开触摸屏设备文件 `/dev/input/event0`
- 设置为非阻塞模式
- 返回设备文件描述符

**调用位置：**
- `main.c:41` - 程序启动时初始化

**返回值：**
- 成功返回0，失败返回-1

#### `touch_device_deinit()`

关闭触摸屏设备。

**调用位置：**
- `main.c:133` - 程序退出时清理

#### `touch_device_get_fd()`

获取触摸屏设备文件描述符。

**返回值：**
- 成功返回文件描述符，失败返回-1

#### `touch_device_is_initialized()`

检查触摸屏设备是否已初始化。

**返回值：**
- 已初始化返回true，否则返回false

## 模块调用关系

### 被调用情况

1. **main.c**
   - 使用全局变量：`main_screen`, `should_exit`, `need_return_to_main`
   - 调用函数：`fast_refresh_main_screen()`, `touch_device_init()`, `touch_device_deinit()`

2. **src/ui/ui_screens.c**
   - 使用全局变量：所有屏幕对象和UI控件
   - 调用函数：`fast_refresh_main_screen()`, `back_to_main_cb()`

3. **src/image_viewer/image_viewer.c**
   - 使用全局变量：`image_screen`, `current_img_obj`, `img_container`, `img_info_label`, `current_img_index`, `is_gif_obj`, `canvas_buf`

4. **src/media_player/simple_video_player.c**
   - 使用全局变量：`current_video_index`

5. **src/ui/video_touch_control.c**
   - 使用全局变量：`need_return_to_main`

6. **src/ui/game_2048_win.c**
   - 使用全局变量：`need_update_2048_display`

### 依赖关系

- 依赖 `lvgl/lvgl.h` - LVGL图形库
- 依赖 `src/media_player/simple_video_player.h` - 检查视频播放状态

## 使用示例

### 示例1：快速刷新主屏幕

```c
// 在视频退出后快速恢复主页显示
extern void fast_refresh_main_screen(void);
fast_refresh_main_screen();
```

### 示例2：初始化触摸屏设备

```c
#include "src/common/touch_device.h"

if (touch_device_init() != 0) {
    printf("警告: 触摸屏设备初始化失败\n");
}
```

### 示例3：获取触摸屏文件描述符

```c
int touch_fd = touch_device_get_fd();
if (touch_fd >= 0) {
    // 使用触摸屏设备
}
```

## 注意事项

1. **视频播放时的mmap冲突**：`fast_refresh_main_screen()` 函数会检查视频播放状态，避免在视频播放时使用mmap操作，防止与MPlayer的framebuffer访问冲突。

2. **全局变量的线程安全**：多个模块可能同时访问全局变量，需要注意线程安全问题。特别是在视频播放和主线程之间。

3. **内存管理**：`canvas_buf` 是静态分配的缓冲区，大小为 `680 * 280`，用于BMP图片显示。

4. **触摸屏设备路径**：触摸屏设备路径硬编码为 `/dev/input/event0`，如果设备路径不同，需要修改 `touch_device.c`。

## 相关文件

- `main.c` - 程序入口，使用common模块的全局变量和函数
- `src/ui/ui_screens.c` - UI界面创建，使用common模块的屏幕对象
- `src/image_viewer/image_viewer.c` - 图片查看器，使用common模块的图片相关变量
- `src/media_player/simple_video_player.c` - 视频播放器，使用common模块的视频相关变量

