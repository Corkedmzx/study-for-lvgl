# UI 模块文档

## 模块概述

`ui` 模块负责创建和管理所有用户界面。该模块包含主屏幕、各种功能窗口（相册、音乐、视频、LED控制、天气、计时器、时钟、2048游戏、退出确认等）以及UI事件处理。

## 文件结构

- `ui_screens.h` / `ui_screens.c` - 主屏幕和基础UI创建
- `ui_callbacks.h` / `ui_callbacks.c` - UI事件回调函数
- `album_win.h` / `album_win.c` - 相册窗口
- `music_win.h` / `music_win.c` - 音乐播放窗口
- `video_win.h` / `video_win.c` - 视频播放窗口
- `led_win.h` / `led_win.c` - LED/蜂鸣器控制窗口
- `weather_win.h` / `weather_win.c` - 天气窗口
- `timer_win.h` / `timer_win.c` - 定时器窗口
- `clock_win.h` / `clock_win.c` - 时钟窗口
- `game_2048_win.h` / `game_2048_win.c` - 2048游戏窗口
- `exit_win.h` / `exit_win.c` - 退出确认窗口
- `login_win.h` / `login_win.c` - 密码锁窗口
- `screensaver_win.h` / `screensaver_win.c` - 屏保窗口
- `video_touch_control.h` / `video_touch_control.c` - 视频触屏控制

## 主要功能模块

### 1. UI Screens (ui_screens)

#### `create_main_screen()`

创建主屏幕（九宫格布局）。

**功能：**
- 创建主屏幕对象
- 加载背景图片（`/mdata/index.bmp`）
- 创建九宫格按钮布局：
  - 第一行：相册、视频、音乐
  - 第二行：LED控制、天气、计时器
  - 第三行：时钟、2048、退出
- 每个按钮包含图标和文字
- 使用FontAwesome字体显示图标

**调用位置：**
- `main.c:58` - 程序启动时创建

#### `create_image_screen()`

创建图片展示屏幕。

**功能：**
- 创建图片屏幕对象
- 创建返回按钮
- 创建标题
- 如果未扫描图片，自动扫描

**调用位置：**
- `main.c:59` - 程序启动时创建

#### `create_player_screen()`

创建播放器屏幕。

**功能：**
- 创建播放器屏幕对象
- 创建播放列表容器（左侧，可滚动）
- 创建视频容器（右侧，用于视频播放）
- 创建多媒体控制区域（播放、停止、上一首、下一首、音量、速度控制）
- 创建状态显示区域（状态标签、速度标签）
- 初始化播放列表

**调用位置：**
- `main.c:60` - 程序启动时创建

#### `main_window_event_handler()`

主窗口事件处理器。

**功能：**
- 处理主屏幕按钮点击事件
- 根据按钮文本执行相应功能：
  - "相册" → `show_album_window()`
  - "视频" → `video_win_show()`
  - "音乐" → `music_win_show()`
  - "LED控制" → `show_led_window()`
  - "天气" → `show_weather_window()`
  - "计时器" → `timer_win_show()`
  - "时钟" → `clock_win_show()`
  - "2048" → `game_2048_win_show()`
  - "退出" → `exit_application()`

**调用位置：**
- 主屏幕按钮点击时自动调用

#### `back_to_main_cb()`

返回主屏幕回调函数。

**功能：**
- 停止所有正在播放的媒体（视频、音频）
- 隐藏所有功能窗口
- 切换到主屏幕
- 使用快速刷新函数强制刷新屏幕

**调用位置：**
- 各种返回按钮点击时调用

### 2. Album Window (album_win)

相册窗口，用于浏览和查看图片。

**主要函数：**
- `show_album_window()` - 显示相册窗口
- 图片切换功能（上一张、下一张）
- 使用 `image_viewer` 模块显示图片

**调用位置：**
- `ui_screens.c:777` - 主屏幕"相册"按钮点击时

### 3. Music Window (music_win)

