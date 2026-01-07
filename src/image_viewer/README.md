# Image Viewer 模块文档

## 模块概述

`image_viewer` 模块负责图片的显示和浏览功能，支持BMP和GIF格式的图片。该模块提供了图片列表显示、图片切换、BMP图片加载等功能。

## 文件结构

- `image_viewer.h` - 模块接口定义
- `image_viewer.c` - 模块实现

## 主要功能

### 1. 图片列表显示

#### `show_images()`

显示图片列表，创建图片显示UI。

**函数签名：**
```c
void show_images(void);
```

**功能：**
- 创建图片显示容器（700x300）
- 创建Canvas对象用于显示图片（680x280）
- 创建切换按钮（上一张、下一张）
- 创建图片信息标签
- 显示当前图片（默认第一张）

**调用位置：**
- `src/ui/album_win.c:102` - 进入相册窗口时调用

**实现细节：**
1. 检查是否有图片文件（使用全局变量 `image_count` 和 `image_files`）
2. 验证文件是否存在
3. 创建图片容器和Canvas对象
4. 创建按钮容器和切换按钮
5. 创建图片信息标签
6. 调用 `show_current_image()` 显示第一张图片

### 2. 显示当前图片

#### `show_current_image()`

显示当前索引的图片。

**函数签名：**
```c
void show_current_image(void);
```

**功能：**
- 根据当前索引加载并显示图片
- 支持BMP和GIF两种格式
- BMP使用Canvas手动绘制
- GIF使用LVGL的GIF对象自动播放
- 更新图片信息标签

**调用位置：**
- `show_images()` - 初始化时显示第一张
- `prev_image_cb()` - 切换到上一张时
- `next_image_cb()` - 切换到下一张时
- `src/ui/album_win.c` - 相册窗口切换图片时

**实现细节：**
1. 检查索引有效性
2. 检查文件是否存在
3. 判断文件类型（BMP或GIF）
4. 删除旧图片对象
5. 根据类型创建新对象：
   - **GIF**：创建容器和GIF对象，使用POSIX文件系统加载
   - **BMP**：创建Canvas对象，调用 `load_bmp_to_canvas()` 加载
6. 更新图片信息标签

### 3. BMP图片加载

#### `load_bmp_to_canvas()`

加载BMP图片到Canvas对象。

**函数签名：**
```c
int load_bmp_to_canvas(lv_obj_t *canvas, const char *bmp_path);
```

**功能：**
- 读取BMP文件头和信息头
- 验证BMP格式（只支持24位未压缩BMP）
- 读取像素数据
- 转换为LVGL颜色格式
- 缩放并居中显示在Canvas上

**参数：**
- `canvas` - Canvas对象
- `bmp_path` - BMP文件路径

**返回值：**
- 成功返回0，失败返回-1

**支持的BMP格式：**
- 24位色深
- 未压缩（compression = 0）
- 支持正向和反向存储（通过height符号判断）

**实现细节：**
1. 打开BMP文件
2. 读取文件头，验证签名（0x4D42）
3. 读取信息头，验证格式
4. 定位到像素数据偏移位置
5. 读取像素数据（BGR格式）
6. 计算缩放比例（保持宽高比）
7. 转换BGR到RGB，绘制到Canvas
8. 刷新Canvas显示

### 4. 图片切换

#### `prev_image_cb()`

切换到上一张图片的回调函数。

**函数签名：**
```c
static void prev_image_cb(lv_event_t * e);
```

**功能：**
- 将当前索引减1
- 如果索引小于0，循环到最后一张
- 调用 `show_current_image()` 显示新图片

#### `next_image_cb()`

切换到下一张图片的回调函数。

**函数签名：**
```c
static void next_image_cb(lv_event_t * e);
```

**功能：**
- 将当前索引加1
- 如果索引超出范围，循环到第一张
- 调用 `show_current_image()` 显示新图片

### 5. Getter/Setter函数

提供了一系列getter/setter函数，用于访问和修改模块内部状态：

