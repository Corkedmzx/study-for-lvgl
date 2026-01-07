# Media Player 模块文档

## 模块概述

`media_player` 模块提供音频和视频播放功能，使用MPlayer作为后端播放引擎。模块分为两个独立的部分：音频播放器和视频播放器。

## 文件结构

- `audio_player.h` / `audio_player.c` - 音频播放器（独立模块）
- `simple_video_player.h` / `simple_video_player.c` - 视频播放器（全屏播放）

## 音频播放器 (audio_player)

### 主要功能

#### `audio_player_init()`

初始化音频播放器。

**函数签名：**
```c
void audio_player_init(void);
```

**功能：**
- 初始化播放器状态变量
- 重置PID、管道、速度、播放状态等

**调用位置：**
- `main.c:46` - 程序启动时初始化

#### `audio_player_play()`

播放音频文件。

**函数签名：**
```c
bool audio_player_play(const char *file_path);
```

**功能：**
- 使用MPlayer播放音频文件
- 创建子进程运行MPlayer
- 使用管道进行控制通信
- 支持多种音频格式（MP3、WAV、OGG、FLAC、AAC、M4A）

**参数：**
- `file_path` - 音频文件路径

**返回值：**
- 成功返回true，失败返回false

**调用位置：**
- `src/ui/music_win.c` - 音乐窗口播放音频
- `src/ui/ui_screens.c` - 播放器屏幕播放音频
- `src/ui/ui_callbacks.c` - 各种回调函数中播放音频

**实现细节：**
1. 检查文件是否存在
2. 如果已有播放器在运行，先停止
3. 创建管道用于控制通信
4. 使用fork创建子进程
5. 子进程执行MPlayer，参数：
   - `-slave` - 启用slave模式（通过标准输入接收命令）
   - `-quiet` - 静默模式
6. 父进程保存PID和管道文件描述符

#### `audio_player_stop()`

停止播放。

**函数签名：**
```c
void audio_player_stop(void);
```

**功能：**
- 通过管道发送quit命令
- 等待进程退出（最多等待1秒）
- 如果进程未退出，强制终止（SIGTERM）

**调用位置：**
- 各种UI回调函数中停止播放
- 切换音频时先停止当前播放

#### `audio_player_toggle_pause()`

暂停/恢复播放。

**函数签名：**
```c
void audio_player_toggle_pause(void);
```

**功能：**
- 通过管道发送pause命令
- 切换暂停状态标志

#### `audio_player_is_playing()`

获取播放状态。

**函数签名：**
```c
bool audio_player_is_playing(void);
```

**返回值：**
- 正在播放返回true，否则返回false

#### `audio_player_is_paused()`

获取暂停状态。

**函数签名：**
```c
bool audio_player_is_paused(void);
```

#### `audio_player_set_speed()`

设置播放速度。

**函数签名：**
```c
void audio_player_set_speed(float speed);
```

**功能：**
- 通过管道发送 `speed_set` 命令
- 更新内部速度变量

#### `audio_player_volume_up()`

增加音量。

**函数签名：**
```c
void audio_player_volume_up(void);
```

**功能：**
- 通过管道发送 `volume +10` 命令

#### `audio_player_volume_down()`

减少音量。

**函数签名：**
```c
void audio_player_volume_down(void);
```

**功能：**
- 通过管道发送 `volume -10` 命令

#### `audio_player_is_running()`

检查MPlayer是否仍在运行。

**函数签名：**
```c
bool audio_player_is_running(void);
```

**功能：**
- 使用 `waitpid()` 非阻塞检查进程状态
- 如果进程已退出，更新内部状态

#### `audio_player_set_status_callback()`

设置状态更新回调函数。

**函数签名：**
```c
void audio_player_set_status_callback(void (*callback)(const char *));
```

**功能：**
- 设置回调函数，用于UI更新播放状态

**调用位置：**
- `src/ui/ui_screens.c:515` - 初始化播放器屏幕时设置回调

## 视频播放器 (simple_video_player)

### 主要功能

#### `simple_video_init()`

初始化视频播放器。

**函数签名：**
```c
void simple_video_init(void);
```

**功能：**
- 初始化播放器状态变量
- 初始化互斥锁

**调用位置：**
- `main.c:49` - 程序启动时初始化

#### `simple_video_play()`

播放视频文件（全屏）。

**函数签名：**
```c
bool simple_video_play(const char *file_path);
```

