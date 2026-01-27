# Ubuntu虚拟机快速设置指南

## 快速开始

### 1. 安装依赖

```bash
sudo apt update
sudo apt install -y build-essential gcc make libsdl2-dev git
```

### 2. 配置SDL驱动（可选，Makefile已通过-D参数设置）

**方式1（推荐）**: 使用Makefile的默认配置，无需修改文件

**方式2**: 手动编辑 `lv_drv_conf.h`，找到以下部分并修改：

```c
/* SDL based drivers for display, mouse, mousewheel and keyboard*/
#ifndef USE_SDL
# define USE_SDL 1  // 改为1
#endif

#if USE_SDL || USE_SDL_GPU
#  define SDL_HOR_RES     800  // 改为800（匹配GEC6818）
#  define SDL_VER_RES     480  // 改为480（匹配GEC6818）
```

### 3. 编译

```bash
make -f Makefile.ubuntu clean
make -f Makefile.ubuntu
```

### 4. 运行

**完整版本**（包含所有功能）：
```bash
./demo_ubuntu
```

**简化版本**（仅测试协作绘图功能，窗口更大）：
```bash
# 编译时已经使用简化版本（main_collab_test.c）
./demo_ubuntu
```

**注意**：简化版本会：
- 直接显示触摸绘图界面（跳过屏保和密码锁）
- 窗口大小为 1600x960（SDL_ZOOM=2）
- 只包含协作绘图相关功能

## 注意事项

- **触摸绘图功能限制**: 在虚拟机上，触摸绘图功能会尝试访问 `/dev/fb0` 和 `/dev/input/event0`，如果这些设备不存在，触摸绘图功能可能无法正常工作。但UI界面和协作绘图功能（网络部分）可以正常测试。
- **鼠标操作**: 使用鼠标左键点击和拖拽来模拟触摸操作
- **协作绘图测试**: 可以测试协作绘图的网络连接和数据同步功能

## 测试协作绘图

### 方式1：虚拟机 + GEC6818设备

1. 在Ubuntu虚拟机上运行 `./demo_ubuntu`
2. 在GEC6818设备上运行 `./demo`
3. 在虚拟机中点击"连接协作"按钮（主机模式）
4. 在GEC6818设备上点击"加入协作"按钮（客机模式）
5. 测试绘制同步（如果触摸绘图功能可用）

### 方式2：两个虚拟机实例

1. 打开两个终端窗口
2. 分别运行 `./demo_ubuntu`
3. 一个作为主机，一个作为客机
4. 测试网络连接和数据同步

详细说明请参考 `UBUNTU_VM_TEST_GUIDE.md`。
