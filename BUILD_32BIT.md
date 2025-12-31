# 编译32位程序说明

## 方案一：ARM开发板（推荐）

如果目标设备是ARM开发板，需要使用ARM交叉编译器。

### 1. 在Ubuntu上安装ARM交叉编译工具链

```bash
sudo apt-get update
sudo apt-get install gcc-arm-linux-gnueabihf g++-arm-linux-gnueabihf
```

### 2. 验证安装

```bash
arm-linux-gnueabihf-gcc --version
```

应该看到类似输出：
```
arm-linux-gnueabihf-gcc (Ubuntu 9.4.0-1ubuntu1~20.04.1) 9.4.0
```

### 3. 编译

```bash
make clean
make -j
```

或者显式指定编译器：
```bash
make clean
CC=arm-linux-gnueabihf-gcc make -j
```

### 4. 验证编译结果

```bash
file demo
```

应该显示：
```
demo: ELF 32-bit LSB executable, ARM, EABI5 version 1 (GNU/Linux), statically linked, ...
```

---

## 方案二：x86-64系统上编译32位x86程序

如果需要在x86-64 Ubuntu上编译32位x86程序（而不是ARM），需要：

### 1. 安装32位开发库

```bash
sudo apt-get update
sudo apt-get install gcc-multilib g++-multilib
```

### 2. 修改Makefile

将Makefile中的CC改为：
```makefile
CC := gcc
CFLAGS ?= -m32 -O3 -g0 -I$(LVGL_DIR)/ ...
LDFLAGS ?= -m32 -static -lm
```

### 3. 编译

```bash
make clean
make -j
```

### 4. 验证

```bash
file demo
```

应该显示：
```
demo: ELF 32-bit LSB executable, Intel 80386, version 1 (GNU/Linux), statically linked, ...
```

---

## 当前Makefile配置

当前Makefile已配置为使用ARM交叉编译器（`arm-linux-gnueabihf-gcc`），并添加了编译器检查机制。

如果编译时出现"找不到ARM交叉编译器"的错误，请按照方案一安装ARM交叉编译工具链。

