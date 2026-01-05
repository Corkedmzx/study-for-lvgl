/**
 * @file time_sync.h
 * @brief 时间同步模块头文件
 */

#ifndef TIME_SYNC_H
#define TIME_SYNC_H

/**
 * @brief 通过网络同步系统时间
 * @return 成功返回0，失败返回-1
 */
int sync_system_time(void);

#endif /* TIME_SYNC_H */