**功能：**
- 使用MPlayer全屏播放视频
- 使用framebuffer输出（`-vo fbdev2`）
- 创建FIFO管道用于控制
- 支持多种视频格式（MP4、AVI、MKV、MOV、FLV、WMV）

**参数：**
- `file_path` - 视频文件路径

**返回值：**
- 成功返回true，失败返回false

**调用位置：**
- `src/ui/video_win.c` - 视频窗口播放视频
- `src/ui/video_touch_control.c` - 触屏控制切换视频

**实现细节：**
1. 如果正在播放，先停止并等待退出
2. 创建FIFO管道（`/tmp/mplayer_fifo`）
3. 使用fork创建子进程
4. 子进程执行MPlayer，参数：
   - `-vo fbdev2` - 使用framebuffer输出
   - `-cache 32768` - 缓存大小
   - `-lavdopts skipframe=nonref:skiploopfilter=all:fast:threads=8` - 解码器优化
   - `-vf scale=800:480,format=bgr24` - 视频滤镜（缩放和格式转换）
   - `-framedrop` - 抽帧减少卡顿
   - `-slave` - 启用slave模式
   - `-input file=/tmp/mplayer_fifo` - FIFO控制
5. 父进程打开FIFO用于发送控制命令

#### `simple_video_stop()`

停止播放。

**函数签名：**
```c
void simple_video_stop(void);
```

**功能：**
- 通过FIFO发送quit命令
- 使用SIGTERM终止进程
- 清理FIFO文件

**调用位置：**
- 各种UI回调函数中停止播放
- 返回主页时停止视频

#### `simple_video_force_stop()`

强制停止播放。

**函数签名：**
```c
void simple_video_force_stop(void);
```

**功能：**
- 立即使用SIGTERM终止MPlayer进程
- 不使用互斥锁（单线程退出逻辑）

**调用位置：**
- `src/ui/ui_screens.c:641` - 返回主页时强制停止

#### `simple_video_toggle_pause()`

暂停/恢复播放。

**函数签名：**
```c
void simple_video_toggle_pause(void);
```

**功能：**
- 通过FIFO发送pause命令
- 切换暂停状态

#### `simple_video_prev()`

切换到上一首视频。

**函数签名：**
```c
void simple_video_prev(void);
```

**功能：**
- 查找上一个视频文件
- 停止当前播放
- 播放新视频
- 更新 `current_video_index`

**调用位置：**
- `src/ui/video_touch_control.c` - 触屏控制（左下角点击）

#### `simple_video_next()`

切换到下一首视频。

**函数签名：**
```c
void simple_video_next(void);
```

**功能：**
- 查找下一个视频文件
- 停止当前播放
- 播放新视频
- 更新 `current_video_index`

**调用位置：**
- `src/ui/video_touch_control.c` - 触屏控制（右下角点击）

#### `simple_video_speed_up()`

加速播放。

**函数签名：**
```c
void simple_video_speed_up(void);
```

**功能：**
- 播放速度增加0.1
- 最大速度2.0x
- 通过FIFO发送 `speed_set` 命令

**调用位置：**
- `src/ui/video_touch_control.c` - 触屏控制（右划）

#### `simple_video_speed_down()`

减速播放。

**函数签名：**
```c
void simple_video_speed_down(void);
```

**功能：**
- 播放速度减少0.1
- 最小速度0.5x

**调用位置：**
- `src/ui/video_touch_control.c` - 触屏控制（左划）

#### `simple_video_volume_up()`

增加音量。

**函数签名：**
```c
void simple_video_volume_up(void);
```

**功能：**
- 通过FIFO发送 `volume +10` 命令

**调用位置：**
- `src/ui/video_touch_control.c` - 触屏控制（上划）

#### `simple_video_volume_down()`

减少音量。

**函数签名：**
```c
void simple_video_volume_down(void);
```

**功能：**
- 通过FIFO发送 `volume -10` 命令

**调用位置：**
- `src/ui/video_touch_control.c` - 触屏控制（下划）

#### `simple_video_is_playing()`

获取播放状态。

**函数签名：**
```c
bool simple_video_is_playing(void);
```

**功能：**
- 检查播放状态标志
- 如果标志为true，检查进程是否还在运行
- 如果进程已退出，更新状态

**返回值：**
- 正在播放返回true，否则返回false

**调用位置：**
- `src/common/common.c:61` - 检查视频播放状态（避免mmap冲突）
- `src/ui/ui_screens.c` - 返回主页时检查状态

