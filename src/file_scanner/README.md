# File Scanner 模块文档

## 模块概述

`file_scanner` 模块负责扫描指定目录中的媒体文件（图片、音频、视频），并将文件路径和名称存储在全局数组中，供其他模块使用。

## 文件结构

- `file_scanner.h` - 模块接口定义
- `file_scanner.c` - 模块实现

## 主要功能

### 1. 图片文件扫描

#### `scan_image_directory()`

扫描指定目录中的图片文件（BMP和GIF格式）。

**函数签名：**
```c
int scan_image_directory(const char *dir_path);
```

**功能：**
- 遍历指定目录，查找 `.bmp` 和 `.gif` 文件
- 将文件路径存储在全局数组 `image_files` 中
- 生成显示名称（文件名 + 类型标识，如 "image (BMP)"）
- 支持动态扩容（初始容量32，按需翻倍）

**支持的格式：**
- `.bmp` - BMP位图文件
- `.gif` - GIF动画文件

**返回值：**
- 成功返回找到的文件数量
- 失败返回-1

**全局变量：**
- `image_files` - 图片文件路径数组（`char **`）
- `image_names` - 图片显示名称数组（`char **`）
- `image_count` - 图片文件数量（`int`）

**调用位置：**
- `main.c:53` - 程序启动时扫描图片目录
- `src/ui/ui_screens.c:264` - 创建图片屏幕时扫描（如果未扫描）

**实现细节：**
1. 先释放旧的数组（如果存在）
2. 分配初始内存（容量32）
3. 打开目录并遍历文件
4. 检查文件扩展名，过滤出BMP和GIF文件
5. 如果数组已满，进行扩容（容量翻倍）
6. 分配内存存储文件路径和显示名称
7. 添加NULL终止符

### 2. 音频文件扫描

#### `scan_audio_directory()`

扫描指定目录中的音频文件。

**函数签名：**
```c
int scan_audio_directory(const char *dir_path);
```

**功能：**
- 遍历指定目录，查找音频文件
- 将文件路径存储在全局数组 `audio_files` 中
- 生成显示名称（文件名 + " (音频)" 后缀）

**支持的格式：**
- `.mp3` - MP3音频文件
- `.wav` - WAV音频文件
- `.ogg` - OGG音频文件
- `.flac` - FLAC无损音频文件
- `.aac` - AAC音频文件
- `.m4a` - M4A音频文件

**返回值：**
- 成功返回找到的文件数量
- 失败返回-1

**全局变量：**
- `audio_files` - 音频文件路径数组（`char **`）
- `audio_names` - 音频显示名称数组（`char **`）
- `audio_count` - 音频文件数量（`int`）

**调用位置：**
- `main.c:54` - 程序启动时扫描音频目录

### 3. 视频文件扫描

#### `scan_video_directory()`

扫描指定目录中的视频文件。

**函数签名：**
```c
int scan_video_directory(const char *dir_path);
```

**功能：**
- 遍历指定目录，查找视频文件
- 将文件路径存储在全局数组 `video_files` 中
- 生成显示名称（文件名 + " (视频)" 后缀）

**支持的格式：**
- `.mp4` - MP4视频文件
- `.avi` - AVI视频文件
- `.mkv` - MKV视频文件
- `.mov` - MOV视频文件
- `.flv` - FLV视频文件
- `.wmv` - WMV视频文件

**返回值：**
- 成功返回找到的文件数量
- 失败返回-1

**全局变量：**
- `video_files` - 视频文件路径数组（`char **`）
- `video_names` - 视频显示名称数组（`char **`）
- `video_count` - 视频文件数量（`int`）

**调用位置：**
- `main.c:55` - 程序启动时扫描视频目录

### 4. 内存管理函数

#### `free_image_arrays()`

释放图片文件数组内存。

**函数签名：**
```c
void free_image_arrays(void);
```

**功能：**
- 释放 `image_files` 和 `image_names` 数组
- 重置 `image_count` 为0

#### `free_audio_arrays()`

释放音频文件数组内存。

**函数签名：**
```c
void free_audio_arrays(void);
```

#### `free_video_arrays()`

释放视频文件数组内存。

**函数签名：**
```c
void free_video_arrays(void);
```

### 5. 获取函数