音乐播放窗口，用于播放音频文件。

**主要函数：**
- `music_win_show()` - 显示音乐窗口
- 播放列表显示
- 播放控制（播放、停止、上一首、下一首）
- 使用 `audio_player` 模块播放音频

**调用位置：**
- `ui_screens.c:781` - 主屏幕"音乐"按钮点击时

### 4. Video Window (video_win)

视频播放窗口，用于播放视频文件。

**主要函数：**
- `video_win_show()` - 显示视频窗口
- 视频列表显示
- 视频播放控制
- 使用 `simple_video_player` 模块播放视频

**调用位置：**
- `ui_screens.c:779` - 主屏幕"视频"按钮点击时

### 5. LED Window (led_win)

LED/蜂鸣器控制窗口。

**主要函数：**
- `show_led_window()` - 显示LED控制窗口
- LED灯控制（开/关）
- 蜂鸣器控制（开/关）
- 通过文件系统接口控制硬件（`/dev/led`, `/dev/beep`）

**调用位置：**
- `ui_screens.c:783` - 主屏幕"LED控制"按钮点击时

### 6. Weather Window (weather_win)

天气窗口，显示天气信息。

**主要函数：**
- `show_weather_window()` - 显示天气窗口
- 调用 `weather` 模块获取天气数据
- 解析并显示多天天气信息
- 支持刷新天气数据

**调用位置：**
- `ui_screens.c:785` - 主屏幕"天气"按钮点击时

### 7. Timer Window (timer_win)

定时器窗口。

**主要函数：**
- `timer_win_show()` - 显示定时器窗口
- 定时器设置和倒计时功能

**调用位置：**
- `ui_screens.c:787` - 主屏幕"计时器"按钮点击时

### 8. Clock Window (clock_win)

时钟窗口，显示当前时间。

**主要函数：**
- `clock_win_show()` - 显示时钟窗口
- 实时显示当前时间
- 使用系统时间，自动更新

**调用位置：**
- `ui_screens.c:789` - 主屏幕"时钟"按钮点击时

### 9. Game 2048 Window (game_2048_win)

2048游戏窗口。

**主要函数：**
- `game_2048_win_show()` - 显示2048游戏窗口
- 使用 `game_2048` 模块实现游戏逻辑
- 触屏手势控制（左划、右划、上划、下划）
- 游戏状态显示（分数、游戏结束提示）
- 支持重置游戏

**调用位置：**
- `ui_screens.c:792` - 主屏幕"2048"按钮点击时

### 10. Exit Window (exit_win)

退出确认窗口。

**主要函数：**
- `exit_application()` - 退出应用程序
- 显示确认对话框
- 清理资源并退出程序

**调用位置：**
- `ui_screens.c:795` - 主屏幕"退出"按钮点击时

### 11. Login Window (login_win)

密码锁窗口。

**主要函数：**
- `login_win_show()` - 显示密码锁窗口
- 密码输入界面（数字键盘）
- 默认密码：`114514`
- 密码正确后进入主界面

**调用位置：**
- `main.c` - 程序启动时，屏保解锁后显示

### 12. Screensaver Window (screensaver_win)

屏保窗口。

**主要函数：**
- `screensaver_win_show()` - 显示屏保窗口
- 触摸解锁功能
- 解锁后进入密码锁界面

**调用位置：**
- `main.c:63` - 程序启动时首先显示

### 13. Video Touch Control (video_touch_control)

视频触屏控制模块。

**主要函数：**
- `video_touch_control_init()` - 初始化触屏控制
- `video_touch_control_start()` - 启动触屏控制线程
- `video_touch_control_stop()` - 停止触屏控制线程

**触屏手势：**
- **左上角点击**：返回主页
- **左下角点击**：上一首视频
- **右下角点击**：下一首视频
- **右上角点击**：暂停/恢复播放
- **上划**：增加音量
- **下划**：减少音量
- **右划**：加速播放
- **左划**：减速播放