## 模块调用关系

### 音频播放器调用

1. **main.c**
   ```c
   audio_player_init();  // 初始化
   ```

2. **src/ui/music_win.c**
   ```c
   audio_player_play();      // 播放音频
   audio_player_stop();      // 停止播放
   ```

3. **src/ui/ui_screens.c**
   ```c
   audio_player_play();              // 播放音频
   audio_player_stop();             // 停止播放
   audio_player_set_status_callback();  // 设置状态回调
   ```

4. **src/ui/ui_callbacks.c**
   ```c
   audio_player_play();      // 各种回调中播放
   audio_player_stop();      // 各种回调中停止
   ```

### 视频播放器调用

1. **main.c**
   ```c
   simple_video_init();  // 初始化
   ```

2. **src/ui/video_win.c**
   ```c
   simple_video_play();   // 播放视频
   simple_video_stop();   // 停止播放
   ```

3. **src/ui/video_touch_control.c**
   ```c
   simple_video_prev();        // 上一首
   simple_video_next();        // 下一首
   simple_video_toggle_pause(); // 暂停/恢复
   simple_video_speed_up();     // 加速
   simple_video_speed_down();   // 减速
   simple_video_volume_up();    // 音量+
   simple_video_volume_down();  // 音量-
   ```

4. **src/common/common.c**
   ```c
   simple_video_is_playing();  // 检查播放状态
   ```

## 使用示例

### 示例1：播放音频

```c
#include "src/media_player/audio_player.h"

audio_player_init();
if (audio_player_play("/mdata/music.mp3")) {
    printf("音频播放成功\n");
}
```

### 示例2：播放视频

```c
#include "src/media_player/simple_video_player.h"

simple_video_init();
if (simple_video_play("/mdata/video.mp4")) {
    printf("视频播放成功\n");
}
```

### 示例3：控制播放

```c
// 暂停/恢复
audio_player_toggle_pause();
simple_video_toggle_pause();

// 调整音量
audio_player_volume_up();
simple_video_volume_up();

// 调整速度
audio_player_set_speed(1.5f);
simple_video_speed_up();
```

## 实现细节

### MPlayer控制

1. **音频播放器**：使用管道（pipe）进行控制
   - 子进程的标准输入重定向到管道
   - 父进程通过管道发送命令

2. **视频播放器**：使用FIFO文件进行控制
   - 创建 `/tmp/mplayer_fifo` FIFO文件
   - MPlayer通过 `-input file=/tmp/mplayer_fifo` 参数读取命令
   - 父进程打开FIFO写入命令

### 进程管理

1. **进程创建**：使用 `fork()` 创建子进程
2. **进程终止**：
   - 先尝试通过命令退出（quit）
   - 等待进程退出（最多1秒）
   - 如果未退出，使用SIGTERM强制终止

3. **进程状态检查**：使用 `waitpid()` 非阻塞检查

### 性能优化（视频播放器）

1. **硬件加速解码**：
   - `skipframe=nonref` - 跳过非参考帧
   - `skiploopfilter=all` - 跳过循环滤波
   - `fast` - 快速模式
   - `threads=8` - 8线程解码

2. **视频处理**：
   - `scale=800:480` - 缩放到屏幕尺寸
   - `format=bgr24` - 格式转换
   - `-framedrop` - 抽帧减少卡顿

3. **缓存优化**：
   - `-cache 32768` - 32MB缓存

## 注意事项

1. **MPlayer依赖**：系统必须安装MPlayer，程序会尝试多个路径查找
2. **权限要求**：播放视频需要访问framebuffer设备（`/dev/fb0`）
3. **FIFO文件**：视频播放器使用 `/tmp/mplayer_fifo`，需要确保 `/tmp` 目录可写
4. **线程安全**：视频播放器使用互斥锁保护，音频播放器未使用（单线程访问）
5. **进程清理**：程序退出时应调用 `audio_player_cleanup()` 和 `simple_video_cleanup()`
6. **状态同步**：播放状态通过进程检查保持同步，避免状态不一致

## 相关文件

- `main.c` - 初始化播放器
- `src/ui/music_win.c` - 音乐窗口，使用音频播放器
- `src/ui/video_win.c` - 视频窗口，使用视频播放器
- `src/ui/video_touch_control.c` - 视频触屏控制，使用视频播放器
- `src/ui/ui_screens.c` - 播放器屏幕，使用音频播放器
- `src/common/common.c` - 检查视频播放状态

