#include "weather.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <time.h>
#include <sys/types.h>

// 使用 wttr.in 免费天气API（知名、可靠、无需API key）
// 使用域名（虽然静态链接会有警告，但运行时通常能正常工作）
static const char *api_servers[] = {
    "wttr.in",           // 主服务器
    NULL
};

// 提取JSON字段值（支持字符串和数字类型）
static char* extract_json_value(char *json_str, const char *key) {
    char pattern[64];
    snprintf(pattern, sizeof(pattern), "\"%s\":", key);
    
    char *p = strstr(json_str, pattern);
    if (!p) return NULL;
    
    p += strlen(pattern);
    
    // 跳过空格
    while (*p == ' ' || *p == '\t') p++;
    
    // 处理字符串值（用引号包围）
    if (*p == '"') {
        p++; // 跳过开头的引号
    char *end = strchr(p, '"');
    if (!end) return NULL;
    
    size_t len = end - p;
    char *value = malloc(len + 1);
    if (!value) return NULL;
    
    strncpy(value, p, len);
    value[len] = '\0';
    
    return value;
    }
    // 处理数字值（整数或小数）
    else if ((*p >= '0' && *p <= '9') || *p == '-') {
        char *start = p;
        // 找到数字的结束位置（逗号、空格、}、]等）
        while ((*p >= '0' && *p <= '9') || *p == '.' || *p == '-' || *p == 'e' || *p == 'E' || *p == '+') {
            p++;
        }
        
        size_t len = p - start;
        char *value = malloc(len + 1);
        if (!value) return NULL;
        
        strncpy(value, start, len);
        value[len] = '\0';
        
        return value;
    }
    
    return NULL;
}

