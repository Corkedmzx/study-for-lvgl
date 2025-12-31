/**
 * @file file_scanner.h
 * @brief 文件扫描模块头文件
 */

#ifndef FILE_SCANNER_H
#define FILE_SCANNER_H

/**
 * @brief 扫描指定目录中的图片文件（BMP和GIF）
 * @param dir_path 目录路径
 * @return 找到的文件数量，失败返回-1
 */
int scan_image_directory(const char *dir_path);

/**
 * @brief 扫描指定目录中的音频文件
 * @param dir_path 目录路径
 * @return 找到的文件数量，失败返回-1
 */
int scan_audio_directory(const char *dir_path);

/**
 * @brief 扫描指定目录中的视频文件
 * @param dir_path 目录路径
 * @return 找到的文件数量，失败返回-1
 */
int scan_video_directory(const char *dir_path);

/**
 * @brief 释放图片文件数组内存
 */
void free_image_arrays(void);

/**
 * @brief 释放音频文件数组内存
 */
void free_audio_arrays(void);

/**
 * @brief 释放视频文件数组内存
 */
void free_video_arrays(void);

/**
 * @brief 获取图片文件路径数组
 * @return 图片文件路径数组
 */
char **get_image_files(void);

/**
 * @brief 获取图片名称数组
 * @return 图片名称数组
 */
char **get_image_names(void);

/**
 * @brief 获取图片文件数量
 * @return 图片文件数量
 */
int get_image_count(void);

/**
 * @brief 获取音频文件路径数组
 * @return 音频文件路径数组
 */
char **get_audio_files(void);

/**
 * @brief 获取音频名称数组
 * @return 音频名称数组
 */
char **get_audio_names(void);

/**
 * @brief 获取音频文件数量
 * @return 音频文件数量
 */
int get_audio_count(void);

/**
 * @brief 获取视频文件路径数组
 * @return 视频文件路径数组
 */
char **get_video_files(void);

/**
 * @brief 获取视频名称数组
 * @return 视频名称数组
 */
char **get_video_names(void);

/**
 * @brief 获取视频文件数量
 * @return 视频文件数量
 */
int get_video_count(void);

#endif /* FILE_SCANNER_H */