- `get_current_image_index()` - 获取当前图片索引
- `set_current_image_index(int index)` - 设置当前图片索引
- `get_img_container()` - 获取图片容器对象
- `get_current_img_obj()` - 获取当前图片对象
- `set_current_img_obj(lv_obj_t *obj)` - 设置当前图片对象
- `get_img_info_label()` - 获取图片信息标签
- `set_is_gif_obj(bool is_gif)` - 设置是否为GIF对象
- `get_is_gif_obj()` - 获取是否为GIF对象
- `get_canvas_buf()` - 获取Canvas缓冲区

## 模块调用关系

### 被调用情况

1. **src/ui/album_win.c**
   ```c
   show_images();              // 进入相册时显示图片列表
   show_current_image();       // 切换图片时显示
   ```

2. **src/ui/ui_screens.c**
   - 使用全局变量：`image_screen`, `current_img_obj`, `img_container`, `img_info_label`

### 依赖关系

- **common模块**：使用 `common.h` 中的全局变量和BMP结构体定义
- **file_scanner模块**：使用 `image_files`, `image_names`, `image_count` 全局变量
- **LVGL库**：使用LVGL的Canvas、GIF、Label等组件
- **LVGL文件系统驱动**：使用POSIX文件系统驱动器加载GIF

## 使用示例

### 示例1：显示图片列表

```c
#include "src/image_viewer/image_viewer.h"

// 先扫描图片目录（如果未扫描）
extern int scan_image_directory(const char *);
scan_image_directory("/mdata");

// 显示图片列表
show_images();
```

### 示例2：切换到下一张图片

```c
#include "src/image_viewer/image_viewer.h"

int current = get_current_image_index();
set_current_image_index(current + 1);
show_current_image();
```

### 示例3：加载BMP图片到Canvas

```c
#include "src/image_viewer/image_viewer.h"
#include "lvgl/lvgl.h"

lv_obj_t *canvas = lv_canvas_create(parent);
lv_obj_set_size(canvas, 680, 280);

if (load_bmp_to_canvas(canvas, "/mdata/image.bmp") == 0) {
    printf("BMP图片加载成功\n");
} else {
    printf("BMP图片加载失败\n");
}
```

## 实现细节

### BMP图片处理

1. **文件格式验证**：
   - 检查文件签名（0x4D42）
   - 验证色深（必须为24位）
   - 验证压缩方式（必须为未压缩）

2. **像素数据读取**：
   - BMP数据从下往上存储
   - 每行对齐到4字节边界
   - BGR格式（需要转换为RGB）

3. **缩放算法**：
   - 计算宽高缩放比例
   - 选择较小的比例（保持宽高比）
   - 居中显示在Canvas上

### GIF图片处理

1. **使用LVGL GIF对象**：
   - 创建 `lv_gif_create()` 对象
   - 使用POSIX文件系统路径（`P:/path/to/file.gif`）
   - LVGL自动处理GIF动画播放

2. **尺寸处理**：
   - 获取GIF实际尺寸
   - 如果超过Canvas尺寸，进行缩放
   - 居中显示

### Canvas缓冲区

- 缓冲区大小：`680 * 280 = 190,400` 像素
- 颜色格式：`LV_IMG_CF_TRUE_COLOR_ALPHA` (ARGB8888, 32位)
- 缓冲区定义在 `common.h` 中：`lv_color_t canvas_buf[680 * 280]`

## 注意事项

1. **文件路径**：图片文件路径来自 `file_scanner` 模块扫描的结果
2. **内存管理**：Canvas缓冲区是静态分配的，不需要手动释放
3. **GIF加载**：GIF文件必须使用POSIX文件系统路径（`P:/path/to/file.gif`）
4. **BMP格式限制**：只支持24位未压缩BMP，其他格式会加载失败
5. **图片切换**：切换图片时会删除旧对象并创建新对象，确保GIF正确显示
6. **线程安全**：模块不是线程安全的，应在主线程中调用

## 相关文件

- `src/common/common.h` - 定义全局变量和BMP结构体
- `src/file_scanner/file_scanner.c` - 提供图片文件列表
- `src/ui/album_win.c` - 相册窗口，调用图片查看器功能
- `src/ui/ui_screens.c` - 创建图片展示屏幕

