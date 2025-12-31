/**
 * @file image_viewer.c
 * @brief 图片显示模块实现
 */

#include "image_viewer.h"
#include "../common/common.h"
#include "../file_scanner/file_scanner.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include "lvgl/lvgl.h"
#include "lvgl/src/font/lv_font.h"
#include "lvgl/src/extra/libs/fsdrv/lv_fsdrv.h"

/* 声明SourceHanSansSC_VF字体（定义在bin/SourceHanSansSC_VF.c中） */
#if LV_FONT_SOURCE_HAN_SANS_SC_VF
extern const lv_font_t SourceHanSansSC_VF;
#endif

// 从common.h中获取全局变量
extern lv_obj_t *image_screen;
extern lv_obj_t *current_img_obj;
extern lv_obj_t *img_container;
extern lv_obj_t *img_info_label;
extern int current_img_index;
extern bool is_gif_obj;
extern lv_color_t canvas_buf[680 * 280];

// 从file_scanner.h中获取函数
extern char **image_files;
extern char **image_names;
extern int image_count;

// 前向声明
static void prev_image_cb(lv_event_t * e);
static void next_image_cb(lv_event_t * e);

/**
 * @brief 显示图片列表
 */
void show_images(void) {
    // 如果没有找到图片文件，显示提示信息
    if (image_count == 0 || image_files == NULL) {
        printf("警告: 没有找到任何图片文件\n");
        // 仍然创建UI，但会显示错误信息
    } else {
        // 检查文件是否存在
        for (int i = 0; i < image_count && image_files[i] != NULL; i++) {
            struct stat st;
            if (stat(image_files[i], &st) == 0) {
                printf("验证图片文件[%d]: %s (大小: %ld 字节)\n", i, image_files[i], st.st_size);
            } else {
                printf("警告: 图片文件不存在[%d]: %s - %s\n", i, image_files[i], strerror(errno));
            }
        }
    }
    
    // 创建图片显示容器（缩小尺寸，为按钮留出空间，避免与返回按钮重叠）
    img_container = lv_obj_create(image_screen);
    lv_obj_set_size(img_container, 700, 300);  // 缩小高度
    lv_obj_align(img_container, LV_ALIGN_CENTER, 0, -20);  // 稍微向上移动，避免与返回按钮重叠
    lv_obj_set_style_bg_color(img_container, lv_color_hex(0xffffff), 0);
    lv_obj_set_style_border_width(img_container, 2, 0);
    lv_obj_set_style_border_color(img_container, lv_color_hex(0xcccccc), 0);
    lv_obj_set_style_radius(img_container, 10, 0);
    lv_obj_set_style_pad_all(img_container, 10, 0);
    // 禁用滚动，避免GIF重复显示
    lv_obj_set_scroll_dir(img_container, LV_DIR_NONE);
    lv_obj_clear_flag(img_container, LV_OBJ_FLAG_SCROLLABLE);
    
    // 创建canvas对象用于显示图片
    current_img_obj = lv_canvas_create(img_container);
    lv_obj_set_size(current_img_obj, 680, 280);
    lv_obj_align(current_img_obj, LV_ALIGN_CENTER, 0, 0);
    is_gif_obj = false;
    
    // 为canvas分配缓冲区（ARGB8888格式，32位）
    lv_canvas_set_buffer(current_img_obj, canvas_buf, 680, 280, LV_IMG_CF_TRUE_COLOR_ALPHA);
    lv_canvas_fill_bg(current_img_obj, lv_color_hex(0xFFFFFF), LV_OPA_COVER);
    
    // 创建切换按钮容器（包含按钮和信息标签）
    lv_obj_t *btn_container = lv_obj_create(image_screen);
    lv_obj_set_size(btn_container, 750, 100);
    lv_obj_align(btn_container, LV_ALIGN_BOTTOM_MID, 0, -20);
    lv_obj_set_style_bg_color(btn_container, lv_color_hex(0xf0f0f0), 0);
    lv_obj_set_style_border_width(btn_container, 0, 0);
    lv_obj_set_flex_flow(btn_container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btn_container, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(btn_container, 10, 0);
    lv_obj_set_style_pad_gap(btn_container, 15, 0);
    
    // 上一张按钮
    lv_obj_t *prev_btn = lv_btn_create(btn_container);
    lv_obj_set_size(prev_btn, 120, 60);
    lv_obj_set_style_bg_color(prev_btn, lv_color_hex(0x2196F3), 0);
    lv_obj_t *prev_label = lv_label_create(prev_btn);
    lv_label_set_text(prev_label, "上一张 <<");
    lv_obj_set_style_text_font(prev_label, &SourceHanSansSC_VF, 0);
    lv_obj_center(prev_label);
    lv_obj_add_event_cb(prev_btn, prev_image_cb, LV_EVENT_CLICKED, NULL);
    
    // 创建图片信息标签（放在两个按钮之间）
    img_info_label = lv_label_create(btn_container);
    lv_obj_set_style_text_font(img_info_label, &SourceHanSansSC_VF, 0);
    lv_obj_set_style_text_color(img_info_label, lv_color_hex(0x1a1a1a), 0);
    lv_label_set_text(img_info_label, "加载中...");
    lv_obj_set_style_text_align(img_info_label, LV_TEXT_ALIGN_CENTER, 0);
    
    // 下一张按钮
    lv_obj_t *next_btn = lv_btn_create(btn_container);
    lv_obj_set_size(next_btn, 120, 60);
    lv_obj_set_style_bg_color(next_btn, lv_color_hex(0x4CAF50), 0);
    lv_obj_t *next_label = lv_label_create(next_btn);
    lv_label_set_text(next_label, "下一张 >>");
    lv_obj_set_style_text_font(next_label, &SourceHanSansSC_VF, 0);
    lv_obj_center(next_label);
    lv_obj_add_event_cb(next_btn, next_image_cb, LV_EVENT_CLICKED, NULL);
    
    // 显示当前图片
    current_img_index = 0;
    show_current_image();
}

/**
 * @brief 显示当前图片
 */
void show_current_image(void) {
    if (current_img_obj == NULL || image_count == 0 || image_files == NULL) {
        if (img_info_label) {
            lv_label_set_text(img_info_label, "没有找到图片文件");
        }
        return;
    }
    
    if (current_img_index < 0 || current_img_index >= image_count || image_files[current_img_index] == NULL) {
        if (img_info_label) {
            lv_label_set_text(img_info_label, "图片索引无效");
        }
        return;
    }
    
    // 检查文件是否存在
    struct stat st;
    const char *file_path = image_files[current_img_index];
    if (stat(file_path, &st) != 0) {
        printf("错误: 图片文件不存在: %s\n", file_path);
        if (current_img_obj != NULL && !is_gif_obj) {
            lv_canvas_fill_bg(current_img_obj, lv_color_hex(0xFFFFFF), LV_OPA_COVER);
        }
        if (img_info_label) {
            char info[100];
            snprintf(info, sizeof(info), "文件不存在: %s", file_path);
            lv_label_set_text(img_info_label, info);
        }
        return;
    }
    
    printf("加载图片[%d]: %s\n", current_img_index, file_path);
    
    // 判断文件类型并加载
    const char *ext = strrchr(file_path, '.');
    bool need_gif = (ext != NULL && strcmp(ext, ".gif") == 0);
    
    // 总是删除旧对象并重新创建，确保GIF正确显示
    if (current_img_obj != NULL) {
        lv_obj_del(current_img_obj);
        current_img_obj = NULL;
        is_gif_obj = false;
        
        lv_obj_invalidate(img_container);
        
        // 多次处理定时器，确保删除操作和资源清理完成
        for (int i = 0; i < 3; i++) {
            lv_timer_handler();
            usleep(1000);
        }
        
        // 清空canvas缓冲区
        memset(canvas_buf, 0, sizeof(canvas_buf));
    }
    
    // 创建新对象
    if (need_gif) {
        // GIF显示方案：使用白色背景容器，GIF按实际尺寸居中显示
        current_img_obj = lv_obj_create(img_container);
        if (current_img_obj == NULL) {
            printf("错误: 无法创建容器对象\n");
            return;
        }
        lv_obj_set_size(current_img_obj, 680, 280);
        lv_obj_align(current_img_obj, LV_ALIGN_CENTER, 0, 0);
        lv_obj_set_style_bg_color(current_img_obj, lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_bg_opa(current_img_obj, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(current_img_obj, 0, 0);
        lv_obj_set_style_pad_all(current_img_obj, 0, 0);
        lv_obj_set_scroll_dir(current_img_obj, LV_DIR_NONE);
        lv_obj_clear_flag(current_img_obj, LV_OBJ_FLAG_SCROLLABLE);
        is_gif_obj = false;
        
        // 在容器中创建GIF对象
        lv_obj_t *gif_obj = lv_gif_create(current_img_obj);
        if (gif_obj == NULL) {
            printf("错误: 无法创建GIF对象\n");
            return;
        }
        
        // 使用POSIX文件系统驱动器（P:）加载GIF
        char lvgl_path[256];
        snprintf(lvgl_path, sizeof(lvgl_path), "P:%s", file_path);
        
        printf("尝试加载GIF: %s (LVGL路径: %s)\n", file_path, lvgl_path);
        
        // 设置GIF源（会自动开始播放）
        lv_gif_set_src(gif_obj, lvgl_path);
        
        // 等待GIF加载完成
        for (int i = 0; i < 5; i++) {
            lv_timer_handler();
            usleep(10000);
        }
        
        // 获取GIF的实际尺寸
        const void *src = lv_img_get_src(gif_obj);
        if (src != NULL) {
            const lv_img_dsc_t *gif_dsc = (const lv_img_dsc_t *)src;
            if (gif_dsc->header.w > 0 && gif_dsc->header.h > 0) {
                int gif_width = gif_dsc->header.w;
                int gif_height = gif_dsc->header.h;
                
                printf("GIF实际尺寸: %dx%d\n", gif_width, gif_height);
                
                int display_width = gif_width;
                int display_height = gif_height;
                
                if (display_width > 680) display_width = 680;
                if (display_height > 280) display_height = 280;
                
                lv_img_set_size_mode(gif_obj, LV_IMG_SIZE_MODE_REAL);
                lv_obj_set_size(gif_obj, display_width, display_height);
                lv_obj_align(gif_obj, LV_ALIGN_CENTER, 0, 0);
                lv_img_set_zoom(gif_obj, 256);
                
                printf("GIF显示尺寸: %dx%d (实际: %dx%d, 容器: 680x280)\n", 
                       display_width, display_height, gif_width, gif_height);
            } else {
                lv_img_set_size_mode(gif_obj, LV_IMG_SIZE_MODE_REAL);
                lv_obj_set_size(gif_obj, 680, 280);
                lv_obj_align(gif_obj, LV_ALIGN_CENTER, 0, 0);
                lv_img_set_zoom(gif_obj, 256);
                printf("警告: GIF尺寸无效，使用默认尺寸 680x280\n");
            }
        } else {
            lv_img_set_size_mode(gif_obj, LV_IMG_SIZE_MODE_REAL);
            lv_obj_set_size(gif_obj, 680, 280);
            lv_obj_align(gif_obj, LV_ALIGN_CENTER, 0, 0);
            lv_img_set_zoom(gif_obj, 256);
            printf("警告: 无法获取GIF源，使用默认尺寸 680x280\n");
        }
        
        printf("GIF图片加载完成: %s\n", file_path);
    } else {
        // BMP使用canvas手动绘制
        memset(canvas_buf, 0, sizeof(canvas_buf));
        
        current_img_obj = lv_canvas_create(img_container);
        if (current_img_obj == NULL) {
            printf("错误: 无法创建canvas对象\n");
            return;
        }
        
        lv_obj_set_size(current_img_obj, 680, 280);
        lv_obj_align(current_img_obj, LV_ALIGN_CENTER, 0, 0);
        is_gif_obj = false;
        
        // 为canvas分配缓冲区
        lv_canvas_set_buffer(current_img_obj, canvas_buf, 680, 280, LV_IMG_CF_TRUE_COLOR_ALPHA);
        lv_canvas_fill_bg(current_img_obj, lv_color_hex(0xFFFFFF), LV_OPA_COVER);
        
        // 加载BMP到canvas
        if (load_bmp_to_canvas(current_img_obj, file_path) != 0) {
            printf("BMP图片加载失败: %s\n", file_path);
            lv_canvas_fill_bg(current_img_obj, lv_color_hex(0xFFFFFF), LV_OPA_COVER);
        } else {
            printf("BMP图片加载成功: %s\n", file_path);
        }
    }
    
    // 更新信息标签
    if (img_info_label) {
        char info[200];
        if (image_names != NULL && image_names[current_img_index] != NULL) {
            snprintf(info, sizeof(info), "%s (%d/%d)\n%s", 
                     image_names[current_img_index], 
                     current_img_index + 1, 
                     image_count,
                     file_path);
        } else {
            snprintf(info, sizeof(info), "图片 %d/%d\n%s", 
                     current_img_index + 1, 
                     image_count,
                     file_path);
        }
        lv_label_set_text(img_info_label, info);
    }
}

/**
 * @brief 加载BMP图片到Canvas
 */
int load_bmp_to_canvas(lv_obj_t *canvas, const char *bmp_path) {
    if (canvas == NULL || bmp_path == NULL) {
        return -1;
    }
    
    // 打开BMP文件
    int bmp_fd = open(bmp_path, O_RDONLY);
    if (bmp_fd == -1) {
        printf("无法打开图片: %s\n", bmp_path);
        perror("详细原因");
        return -1;
    }
    
    // 读取文件头
    struct bmp_header header;
    if (read(bmp_fd, &header, sizeof(header)) != sizeof(header)) {
        printf("读取BMP头失败: %s\n", bmp_path);
        perror("详细原因");
        close(bmp_fd);
        return -1;
    }
    
    // 验证签名
    if (header.signature != 0x4D42) {
        printf("无效的BMP文件签名: 0x%X\n", header.signature);
        close(bmp_fd);
        return -1;
    }
    
    // 读取信息头
    struct bmp_info_header info_header;
    if (read(bmp_fd, &info_header, sizeof(info_header)) != sizeof(info_header)) {
        printf("读取BMP信息头失败\n");
        close(bmp_fd);
        return -1;
    }
    
    // 格式验证
    if (info_header.bpp != 24) {
        printf("只支持24位BMP图片 (当前: %d bpp)\n", info_header.bpp);
        close(bmp_fd);
        return -1;
    }
    
    if (info_header.compression != 0) {
        printf("不支持压缩的BMP图片\n");
        close(bmp_fd);
        return -1;
    }
    
    // 定位像素数据
    lseek(bmp_fd, header.data_offset, SEEK_SET);
    
    int img_width = info_header.width;
    int img_height = abs(info_header.height);
    int row_size = ((img_width * 3 + 3) / 4) * 4;  // BMP行对齐到4字节
    
    printf("BMP信息: %dx%d, bpp=%d, row_size=%d\n", img_width, img_height, info_header.bpp, row_size);
    
    // 读取像素数据
    unsigned char *bmp_buf = (unsigned char *)malloc(row_size * img_height);
    if (!bmp_buf) {
        printf("内存分配失败\n");
        close(bmp_fd);
        return -1;
    }
    
    ssize_t bytes_read = read(bmp_fd, bmp_buf, row_size * img_height);
    close(bmp_fd);
    
    if (bytes_read != (ssize_t)(row_size * img_height)) {
        printf("图片读取不完整: %zd/%d 字节\n", bytes_read, row_size * img_height);
        free(bmp_buf);
        return -1;
    }
    
    // 清空canvas
    lv_canvas_fill_bg(canvas, lv_color_hex(0xFFFFFF), LV_OPA_COVER);
    
    // 获取canvas尺寸
    lv_img_dsc_t *canvas_dsc = lv_canvas_get_img(canvas);
    int canvas_width = canvas_dsc->header.w;
    int canvas_height = canvas_dsc->header.h;
    
    // 计算缩放和居中位置
    float scale_x = (float)canvas_width / img_width;
    float scale_y = (float)canvas_height / img_height;
    float scale = (scale_x < scale_y) ? scale_x : scale_y;  // 保持宽高比
    
    int scaled_width = (int)(img_width * scale);
    int scaled_height = (int)(img_height * scale);
    int x_offset = (canvas_width - scaled_width) / 2;
    int y_offset = (canvas_height - scaled_height) / 2;
    
    printf("缩放: %.2f, 显示尺寸: %dx%d, 偏移: (%d, %d)\n", 
           (double)scale, scaled_width, scaled_height, x_offset, y_offset);
    
    // 转换并绘制像素
    for (int y = 0; y < scaled_height; y++) {
        for (int x = 0; x < scaled_width; x++) {
            // 计算源图片坐标
            int src_x = (int)(x / scale);
            int src_y = (int)(y / scale);
            
            // BMP数据是从下往上存储的
            int bmp_y = info_header.height > 0 ? (img_height - src_y - 1) : src_y;
            int src_pos = bmp_y * row_size + src_x * 3;
            
            if (src_pos < 0 || src_pos >= row_size * img_height) {
                continue;
            }
            
            // 读取BGR数据（BMP格式是BGR）
            uint8_t b = bmp_buf[src_pos];
            uint8_t g = bmp_buf[src_pos + 1];
            uint8_t r = bmp_buf[src_pos + 2];
            
            // 转换为LVGL颜色格式（ARGB8888）
            lv_color_t color = lv_color_make(r, g, b);
            
            // 绘制到canvas
            int canvas_x = x_offset + x;
            int canvas_y = y_offset + y;
            
            if (canvas_x >= 0 && canvas_x < canvas_width && 
                canvas_y >= 0 && canvas_y < canvas_height) {
                lv_canvas_set_px_color(canvas, canvas_x, canvas_y, color);
            }
        }
    }
    
    free(bmp_buf);
    
    // 刷新canvas显示
    lv_obj_invalidate(canvas);
    
    return 0;
}

/**
 * @brief 上一张图片回调
 */
static void prev_image_cb(lv_event_t * e) {
    (void)e;
    
    if (image_count == 0 || image_files == NULL) return;
    
    current_img_index--;
    if (current_img_index < 0) {
        current_img_index = image_count - 1;  // 循环到最后一张
    }
    
    printf("切换到上一张图片，索引: %d\n", current_img_index);
    show_current_image();
}

/**
 * @brief 下一张图片回调
 */
static void next_image_cb(lv_event_t * e) {
    (void)e;
    
    if (image_count == 0 || image_files == NULL) return;
    
    current_img_index++;
    if (current_img_index >= image_count) {
        current_img_index = 0;  // 循环到第一张
    }
    
    printf("切换到下一张图片，索引: %d\n", current_img_index);
    show_current_image();
}

// Getter/Setter函数实现
int get_current_image_index(void) {
    return current_img_index;
}

void set_current_image_index(int index) {
    current_img_index = index;
}

lv_obj_t *get_img_container(void) {
    return img_container;
}

lv_obj_t *get_current_img_obj(void) {
    return current_img_obj;
}

void set_current_img_obj(lv_obj_t *obj) {
    current_img_obj = obj;
}

lv_obj_t *get_img_info_label(void) {
    return img_info_label;
}

void set_is_gif_obj(bool is_gif) {
    is_gif_obj = is_gif;
}

bool get_is_gif_obj(void) {
    return is_gif_obj;
}

lv_color_t *get_canvas_buf(void) {
    return canvas_buf;
}

