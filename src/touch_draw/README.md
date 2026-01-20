# 触摸绘图模块

## 功能说明

触摸绘图模块提供了基于触摸屏的绘图功能，用户可以通过触摸屏幕来绘制线条。

## 主要特性

- **触摸绘图**：支持触摸屏绘图，使用framebuffer直接绘制
- **实时绘制**：触摸移动时实时绘制线条
- **坐标映射**：自动将触摸坐标映射到屏幕坐标
- **多线程处理**：使用独立线程处理触摸事件，不影响UI响应

## 技术实现

### 核心组件

1. **Framebuffer操作**：直接操作Linux framebuffer设备进行绘制
2. **触摸事件处理**：通过Linux input子系统读取触摸事件
3. **Bresenham算法**：使用Bresenham算法绘制平滑的线条
4. **多线程架构**：独立线程处理触摸事件和绘制操作

### 坐标映射

触摸坐标范围：
- X轴：0 - 1024
- Y轴：0 - 600

屏幕坐标范围：
- X轴：0 - 800
- Y轴：0 - 480

映射公式：
```
screen_x = (touch_x - touch_min_x) * SCREEN_WIDTH / (touch_max_x - touch_min_x)
screen_y = (touch_y - touch_min_y) * SCREEN_HEIGHT / (touch_max_y - touch_min_y)
```

## API接口

### `touch_draw_win_show()`
显示触摸绘图窗口。

### `touch_draw_win_hide()`
隐藏触摸绘图窗口。

### `touch_draw_init()`
初始化触摸绘图模块，启动触摸事件处理线程。

### `touch_draw_cleanup()`
清理触摸绘图模块，停止线程并释放资源。

## 使用方法

1. 从主页第二页点击"触摸绘图"按钮
2. 触摸屏幕开始绘制
3. 移动手指绘制线条
4. 点击"返回"按钮退出

## 注意事项

- 触摸绘图功能会直接操作framebuffer，可能会与LVGL的显示产生冲突
- 退出触摸绘图功能后，framebuffer会被清空并恢复LVGL显示
- 触摸坐标范围可能需要根据实际硬件调整

## 文件结构

```
touch_draw/
├── touch_draw.h      # 头文件
├── touch_draw.c      # 实现文件
└── README.md         # 说明文档
```

## 依赖

- Linux framebuffer设备 (`/dev/fb0`)
- Linux input设备 (`/dev/input/event0`)
- pthread库（多线程支持）
- LVGL图形库（UI界面）
