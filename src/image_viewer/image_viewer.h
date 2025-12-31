/**
 * @file image_viewer.h
 * @brief 图片显示模块头文件
 */

#ifndef IMAGE_VIEWER_H
#define IMAGE_VIEWER_H

#include "lvgl/lvgl.h"
#include "../common/common.h"  // 包含BMP结构体定义

/**
 * @brief 显示图片列表
 */
void show_images(void);

/**
 * @brief 显示当前图片
 */
void show_current_image(void);

/**
 * @brief 加载BMP图片到Canvas
 * @param canvas Canvas对象
 * @param bmp_path BMP文件路径
 * @return 成功返回0，失败返回-1
 */
int load_bmp_to_canvas(lv_obj_t *canvas, const char *bmp_path);

/**
 * @brief 获取当前图片索引
 * @return 当前图片索引
 */
int get_current_image_index(void);

/**
 * @brief 设置当前图片索引
 * @param index 图片索引
 */
void set_current_image_index(int index);

/**
 * @brief 获取图片容器对象
 * @return 图片容器对象
 */
lv_obj_t *get_img_container(void);

/**
 * @brief 获取当前图片对象
 * @return 当前图片对象
 */
lv_obj_t *get_current_img_obj(void);

/**
 * @brief 设置当前图片对象
 * @param obj 图片对象
 */
void set_current_img_obj(lv_obj_t *obj);

/**
 * @brief 获取图片信息标签
 * @return 图片信息标签对象
 */
lv_obj_t *get_img_info_label(void);

/**
 * @brief 设置是否为GIF对象
 * @param is_gif 是否为GIF
 */
void set_is_gif_obj(bool is_gif);

/**
 * @brief 获取是否为GIF对象
 * @return 是否为GIF
 */
bool get_is_gif_obj(void);

/**
 * @brief 获取Canvas缓冲区
 * @return Canvas缓冲区指针
 */
lv_color_t *get_canvas_buf(void);

#endif /* IMAGE_VIEWER_H */