// 获取多天天气数据
char* get_weather_data(void) {
    int tcp_socket = -1;
    char *response = NULL;
    
    // 尝试连接各服务器
    for (int i = 0; api_servers[i] != NULL; i++) {
        tcp_socket = socket(AF_INET, SOCK_STREAM, 0);
        if (tcp_socket < 0) continue;

        // 使用 getaddrinfo 进行域名解析
        // 注意：静态链接时会有警告，但运行时通常能正常工作
        struct addrinfo hints = {0};
        struct addrinfo *result = NULL;
        hints.ai_family = AF_INET;      // 只使用 IPv4
        hints.ai_socktype = SOCK_STREAM;
        
        int ret = getaddrinfo(api_servers[i], "80", &hints, &result);
        if (ret != 0 || !result) {
            printf("[天气] DNS解析失败: %s (错误: %d)\n", api_servers[i], ret);
            continue;
        }
        
        // 使用第一个解析结果
        struct sockaddr_in *addr = (struct sockaddr_in *)result->ai_addr;

        // 设置超时（增加到10秒，因为网络可能较慢）
        struct timeval timeout = {.tv_sec = 10, .tv_usec = 0};
        setsockopt(tcp_socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
        setsockopt(tcp_socket, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

        printf("[天气] 尝试连接: %s\n", api_servers[i]);
        if (connect(tcp_socket, (struct sockaddr *)addr, result->ai_addrlen) < 0) {
            printf("[天气] 连接失败: %s (错误: %s, errno: %d)\n", 
                   api_servers[i], strerror(errno), errno);
            freeaddrinfo(result);
            close(tcp_socket);
            continue;
        }
        
        printf("[天气] 连接成功: %s\n", api_servers[i]);
        // 释放 addrinfo 结果
        freeaddrinfo(result);
        
        // 发送请求到 wttr.in（广西贺州市，JSON格式）
        // 尝试使用拼音城市名 "Hezhou"（贺州的拼音）
        // 如果不行，可以尝试坐标格式：~24.4141,111.5665
        const char *request = 
            "GET /Hezhou?format=j1&lang=zh HTTP/1.1\r\n"
            "Host: wttr.in\r\n"
            "User-Agent: curl\r\n"
            "Accept: */*\r\n"
            "Connection: close\r\n\r\n";
        
        printf("[天气] 发送HTTP请求...\n");
        ssize_t sent = write(tcp_socket, request, strlen(request));
        if (sent < 0) {
            printf("[天气] 发送请求失败: %s (errno: %d)\n", strerror(errno), errno);
            close(tcp_socket);
            continue;
        }
        printf("[天气] 已发送 %zd 字节\n", sent);
        
        // 读取响应（循环读取直到完整）
        size_t buffer_size = 65536;  // 64KB缓冲区
        response = malloc(buffer_size);
        if (!response) {
            close(tcp_socket);
            return NULL;
        }
        
        size_t total_read = 0;
        ssize_t len;
        
        // 先读取HTTP头部，查找Content-Length
        char header_buffer[4096] = {0};
        size_t header_len = 0;
        int content_length = -1;
        
        // 读取头部（直到遇到两个连续的\n）
        while (header_len < sizeof(header_buffer) - 1) {
            len = read(tcp_socket, header_buffer + header_len, 1);
            if (len <= 0) break;
            header_len += len;
            header_buffer[header_len] = '\0';
            
            // 检查是否到达头部结束（\r\n\r\n）
            if (header_len >= 4 && 
                header_buffer[header_len-4] == '\r' &&
                header_buffer[header_len-3] == '\n' &&
                header_buffer[header_len-2] == '\r' &&
                header_buffer[header_len-1] == '\n') {
                break;
            }
        }
        
        // 解析Content-Length
        char *cl_header = strstr(header_buffer, "Content-Length:");
        if (cl_header) {
            content_length = atoi(cl_header + 15);
            printf("[天气] Content-Length: %d\n", content_length);
        }
        
        // 将头部内容复制到response
        if (header_len > 0) {
            memcpy(response, header_buffer, header_len);
            total_read = header_len;
        }
        
        // 继续读取响应体
        while (total_read < buffer_size - 1) {
            len = read(tcp_socket, response + total_read, buffer_size - total_read - 1);
            if (len <= 0) {
                if (len == 0) {
                    printf("[天气] 连接正常关闭\n");
                } else {
                    printf("[天气] 读取错误: %s (errno: %d)\n", strerror(errno), errno);
                }
                break;
            }
            total_read += len;
            
            // 如果知道Content-Length，检查是否读取完整
            if (content_length > 0 && total_read >= (size_t)content_length) {
                printf("[天气] 已读取完整响应（Content-Length: %d）\n", content_length);
                break;
            }
            
            // 如果连续多次读取到0字节，可能已经结束
            if (len < 1024 && total_read > 1000) {
                // 给一个小延迟，看看是否还有数据
                usleep(10000);  // 10ms
            }
        }
        
        close(tcp_socket);
        
        if (total_read > 0) {
            response[total_read] = '\0';
            printf("[天气] 收到响应: %zu 字节\n", total_read);
            // 打印响应前500字符用于调试
            if (total_read > 500) {
                char preview[501];
                strncpy(preview, response, 500);
                preview[500] = '\0';
                printf("[天气] 响应预览: %s\n", preview);
            } else {
                printf("[天气] 完整响应: %s\n", response);
            }
            break;
        } else {
            printf("[天气] 未收到任何数据\n");
            free(response);
            response = NULL;
        }
    }
    
    if (!response) {
        char *offline_result = malloc(256);
        if (!offline_result) return NULL;
        snprintf(offline_result, 256, "网络连接失败");
        return offline_result;
    }
    
    // 解析多天数据
    char *result = malloc(2048);
    if (!result) {
        free(response);
        return NULL;
    }
    
    result[0] = '\0';
    char *current_pos = response;
    
    // 查找JSON数据部分（跳过HTTP头）
    char *json_start = strstr(response, "{");
    if (!json_start) {
        json_start = strstr(response, "[");
    }
    if (!json_start) {
        // 尝试查找第一个引号后的内容
        json_start = strchr(response, '"');
        if (json_start) {
            json_start = strchr(json_start + 1, '"');
        }
    }
    
    if (!json_start) {
        // 如果找不到JSON开始，尝试直接查找riqi
        json_start = response;
    }
    
    printf("[天气] JSON开始位置: %p (偏移: %d)\n", json_start, (int)(json_start - response));
    
    // wttr.in API返回的JSON格式：{"current_condition":[...], "weather":[...]}
    // 解析 weather 数组中的多天数据
    char *weather_array = strstr(json_start, "\"weather\"");
    if (!weather_array) {
        // 尝试查找数组开始
        weather_array = strstr(json_start, "\"weather\":[");
        if (!weather_array) {
            // 打印响应内容用于调试
            int preview_len = strlen(response) > 1000 ? 1000 : strlen(response);
            char *preview = malloc(preview_len + 1);
            if (preview) {
                strncpy(preview, response, preview_len);
                preview[preview_len] = '\0';
                printf("[天气] 未找到weather字段，响应预览:\n%s\n", preview);
                free(preview);
            }
            free(response);
            strcpy(result, "数据格式错误：未找到weather字段");
            return result;
        }
    }
    
    printf("[天气] 找到weather字段，位置: %p\n", weather_array);
    
    // 查找数组开始位置
    char *array_start = strchr(weather_array, '[');
    if (!array_start) {
        free(response);
        strcpy(result, "数据格式错误：未找到天气数组");
        return result;
    }
    
    // 解析最多6天数据（wttr.in 提供3天预报）
    int day_parsed = 0;
    // 找到第一个对象的开始位置（跳过 [ 和可能的空格）
    current_pos = array_start + 1;  // 跳过 [
    while (*current_pos == ' ' || *current_pos == '\n' || *current_pos == '\r' || *current_pos == '\t') {
        current_pos++;
    }
    // 确保指向第一个 {
    if (*current_pos != '{') {
        char *first_brace = strchr(current_pos, '{');
        if (first_brace) {
            current_pos = first_brace;
        }
    }
    
    for (int day = 0; day < 6 && current_pos; day++) {
        printf("[天气] 解析第%d天数据，当前位置: %p (字符: %c)\n", day + 1, current_pos, *current_pos);
        // wttr.in 的字段名
        char *date = extract_json_value(current_pos, "date");
        if (date) {
            printf("[天气] 第%d天日期: %s\n", day + 1, date);
        }
        char *avgtempC = extract_json_value(current_pos, "avgtempC");
        char *maxtempC = extract_json_value(current_pos, "maxtempC");
        char *mintempC = extract_json_value(current_pos, "mintempC");
        char *windspeedKmph = extract_json_value(current_pos, "windspeedKmph");
        char *humidity = extract_json_value(current_pos, "humidity");
        char *cloudcover = extract_json_value(current_pos, "cloudcover");
        
        // 尝试解析中文天气描述（lang_zh数组中的value）
        char *condition = NULL;
        char *lang_zh_start = strstr(current_pos, "\"lang_zh\"");
        if (lang_zh_start) {
            char *value_start = strstr(lang_zh_start, "\"value\"");
            if (value_start) {
                condition = extract_json_value(value_start, "value");
            }
        }
        // 如果没找到中文描述，尝试英文condition
        if (!condition) {
            condition = extract_json_value(current_pos, "condition");
        }
        
        // 如果至少有一个字段存在，认为这是一天的数据
        if (date || avgtempC || condition) {
            // 添加到结果字符串（格式：日期\n最高温/最低温\n平均温度\n天气\n风力\n湿度\n云量）
            strcat(result, date ? date : "--");
            strcat(result, "\n");
            
            // 温度范围（最高/最低）
            if (maxtempC && mintempC) {
                strcat(result, maxtempC);
                strcat(result, "/");
                strcat(result, mintempC);
                strcat(result, "°C");
            } else if (maxtempC) {
                strcat(result, "最高:");
                strcat(result, maxtempC);
                strcat(result, "°C");
            } else if (mintempC) {
                strcat(result, "最低:");
                strcat(result, mintempC);
                strcat(result, "°C");
            } else {
                strcat(result, "--");
            }
            strcat(result, "\n");
            
            // 平均温度
            strcat(result, avgtempC ? avgtempC : "--");
            strcat(result, "°C");
            strcat(result, "\n");
            
            // 天气状况
            strcat(result, condition ? condition : "--");
            strcat(result, "\n");
            
            // 风力
            strcat(result, windspeedKmph ? windspeedKmph : "--");
            strcat(result, "km/h");
            strcat(result, "\n");
            
            // 湿度
            strcat(result, humidity ? humidity : "--");
            strcat(result, "%");
            strcat(result, "\n");
            
            // 云量
            strcat(result, cloudcover ? cloudcover : "--");
            strcat(result, "%");
            
            if (day < 5) {
                strcat(result, "|"); // 天数分隔符
            }
            day_parsed++;
        }
        
        // 释放内存
        if (maxtempC) free(maxtempC);
        if (mintempC) free(mintempC);
        if (cloudcover) free(cloudcover);
        
        // 释放内存
        if (date) free(date);
        if (avgtempC) free(avgtempC);
        if (condition) free(condition);
        if (windspeedKmph) free(windspeedKmph);
        if (humidity) free(humidity);
        
        // 查找下一天（查找下一个 { 开始的对象）
        // 需要跳过当前对象的所有内容，找到下一个对象的开始
        if (current_pos) {
            // 找到当前对象的结束位置（匹配的 }）
            int brace_count = 0;
            char *pos = current_pos;
            // 找到weather数组的结束位置（从array_start开始，找到匹配的]）
            char *array_end = array_start;
            int bracket_count = 0;
            char *search_pos = array_start;
            while (*search_pos && search_pos < response + strlen(response)) {
                if (*search_pos == '[') {
                    bracket_count++;
                } else if (*search_pos == ']') {
                    bracket_count--;
                    if (bracket_count == 0) {
                        array_end = search_pos;
                        break;
                    }
                }
                search_pos++;
            }
            if (!array_end || array_end == array_start) {
                printf("[天气] 无法找到数组结束位置\n");
                break;
            }
            printf("[天气] 数组结束位置: %p (距离开始: %d)\n", array_end, (int)(array_end - array_start));
            
            // 跳过当前对象
            while (pos < array_end) {
                if (*pos == '{') {
                    brace_count++;
                } else if (*pos == '}') {
                    brace_count--;
                    if (brace_count == 0) {
                        // 找到了当前对象的结束
                        pos++; // 移动到 } 之后
                        break;
                    }
                }
                pos++;
            }
            
            // 跳过逗号和空格，找到下一个对象的开始
            while (pos < array_end && (*pos == ',' || *pos == ' ' || *pos == '\n' || *pos == '\r' || *pos == '\t')) {
                pos++;
            }
            
            // 检查是否还有下一个对象
            if (pos < array_end && *pos == '{') {
                current_pos = pos;
                printf("[天气] 找到第%d天数据，位置: %p (字符: %c)\n", day + 2, current_pos, *current_pos);
            } else {
                printf("[天气] 没有更多天数数据 (pos: %p, array_end: %p, 字符: %c)\n", 
                       pos, array_end, pos < array_end ? *pos : '?');
                // 打印当前位置附近的内容用于调试
                if (pos < array_end) {
                    char debug_buf[200];
                    int debug_len = pos + 100 < array_end ? 100 : array_end - pos;
                    strncpy(debug_buf, pos, debug_len);
                    debug_buf[debug_len] = '\0';
                    printf("[天气] 当前位置内容: %.100s\n", debug_buf);
                }
                break; // 没有更多数据
            }
        }
    }
    
    // 如果没有解析到任何数据
    if (day_parsed == 0) {
        // 打印调试信息
        printf("[天气] 解析失败：未找到任何天气数据\n");
        printf("[天气] 尝试查找的字段: date, avgtempC, condition\n");
        // 打印weather数组附近的内容
        if (array_start) {
            char *preview_start = array_start;
            int preview_len = 500;
            if (preview_start + preview_len > response + strlen(response)) {
                preview_len = strlen(response) - (preview_start - response);
            }
            if (preview_len > 0) {
                char *preview = malloc(preview_len + 1);
                if (preview) {
                    strncpy(preview, preview_start, preview_len);
                    preview[preview_len] = '\0';
                    printf("[天气] weather数组内容预览:\n%s\n", preview);
                    free(preview);
                }
            }
        }
        free(response);
        strcpy(result, "数据格式错误：无法解析天气数据");
        return result;
    }
    
    printf("[天气] 成功解析 %d 天数据\n", day_parsed);
    
    free(response);
    return result;
}

