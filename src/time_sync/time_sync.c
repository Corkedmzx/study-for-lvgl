/**
 * @file time_sync.c
 * @brief 时间同步模块实现
 */

#include "time_sync.h"
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
#include <sys/time.h>

// 将UTC时间转换为time_t（避免使用timegm的兼容性问题）
static time_t parse_utc_time(struct tm *tm) {
    // 保存原始时区设置
    char *tz_orig = getenv("TZ");
    setenv("TZ", "UTC", 1);
    tzset();
    
    // 使用mktime，此时会按UTC时区解析
    time_t t = mktime(tm);
    
    // 恢复原始时区
    if (tz_orig) {
        setenv("TZ", tz_orig, 1);
    } else {
        unsetenv("TZ");
    }
    tzset();
    
    return t;
}

// 使用多个时间服务器（使用HTTP Date头，任何HTTP服务器都可以）
static const char *time_servers[] = {
    "www.baidu.com",         // 百度（主服务器，国内访问快）
    "www.qq.com",            // 腾讯（备用）
    "www.sina.com.cn",       // 新浪（备用）
    "www.google.com",        // Google（备用，国外）
    NULL
};

/**
 * @brief 从HTTP响应中提取时间戳
 */
static time_t parse_time_from_response(const char *response) {
    if (!response) {
        return 0;
    }
    
    // 查找JSON格式的时间字段（不同API可能使用不同的字段名）
    const char *time_patterns[] = {
        "\"datetime\":\"",      // worldtimeapi.org格式
        "\"currentDateTime\":\"", // timeapi.io格式
        "\"currentFileTime\":",   // worldclockapi.com格式（Unix时间戳，毫秒）
        "\"unixtime\":",          // ipgeolocation.io格式（Unix时间戳，秒）
        "\"timestamp\":",         // 通用时间戳字段
        NULL
    };
    
    for (int i = 0; time_patterns[i] != NULL; i++) {
        char *p = strstr(response, time_patterns[i]);
        if (p) {
            p += strlen(time_patterns[i]);
            
            // 处理Unix时间戳格式（数字）
            if (strstr(time_patterns[i], "currentFileTime") != NULL ||
                strstr(time_patterns[i], "unixtime") != NULL ||
                strstr(time_patterns[i], "timestamp") != NULL) {
                // 提取数字时间戳
                char *end;
                long long timestamp = strtoll(p, &end, 10);
                if (timestamp > 0) {
                    // 判断是秒还是毫秒（通常大于10000000000的是毫秒）
                    if (timestamp > 10000000000LL) {
                        return (time_t)(timestamp / 1000);  // 毫秒转秒
                    } else {
                        return (time_t)timestamp;  // 已经是秒
                    }
                }
            }
            // 处理ISO 8601格式（字符串）
            else {
                // 跳过引号
                if (*p == '"') p++;
                
                // ISO 8601格式: 2025-01-01T12:00:00+08:00 或 2025-01-01T12:00:00.000000+08:00
                struct tm tm = {0};
                int timezone_offset = 0;  // 时区偏移（小时）
                char *end = strchr(p, '"');
                if (!end) end = strchr(p, '+');
                if (!end) {
                    end = strchr(p, '-');
                    // 检查是否是时区偏移的负号（在T之后）
                    if (end && end > strchr(p, 'T')) {
                        // 这是时区偏移，不是日期的一部分
                        char *tz_start = end;
                        // 尝试解析时区偏移
                        int tz_hour = 0, tz_min = 0;
                        if (sscanf(tz_start, "%d:%d", &tz_hour, &tz_min) == 2 ||
                            sscanf(tz_start, "%d", &tz_hour) == 1) {
                            timezone_offset = tz_hour;
                            if (tz_start[0] == '-') timezone_offset = -timezone_offset;
                        }
                        // 找到时区偏移前的结束位置
                        end = tz_start;
                    }
                }
                if (!end) end = strchr(p, 'Z');  // UTC时间标记
                if (!end) end = strchr(p, '\r');
                if (!end) end = strchr(p, '\n');
                
                if (end) {
                    size_t len = end - p;
                    if (len >= 19) {  // 至少需要 "YYYY-MM-DDTHH:MM:SS"
                        char time_str[64] = {0};
                        strncpy(time_str, p, len < sizeof(time_str) - 1 ? len : sizeof(time_str) - 1);
                        
                        // 解析ISO 8601格式
                        if (sscanf(time_str, "%d-%d-%dT%d:%d:%d",
                                   &tm.tm_year, &tm.tm_mon, &tm.tm_mday,
                                   &tm.tm_hour, &tm.tm_min, &tm.tm_sec) == 6) {
                            tm.tm_year -= 1900;  // 年份从1900开始
                            tm.tm_mon -= 1;     // 月份从0开始
                            tm.tm_isdst = 0;    // 不使用夏令时
                            
                            // 转换为time_t（假设是UTC时间）
                            time_t t = parse_utc_time(&tm);
                            if (t > 0) {
                                // 如果有时区偏移，需要调整
                                if (timezone_offset != 0) {
                                    t -= timezone_offset * 3600;  // 减去时区偏移，转换为UTC
                                }
                                return t;
                            }
                        }
                    }
                }
            }
        }
    }
    
    return 0;
}

