# HAL (硬件抽象层) 模块文档

## 模块概述

`hal` (Hardware Abstraction Layer) 模块是硬件抽象层，负责初始化LVGL图形库所需的底层硬件接口，包括显示驱动、输入设备、文件系统等。

## 文件结构

- `hal.h` - 硬件抽象层接口定义
- `hal.c` - 硬件抽象层实现

## 主要功能

### 1. 硬件初始化

#### `hal_init()`

初始化硬件抽象层，包括文件系统、显示驱动、输入设备等。

**函数签名：**
```c
void hal_init(void);
```

**功能：**
1. **初始化POSIX文件系统驱动**
   - 调用 `lv_fs_posix_init()` 初始化LVGL的文件系统驱动
   - 驱动器标识符为 `P:`（用于GIF加载等）

2. **初始化Linux Framebuffer显示驱动**
   - 调用 `fbdev_init()` 初始化framebuffer设备
   - 创建显示缓冲区（`DISP_BUF_SIZE = 480 * 800`）
   - 注册显示驱动，设置分辨率为 800x480
   - 设置刷新回调函数 `fbdev_flush`

3. **初始化触摸屏输入设备**
   - 调用 `evdev_init()` 初始化evdev输入设备
   - 注册指针输入设备（触摸屏）
   - 设置读取回调函数 `evdev_read`

**调用位置：**
- `main.c:25` - 程序启动时初始化硬件

**实现细节：**
```c
void hal_init(void) {
    // 1. 初始化POSIX文件系统驱动
    lv_fs_posix_init();
    
    // 2. 初始化framebuffer
    fbdev_init();
    
    // 3. 创建显示缓冲区
    static lv_color_t buf[DISP_BUF_SIZE];
    static lv_disp_draw_buf_t disp_buf;
    lv_disp_draw_buf_init(&disp_buf, buf, NULL, DISP_BUF_SIZE);
    
    // 4. 注册显示驱动
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.draw_buf = &disp_buf;
    disp_drv.flush_cb = fbdev_flush;
    disp_drv.hor_res = 800;
    disp_drv.ver_res = 480;
    lv_disp_drv_register(&disp_drv);
    
    // 5. 初始化触摸屏
    evdev_init();
    static lv_indev_drv_t indev_drv_1;
    lv_indev_drv_init(&indev_drv_1);
    indev_drv_1.type = LV_INDEV_TYPE_POINTER;
    indev_drv_1.read_cb = evdev_read;
    lv_indev_drv_register(&indev_drv_1);
}
```

### 2. 系统时间获取

#### `custom_tick_get()`

获取系统时间戳（毫秒），用于LVGL的定时器系统。

**函数签名：**
```c
uint32_t custom_tick_get(void);
```

**功能：**
- 返回从程序启动开始经过的毫秒数
- 使用 `gettimeofday()` 获取系统时间
- 首次调用时记录启动时间，后续调用返回相对时间

**返回值：**
- 从程序启动开始经过的毫秒数

**调用位置：**
- LVGL内部定时器系统使用（通过 `lv_tick_set_cb()` 设置）

**实现细节：**
```c
uint32_t custom_tick_get(void) {
    static uint64_t start_ms = 0;
    if(start_ms == 0) {
        // 首次调用，记录启动时间
        struct timeval tv_start;
        gettimeofday(&tv_start, NULL);
        start_ms = (tv_start.tv_sec * 1000000 + tv_start.tv_usec) / 1000;
    }
    
    // 获取当前时间
    struct timeval tv_now;
    gettimeofday(&tv_now, NULL);
    uint64_t now_ms = (tv_now.tv_sec * 1000000 + tv_now.tv_usec) / 1000;
    
    // 返回相对时间
    return now_ms - start_ms;
}
```

## 模块调用关系

### 被调用情况

1. **main.c**
   ```c
   lv_init();        // 先初始化LVGL
   hal_init();       // 然后初始化硬件抽象层
   ```

### 依赖关系

- **LVGL库**：`lvgl/lvgl.h`
- **LVGL文件系统驱动**：`lvgl/src/extra/libs/fsdrv/lv_fsdrv.h`
- **LVGL显示驱动**：`lv_drivers/display/fbdev.h`
- **LVGL输入驱动**：`lv_drivers/indev/evdev.h`
- **标准C库**：`stdio.h`, `sys/time.h`

## 硬件配置

### 显示设备

- **设备路径**：`/dev/fb0` (framebuffer设备)
- **分辨率**：800x480
- **缓冲区大小**：480 * 800 = 384,000 像素
- **颜色格式**：由LVGL和framebuffer驱动决定

### 输入设备

- **设备类型**：触摸屏（指针输入设备）
- **驱动**：evdev (Linux输入事件设备)
- **设备路径**：由 `evdev_init()` 自动检测

### 文件系统

- **驱动器标识符**：`P:`
- **类型**：POSIX文件系统
- **用途**：主要用于GIF文件加载

## 使用示例

### 示例1：初始化硬件抽象层

```c
#include "src/hal/hal.h"
#include "lvgl/lvgl.h"

int main(void) {
    // 先初始化LVGL
    lv_init();
    
    // 然后初始化硬件抽象层
    hal_init();
    
    // 现在可以使用LVGL创建UI了
    // ...
}
```

### 示例2：获取系统时间戳

```c
#include "src/hal/hal.h"

uint32_t elapsed_ms = custom_tick_get();
printf("程序运行了 %u 毫秒\n", elapsed_ms);
```

## 实现细节

### 显示驱动初始化流程

1. 调用 `fbdev_init()` 初始化framebuffer设备
2. 创建显示缓冲区（静态分配，避免动态分配问题）
3. 初始化显示缓冲区描述符
4. 初始化显示驱动结构体
5. 设置刷新回调函数
6. 设置分辨率
7. 注册显示驱动

### 输入设备初始化流程

1. 调用 `evdev_init()` 初始化evdev设备
2. 初始化输入设备驱动结构体
3. 设置设备类型为指针设备（触摸屏）
4. 设置读取回调函数
5. 注册输入设备驱动

### 文件系统初始化流程

1. 调用 `lv_fs_posix_init()` 初始化POSIX文件系统驱动
2. 驱动器标识符为 `P:`，可以在LVGL中使用 `P:/path/to/file` 访问文件

## 注意事项

1. **初始化顺序**：必须先调用 `lv_init()`，再调用 `hal_init()`
2. **分辨率固定**：显示分辨率硬编码为 800x480，如需修改需要修改 `hal.c`
3. **缓冲区大小**：显示缓冲区大小固定为 `480 * 800`，需要与分辨率匹配
4. **文件系统驱动器**：POSIX文件系统驱动器标识符为 `P:`，在加载GIF等文件时需要使用 `P:/path/to/file` 格式
5. **触摸屏设备**：触摸屏设备路径由 `evdev_init()` 自动检测，通常为 `/dev/input/event0` 或类似路径

## 相关文件

- `main.c` - 程序入口，调用 `hal_init()`
- `lvgl/` - LVGL图形库
- `lv_drivers/` - LVGL驱动库（framebuffer和evdev）
- `src/common/touch_device.c` - 触摸屏设备管理（独立于HAL的触摸屏管理）

## 与 touch_device 模块的区别

- **hal模块**：初始化LVGL的输入设备驱动，用于LVGL的UI交互
- **touch_device模块**：提供独立的触摸屏设备文件描述符，用于视频播放时的触屏手势控制

两个模块可以同时使用，互不冲突。

