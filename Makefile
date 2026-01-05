#
# Makefile
#
CC = arm-linux-gnueabihf-gcc
LVGL_DIR_NAME ?= lvgl
LVGL_DIR ?= ${shell pwd}
BUILD_DIR = build
CFLAGS ?= -O3 -g0 -I$(LVGL_DIR)/ -Isrc/ -Wall -Wshadow -Wundef -Wmissing-prototypes -Wno-discarded-qualifiers -Wall -Wextra -Wno-unused-function -Wno-error=strict-prototypes -Wpointer-arith -fno-strict-aliasing -Wno-error=cpp -Wuninitialized -Wmaybe-uninitialized -Wno-unused-parameter -Wno-missing-field-initializers -Wtype-limits -Wsizeof-pointer-memaccess -Wno-format-nonliteral -Wno-cast-qual -Wunreachable-code -Wno-switch-default -Wreturn-type -Wmultichar -Wformat-security -Wno-ignored-qualifiers -Wno-error=pedantic -Wno-sign-compare -Wno-error=missing-prototypes -Wdouble-promotion -Wclobbered -Wdeprecated -Wempty-body -Wtype-limits -Wstack-usage=2048 -Wno-unused-value -Wno-unused-parameter -Wno-missing-field-initializers -Wuninitialized -Wmaybe-uninitialized -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers -Wtype-limits -Wsizeof-pointer-memaccess -Wno-format-nonliteral -Wpointer-arith -Wno-cast-qual -Wmissing-prototypes -Wunreachable-code -Wno-switch-default -Wreturn-type -Wmultichar -Wno-discarded-qualifiers -Wformat-security -Wno-ignored-qualifiers -Wno-sign-compare
# 链接选项（移除FFmpeg库，使用MPlayer + framebuffer播放器）
# 使用静态链接以避免GLIBC版本不匹配问题
LDFLAGS ?= -static -lm -lpthread
BIN = demo


#Collect the files to compile
MAINSRC = ./main.c

include $(LVGL_DIR)/lvgl/lvgl.mk
include $(LVGL_DIR)/lv_drivers/lv_drivers.mk

CSRCS +=$(LVGL_DIR)/mouse_cursor_icon.c 

# Add SourceHanSansSC_VF font file (large font, requires LV_FONT_FMT_TXT_LARGE=1)
CSRCS += bin/SourceHanSansSC_VF.c

# Add src directory source files
CSRCS += src/common/common.c
CSRCS += src/common/touch_device.c
CSRCS += src/hal/hal.c
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
CSRCS += src/ui/clock_win.c
CSRCS += src/ui/game_2048_win.c
CSRCS += src/game_2048/game_2048.c 

OBJEXT ?= .o

# 将所有目标文件路径改为 build 目录
AOBJS = $(patsubst %.S,$(BUILD_DIR)/%.o,$(ASRCS))
COBJS = $(patsubst %.c,$(BUILD_DIR)/%.o,$(CSRCS))

MAINOBJ = $(BUILD_DIR)/$(patsubst ./%,%,$(MAINSRC:.c=$(OBJEXT)))

SRCS = $(ASRCS) $(CSRCS) $(MAINSRC)
OBJS = $(AOBJS) $(COBJS)

## MAINOBJ -> OBJFILES

all: $(BUILD_DIR) default

# 创建 build 目录（包括所有需要的子目录）
$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)
	@mkdir -p $(sort $(dir $(AOBJS) $(COBJS) $(MAINOBJ)))

# 编译规则：将 .c 文件编译到 build 目录
$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	@$(CC) $(CFLAGS) -c $< -o $@
	@echo "CC $< -> $@"

# 编译规则：将 .S 文件编译到 build 目录
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