/**
 * @brief 从HTTP响应头中解析Date字段（RFC 822格式）
 * 格式示例: "Date: Mon, 01 Jan 2025 12:00:00 GMT"
 */
static time_t parse_date_header(const char *header) {
    if (!header) {
        return 0;
    }
    
    // 查找Date头
    char *date_start = strstr(header, "Date:");
    if (!date_start) {
        date_start = strstr(header, "date:");  // 小写
    }
    if (!date_start) {
        printf("[时间解析] 未找到Date头\n");
        return 0;
    }
    
    date_start += 5;  // 跳过 "Date:"
    
    // 跳过空格
    while (*date_start == ' ' || *date_start == '\t') {
        date_start++;
    }
    
    // 找到Date值的结束位置（换行符或回车符）
    char *date_end = strchr(date_start, '\r');
    if (!date_end) date_end = strchr(date_start, '\n');
    if (date_end) {
        size_t date_len = date_end - date_start;
        char date_str[128] = {0};
        if (date_len < sizeof(date_str) - 1) {
            strncpy(date_str, date_start, date_len);
            date_str[date_len] = '\0';
            printf("[时间解析] 原始Date头: %s\n", date_str);
        }
    }
    
    // RFC 822格式: "Mon, 01 Jan 2025 12:00:00 GMT"
    // 或者: "Mon, 01 Jan 2025 12:00:00 +0800"
    struct tm tm = {0};
    char day_name[4] = {0};
    char month_name[4] = {0};
    char timezone[8] = {0};
    int tz_offset = 0;
    
    // 尝试解析标准格式: "Mon, 01 Jan 2025 12:00:00 GMT"
    if (sscanf(date_start, "%3s, %d %3s %d %d:%d:%d %7s",
               day_name, &tm.tm_mday, month_name, &tm.tm_year,
               &tm.tm_hour, &tm.tm_min, &tm.tm_sec, timezone) == 8) {
        tm.tm_year -= 1900;  // 年份从1900开始
        
        // 解析月份
        const char *months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                 "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
        for (int i = 0; i < 12; i++) {
            if (strncmp(month_name, months[i], 3) == 0) {
                tm.tm_mon = i;
                break;
            }
        }
        
        tm.tm_isdst = 0;  // GMT不使用夏令时
        
        // 转换为time_t（GMT/UTC时间）
        time_t t = parse_utc_time(&tm);
        if (t > 0) {
            struct tm *check_tm = gmtime(&t);
            char check_str[64];
            strftime(check_str, sizeof(check_str), "%Y-%m-%d %H:%M:%S UTC", check_tm);
            printf("[时间解析] 解析为UTC时间: %s\n", check_str);
        }
        return t;
    }
    
    // 尝试解析带时区偏移的格式: "Mon, 01 Jan 2025 12:00:00 +0800"
    if (sscanf(date_start, "%3s, %d %3s %d %d:%d:%d %7s",
               day_name, &tm.tm_mday, month_name, &tm.tm_year,
               &tm.tm_hour, &tm.tm_min, &tm.tm_sec, timezone) == 8) {
        tm.tm_year -= 1900;
        
        // 解析月份
        const char *months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                 "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
        for (int i = 0; i < 12; i++) {
            if (strncmp(month_name, months[i], 3) == 0) {
                tm.tm_mon = i;
                break;
            }
        }
        
        // 解析时区偏移（格式: +0800 或 -0500）
        if (sscanf(timezone, "%d", &tz_offset) == 1) {
            int hours = tz_offset / 100;
            int minutes = tz_offset % 100;
            if (timezone[0] == '-') {
                hours = -hours;
                minutes = -minutes;
            }
            tz_offset = hours * 3600 + minutes * 60;
        }
        
        tm.tm_isdst = 0;
        
        // 转换为UTC时间
        time_t t = parse_utc_time(&tm);
        if (t > 0) {
            // 减去时区偏移，转换为UTC
            t -= tz_offset;
            struct tm *check_tm = gmtime(&t);
            char check_str[64];
            strftime(check_str, sizeof(check_str), "%Y-%m-%d %H:%M:%S UTC", check_tm);
            printf("[时间解析] 解析为UTC时间: %s (时区偏移: %d秒)\n", check_str, tz_offset);
        }
        return t;
    }
    
    return 0;
}

