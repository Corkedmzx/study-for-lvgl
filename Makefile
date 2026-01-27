#
# Makefile for Ubuntu Virtual Machine (x86_64)
# 使用SDL驱动进行显示和输入
#
CC = gcc
LVGL_DIR_NAME ?= lvgl
LVGL_DIR ?= ${shell pwd}
BUILD_DIR = build_ubuntu

# 编译选项
# 注意：需要在lv_drv_conf.h中设置USE_SDL=1和SDL分辨率（800x480）
CFLAGS ?= -O2 -g -I$(LVGL_DIR)/ -Isrc/ -DUSE_SDL=1 -DSDL_HOR_RES=800 -DSDL_VER_RES=480 -Wall -Wshadow -Wundef -Wmissing-prototypes -Wno-discarded-qualifiers -Wall -Wextra -Wno-unused-function -Wno-error=strict-prototypes -Wpointer-arith -fno-strict-aliasing -Wno-error=cpp -Wuninitialized -Wmaybe-uninitialized -Wno-unused-parameter -Wno-missing-field-initializers -Wtype-limits -Wsizeof-pointer-memaccess -Wno-format-nonliteral -Wno-cast-qual -Wunreachable-code -Wno-switch-default -Wreturn-type -Wmultichar -Wformat-security -Wno-ignored-qualifiers -Wno-error=pedantic -Wno-sign-compare -Wno-error=missing-prototypes -Wdouble-promotion -Wclobbered -Wdeprecated -Wempty-body -Wtype-limits -Wstack-usage=2048 -Wno-unused-value -Wno-unused-parameter -Wno-missing-field-initializers -Wuninitialized -Wmaybe-uninitialized -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers -Wtype-limits -Wsizeof-pointer-memaccess -Wno-format-nonliteral -Wpointer-arith -Wno-cast-qual -Wmissing-prototypes -Wunreachable-code -Wno-switch-default -Wreturn-type -Wmultichar -Wno-discarded-qualifiers -Wformat-security -Wno-ignored-qualifiers -Wno-sign-compare

# 链接选项（Ubuntu虚拟机版本，使用SDL）
LDFLAGS ?= -lm -lpthread -lSDL2
BIN = demo_ubuntu


#Collect the files to compile
# 使用简化的测试程序（仅测试协作绘图功能）
MAINSRC = ./main_collab_test.c

include $(LVGL_DIR)/lvgl/lvgl.mk
include $(LVGL_DIR)/lv_drivers/lv_drivers.mk

CSRCS +=$(LVGL_DIR)/mouse_cursor_icon.c 

# Add SourceHanSansSC_VF font file (large font, requires LV_FONT_FMT_TXT_LARGE=1)
CSRCS += bin/SourceHanSansSC_VF.c

# Add FontAwesome solid font file (for icons)
CSRCS += bin/FA-solid-900.c

# 排除hal.c，只使用hal_sdl.c（Ubuntu虚拟机版本）
CSRCS := $(filter-out src/hal/hal.c,$(CSRCS))

# 排除sdl_gpu.c，只使用sdl.c（除非需要GPU加速）
CSRCS := $(filter-out %/sdl/sdl_gpu.c,$(CSRCS))

# Add src directory source files
CSRCS += src/common/common.c
CSRCS += src/common/touch_device.c
CSRCS += src/hal/hal_sdl.c  # 使用SDL版本的HAL
CSRCS += src/file_scanner/file_scanner.c
CSRCS += src/image_viewer/image_viewer.c
CSRCS += src/media_player/simple_video_player.c
CSRCS += src/media_player/audio_player.c
CSRCS += src/weather/weather.c
CSRCS += src/time_sync/time_sync.c
CSRCS += src/ui/ui_screens.c
CSRCS += src/ui/ui_callbacks.c
CSRCS += src/ui/album_win.c
CSRCS += src/ui/music_win.c
CSRCS += src/ui/video_win.c
CSRCS += src/ui/video_touch_control.c
CSRCS += src/ui/led_win.c
CSRCS += src/ui/weather_win.c
CSRCS += src/ui/exit_win.c 
CSRCS += src/ui/login_win.c
CSRCS += src/ui/timer_win.c
CSRCS += src/ui/screensaver_win.c
CSRCS += src/ui/clock_win.c
CSRCS += src/ui/game_2048_win.c
CSRCS += src/game_2048/game_2048.c
CSRCS += src/touch_draw/touch_draw.c 
CSRCS += src/collaborative_draw/draw_protocol.c
CSRCS += src/collaborative_draw/bemfa_tcp_client.c
CSRCS += src/collaborative_draw/collaborative_draw.c 

OBJEXT ?= .o

# 将所有目标文件路径改为 build_ubuntu 目录
AOBJS = $(patsubst %.S,$(BUILD_DIR)/%.o,$(ASRCS))
COBJS = $(patsubst %.c,$(BUILD_DIR)/%.o,$(CSRCS))

MAINOBJ = $(BUILD_DIR)/$(patsubst ./%,%,$(MAINSRC:.c=$(OBJEXT)))

SRCS = $(ASRCS) $(CSRCS) $(MAINSRC)
OBJS = $(AOBJS) $(COBJS)

## MAINOBJ -> OBJFILES

all: $(BUILD_DIR) default

# 创建 build_ubuntu 目录（包括所有需要的子目录）
$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)
	@mkdir -p $(sort $(dir $(AOBJS) $(COBJS) $(MAINOBJ)))

# 编译规则：将 .c 文件编译到 build_ubuntu 目录
$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	@$(CC) $(CFLAGS) -c $< -o $@
	@echo "CC $< -> $@"

# 编译规则：将 .S 文件编译到 build_ubuntu 目录
$(BUILD_DIR)/%.o: %.S
	@mkdir -p $(dir $@)
	@$(CC) $(CFLAGS) -c $< -o $@
	@echo "AS $< -> $@"
    
default: $(AOBJS) $(COBJS) $(MAINOBJ)
	$(CC) -o $(BIN) $(MAINOBJ) $(AOBJS) $(COBJS) $(LDFLAGS)
	@echo "LINK $(BIN)"

clean: 
	rm -f $(BIN)
	rm -rf $(BUILD_DIR)