**调用位置：**
- `main.c:50` - 程序启动时初始化
- `src/ui/video_win.c` - 视频播放时启动

## 模块调用关系

### 被调用情况

1. **main.c**
   ```c
   create_main_screen();        // 创建主屏幕
   create_image_screen();       // 创建图片屏幕
   create_player_screen();      // 创建播放器屏幕
   screensaver_win_show();      // 显示屏保
   ```

2. **主屏幕按钮点击**
   - 通过 `main_window_event_handler()` 处理
   - 调用相应的窗口显示函数

3. **各种窗口**
   - 通过返回按钮调用 `back_to_main_cb()` 返回主屏幕

### 依赖关系

- **common模块**：使用全局变量（屏幕对象、媒体索引等）
- **image_viewer模块**：相册窗口使用
- **media_player模块**：音乐和视频窗口使用
- **weather模块**：天气窗口使用
- **game_2048模块**：2048游戏窗口使用
- **file_scanner模块**：获取媒体文件列表
- **LVGL库**：所有UI组件

## 使用示例

### 示例1：创建主屏幕

```c
#include "src/ui/ui_screens.h"

create_main_screen();
lv_scr_load(main_screen);
```

### 示例2：显示相册窗口

```c
#include "src/ui/album_win.h"

show_album_window();
```

### 示例3：返回主屏幕

```c
#include "src/ui/ui_screens.h"

back_to_main_cb(event);
```

## 实现细节

### UI布局

1. **主屏幕**：九宫格布局，3行3列
2. **图片屏幕**：图片容器 + 控制按钮
3. **播放器屏幕**：播放列表（左） + 视频容器（右） + 控制面板（下）

### 事件处理

1. **按钮点击**：使用 `LV_EVENT_CLICKED` 事件
2. **触屏手势**：使用独立的触屏控制线程处理
3. **定时器更新**：使用LVGL定时器更新UI

### 资源管理

1. **屏幕对象**：全局变量，在 `common.h` 中定义
2. **内存管理**：LVGL自动管理UI对象内存
3. **资源清理**：程序退出时自动清理

## 注意事项

1. **初始化顺序**：必须先创建屏幕，再显示
2. **线程安全**：UI操作应在主线程中进行
3. **屏幕切换**：使用 `lv_scr_load()` 切换屏幕
4. **对象隐藏**：使用 `lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN)` 隐藏对象
5. **字体支持**：需要加载中文字体和FontAwesome字体
6. **触屏控制**：视频播放时的触屏控制使用独立线程，需要注意线程同步

## 相关文件

- `main.c` - 程序入口，创建UI屏幕
- `src/common/common.h` - 定义全局变量
- 其他功能模块 - 提供具体功能实现

## UI窗口详细说明

### 主屏幕布局

```
┌─────────────────────────────────┐
│      LVGL多媒体系统 (标题)        │
├──────────┬──────────┬──────────┤
│  相册    │   视频    │   音乐   │
├──────────┼──────────┼──────────┤
│ LED控制  │   天气    │  计时器  │
├──────────┼──────────┼──────────┤
│  时钟    │   2048    │   退出   │
└──────────┴──────────┴──────────┘
```

### 播放器屏幕布局

```
┌─────────────────────────────────────┐
│  播放列表          │  视频容器        │
│  (可滚动)          │  (800x480)      │
│                    │                 │
│  - 音频1           │                 │
│  - 音频2           │                 │
│  - 音频3           │                 │
│                    │                 │
├────────────────────┴─────────────────┤
│  状态: 播放中  速度: 1.00x           │
├─────────────────────────────────────┤
│  上一首  播放  停止  下一首          │
│  音量-  音量+  速度-  速度+          │
└─────────────────────────────────────┘
```

## 扩展建议

1. **主题支持**：支持多种UI主题
2. **动画效果**：添加页面切换动画
3. **手势识别**：扩展更多触屏手势
4. **多语言支持**：支持多语言界面
5. **设置界面**：添加系统设置界面