/**
 * @brief 通过网络同步系统时间
 */
int sync_system_time(void) {
    int tcp_socket = -1;
    char *response = NULL;
    time_t remote_time = 0;
    
    printf("[时间同步] 开始同步系统时间...\n");
    
    // 尝试连接各时间服务器
    for (int i = 0; time_servers[i] != NULL; i++) {
        tcp_socket = socket(AF_INET, SOCK_STREAM, 0);
        if (tcp_socket < 0) {
            printf("[时间同步] 创建socket失败: %s\n", strerror(errno));
            continue;
        }

        // 使用 getaddrinfo 进行域名解析
        struct addrinfo hints = {0};
        struct addrinfo *result = NULL;
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        
        int ret = getaddrinfo(time_servers[i], "80", &hints, &result);
        if (ret != 0 || !result) {
            printf("[时间同步] DNS解析失败: %s (错误: %d)\n", time_servers[i], ret);
            close(tcp_socket);
            continue;
        }
        
        // 使用第一个解析结果
        struct sockaddr_in *addr = (struct sockaddr_in *)result->ai_addr;

        // 设置超时
        struct timeval timeout = {.tv_sec = 10, .tv_usec = 0};
        setsockopt(tcp_socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
        setsockopt(tcp_socket, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

        printf("[时间同步] 尝试连接: %s\n", time_servers[i]);
        if (connect(tcp_socket, (struct sockaddr *)addr, result->ai_addrlen) < 0) {
            printf("[时间同步] 连接失败: %s (错误: %s)\n", time_servers[i], strerror(errno));
            freeaddrinfo(result);
            close(tcp_socket);
            continue;
        }
        
        printf("[时间同步] 连接成功: %s\n", time_servers[i]);
        freeaddrinfo(result);
        
        // 发送简单的HTTP GET请求（只需要响应头中的Date字段）
        char request_buf[256];
        snprintf(request_buf, sizeof(request_buf),
                 "HEAD / HTTP/1.1\r\n"
                 "Host: %s\r\n"
                 "User-Agent: curl\r\n"
                 "Connection: close\r\n\r\n",
                 time_servers[i]);
        const char *request = request_buf;
        
        printf("[时间同步] 发送HTTP请求...\n");
        ssize_t sent = write(tcp_socket, request, strlen(request));
        if (sent < 0) {
            printf("[时间同步] 发送请求失败: %s\n", strerror(errno));
            close(tcp_socket);
            continue;
        }
        printf("[时间同步] 已发送 %zd 字节\n", sent);
        
        // 读取响应
        size_t buffer_size = 8192;
        response = malloc(buffer_size);
        if (!response) {
            close(tcp_socket);
            continue;
        }
        
        size_t total_read = 0;
        ssize_t len;
        
        // 读取HTTP头部，查找Content-Length
        char header_buffer[2048] = {0};
        size_t header_len = 0;
        int content_length = -1;
        
        while (header_len < sizeof(header_buffer) - 1) {
            len = read(tcp_socket, header_buffer + header_len, 1);
            if (len <= 0) break;
            header_len++;
            
            if (header_len >= 4 && 
                strncmp(header_buffer + header_len - 4, "\r\n\r\n", 4) == 0) {
                break;
            }
        }
        
        // 解析Content-Length
        char *cl_header = strstr(header_buffer, "Content-Length:");
        if (cl_header) {
            sscanf(cl_header, "Content-Length: %d", &content_length);
            printf("[时间同步] Content-Length: %d\n", content_length);
        }
        
        // 检查HTTP状态码
        if (strstr(header_buffer, "HTTP/1.1 200") == NULL && 
            strstr(header_buffer, "HTTP/1.0 200") == NULL) {
            // 不是200 OK，打印状态码
            char *status_line = strstr(header_buffer, "HTTP/");
            if (status_line) {
                char status[128] = {0};
                char *end = strchr(status_line, '\r');
                if (!end) end = strchr(status_line, '\n');
                if (end) {
                    size_t len = end - status_line;
                    if (len < sizeof(status) - 1) {
                        strncpy(status, status_line, len);
                        status[len] = '\0';
                        printf("[时间同步] HTTP状态: %s\n", status);
                    }
                }
            }
            // 如果是重定向，尝试读取Location头
            if (strstr(header_buffer, "301") || strstr(header_buffer, "302")) {
                char *location = strstr(header_buffer, "Location:");
                if (location) {
                    char loc[256] = {0};
                    sscanf(location, "Location: %255s", loc);
                    printf("[时间同步] 重定向到: %s\n", loc);
                }
            }
        }
        
        // 读取响应体
        if (content_length > 0 && content_length < (int)buffer_size) {
            while (total_read < (size_t)content_length) {
                len = read(tcp_socket, response + total_read, 
                          content_length - total_read);
                if (len <= 0) break;
                total_read += len;
            }
            response[total_read] = '\0';
        } else {
            // 如果没有Content-Length，读取直到连接关闭或超时
            while (total_read < buffer_size - 1) {
                len = read(tcp_socket, response + total_read, 
                          buffer_size - total_read - 1);
                if (len <= 0) break;
                total_read += len;
            }
            response[total_read] = '\0';
        }
        
        printf("[时间同步] 收到响应: %zu 字节\n", total_read);
        if (total_read > 0) {
            // 打印响应前500字符用于调试
            size_t preview_len = total_read < 500 ? total_read : 500;
            printf("[时间同步] 响应预览 (前%zu字符):\n", preview_len);
            for (size_t j = 0; j < preview_len; j++) {
                putchar(response[j] >= 32 && response[j] < 127 ? response[j] : '.');
            }
            printf("\n");
        }
        
        close(tcp_socket);
        
        // 检查是否是200 OK响应（HEAD请求可能返回200或301/302）
        if (strstr(header_buffer, "HTTP/1.1 200") == NULL && 
            strstr(header_buffer, "HTTP/1.0 200") == NULL &&
            strstr(header_buffer, "HTTP/1.1 301") == NULL &&
            strstr(header_buffer, "HTTP/1.1 302") == NULL) {
            printf("[时间同步] 非成功响应，跳过解析\n");
            free(response);
            response = NULL;
            continue;
        }
        
        // 从HTTP响应头中解析Date字段
        remote_time = parse_date_header(header_buffer);
        if (remote_time > 0) {
            printf("[时间同步] ========== 时间解析结果 ==========\n");
            printf("[时间同步] 解析到UTC时间戳: %ld\n", (long)remote_time);
            
            // 显示解析到的UTC时间
            struct tm *tm_utc = gmtime(&remote_time);
            char utc_str[64];
            strftime(utc_str, sizeof(utc_str), "%Y-%m-%d %H:%M:%S", tm_utc);
            printf("[时间同步] HTTP获取的UTC时间: %s\n", utc_str);
            
            // 直接加上8小时（28800秒）得到中国时区时间
            time_t china_time = remote_time + 8 * 3600;  // UTC + 8小时
            
            // 显示加8小时后的时间
            struct tm *tm_china = gmtime(&china_time);
            char china_str[64];
            strftime(china_str, sizeof(china_str), "%Y-%m-%d %H:%M:%S", tm_china);
            printf("[时间同步] 加8小时后的时间（中国时区）: %s\n", china_str);
            printf("[时间同步] ====================================\n");
            
            // 根据开发板文档，使用date命令直接设置本地时间（UTC+8）
            // 格式：date -s "YYYY-MM-DD HH:MM:SS"
            char date_cmd[256];
            snprintf(date_cmd, sizeof(date_cmd), "date -s \"%s\"", china_str);
            printf("[时间同步] 执行命令: %s\n", date_cmd);
            
            int date_ret = system(date_cmd);
            if (date_ret == 0) {
                printf("[时间同步] 系统时间设置成功（使用date命令）\n");
                
                // 将时间写入硬件时钟（RTC），防止重启后丢失
                printf("[时间同步] 将时间写入硬件时钟...\n");
                int hwclock_ret = system("hwclock -w");
                if (hwclock_ret == 0) {
                    printf("[时间同步] 硬件时钟写入成功\n");
                } else {
                    printf("[时间同步] 警告：硬件时钟写入失败（可能没有hwclock命令）\n");
                }
                
                // 验证设置后的时间
                time_t now = time(NULL);
                struct tm *tm_now = localtime(&now);
                char now_str[64];
                strftime(now_str, sizeof(now_str), "%Y-%m-%d %H:%M:%S", tm_now);
                printf("[时间同步] 设置后系统时间: %s\n", now_str);
                printf("[时间同步] 期望的时间（UTC+8）: %s\n", china_str);
                
                // 设置系统时区为Asia/Shanghai (UTC+8)，用于后续时间显示
                setenv("TZ", "Asia/Shanghai", 1);
                tzset();
                
                // 尝试设置系统时区文件（如果支持）
                system("ln -sf /usr/share/zoneinfo/Asia/Shanghai /etc/localtime 2>/dev/null || true");
                
                free(response);
                return 0;
            } else {
                printf("[时间同步] 使用date命令设置时间失败（返回码: %d）\n", date_ret);
                printf("[时间同步] 尝试使用settimeofday系统调用...\n");
                
                // 备用方案：使用settimeofday设置UTC时间戳
                struct timeval tv;
                tv.tv_sec = remote_time;  // UTC时间戳
                tv.tv_usec = 0;
                
                if (settimeofday(&tv, NULL) == 0) {
                    printf("[时间同步] 使用settimeofday设置时间成功\n");
                    
                    // 设置时区
                    setenv("TZ", "Asia/Shanghai", 1);
                    tzset();
                    system("ln -sf /usr/share/zoneinfo/Asia/Shanghai /etc/localtime 2>/dev/null || true");
                    
                    // 写入硬件时钟
                    system("hwclock -w 2>/dev/null || true");
                    
                    free(response);
                    return 0;
                } else {
                    printf("[时间同步] 设置系统时间失败: %s (需要root权限)\n", strerror(errno));
                    printf("[时间同步] HTTP获取的UTC时间: %s\n", utc_str);
                    printf("[时间同步] 加8小时后的时间（中国时区）: %s\n", china_str);
                    printf("[时间同步] 建议：使用root权限运行程序\n");
                    free(response);
                    return -1;
                }
            }
        } else {
            printf("[时间同步] 无法从响应中解析时间\n");
        }
        
        free(response);
        response = NULL;
    }
    
    printf("[时间同步] 所有服务器尝试失败\n");
    return -1;
}