提供了一系列getter函数，用于获取扫描结果：

- `get_image_files()` - 获取图片文件路径数组
- `get_image_names()` - 获取图片名称数组
- `get_image_count()` - 获取图片文件数量
- `get_audio_files()` - 获取音频文件路径数组
- `get_audio_names()` - 获取音频名称数组
- `get_audio_count()` - 获取音频文件数量
- `get_video_files()` - 获取视频文件路径数组
- `get_video_names()` - 获取视频名称数组
- `get_video_count()` - 获取视频文件数量

## 模块调用关系

### 被调用情况

1. **main.c**
   ```c
   scan_image_directory(IMAGE_DIR);  // IMAGE_DIR = "/mdata"
   scan_audio_directory(MEDIA_DIR);  // MEDIA_DIR = "/mdata"
   scan_video_directory(MEDIA_DIR);
   ```

2. **src/ui/ui_screens.c**
   ```c
   // 创建图片屏幕时，如果未扫描则扫描
   if (image_count == 0 || image_files == NULL) {
       scan_image_directory(IMAGE_DIR);
   }
   ```

3. **src/image_viewer/image_viewer.c**
   - 使用全局变量：`image_files`, `image_names`, `image_count`
   - 通过extern声明访问这些变量

4. **src/ui/music_win.c**
   - 使用全局变量：`audio_files`, `audio_names`, `audio_count`

5. **src/ui/video_win.c**
   - 使用全局变量：`video_files`, `video_names`, `video_count`

6. **src/media_player/simple_video_player.c**
   - 使用全局变量：`video_files`, `video_count`, `current_video_index`

### 依赖关系

- 依赖标准C库：`stdio.h`, `stdlib.h`, `string.h`, `dirent.h`, `sys/stat.h`
- 不依赖其他项目模块

## 使用示例

### 示例1：扫描图片目录

```c
#include "src/file_scanner/file_scanner.h"

int count = scan_image_directory("/mdata");
if (count > 0) {
    printf("找到 %d 个图片文件\n", count);
    
    // 获取文件列表
    char **files = get_image_files();
    for (int i = 0; i < count; i++) {
        printf("图片[%d]: %s\n", i, files[i]);
    }
}
```

### 示例2：使用扫描结果

```c
#include "src/file_scanner/file_scanner.h"

// 扫描后，直接使用全局变量
extern char **image_files;
extern int image_count;

for (int i = 0; i < image_count; i++) {
    printf("图片: %s\n", image_files[i]);
}
```

### 示例3：释放内存

```c
// 在程序退出或重新扫描前释放内存
free_image_arrays();
free_audio_arrays();
free_video_arrays();
```

## 实现细节

### 内存管理

1. **动态扩容**：初始容量为32，当数组满时容量翻倍
2. **内存分配**：每个文件路径和名称都单独分配内存
3. **NULL终止符**：数组末尾添加NULL，便于遍历

### 文件过滤

1. **扩展名检查**：使用 `strcasecmp()` 进行大小写不敏感的扩展名比较
2. **文件类型检查**：使用 `stat()` 检查文件是否为普通文件（`S_ISREG`）
3. **跳过隐藏文件**：自动跳过 `.` 和 `..` 目录项

### 显示名称生成

- 图片：文件名 + " (BMP)" 或 " (GIF)"
- 音频：文件名 + " (音频)"
- 视频：文件名 + " (视频)"

## 注意事项

1. **目录路径**：默认使用 `/mdata` 目录，定义在 `src/common/common.h` 中
2. **内存泄漏**：每次扫描前会自动释放旧数组，但程序退出时建议手动释放
3. **线程安全**：全局变量不是线程安全的，多线程访问需要注意同步
4. **文件数量限制**：理论上没有限制（动态扩容），但受内存限制
5. **路径长度**：文件路径最大长度为512字节（`full_path` 缓冲区大小）

## 相关文件

- `main.c` - 程序启动时调用扫描函数
- `src/common/common.h` - 定义 `IMAGE_DIR` 和 `MEDIA_DIR`
- `src/image_viewer/image_viewer.c` - 使用图片扫描结果
- `src/ui/music_win.c` - 使用音频扫描结果
- `src/ui/video_win.c` - 使用视频扫描结果

