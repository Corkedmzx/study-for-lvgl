/**
 * @file game_2048.h
 * @brief 2048游戏逻辑头文件
 */

#ifndef GAME_2048_H
#define GAME_2048_H

#include <stdbool.h>

#define GRID_SIZE 4  // 4x4网格

// 游戏状态
typedef struct {
    int grid[GRID_SIZE][GRID_SIZE];  // 游戏网格
    int score;                        // 当前分数
    bool game_over;                   // 游戏是否结束
    bool moved;                       // 本次操作是否移动了方块
} game_2048_t;

/**
 * @brief 初始化游戏
 * @param game 游戏状态指针
 */
void game_2048_init(game_2048_t *game);

/**
 * @brief 在随机位置添加数字2
 * @param game 游戏状态指针
 * @return 成功返回true，失败返回false（网格已满）
 */
bool game_2048_add_random_tile(game_2048_t *game);

/**
 * @brief 向左滑动
 * @param game 游戏状态指针
 * @return 是否发生了移动
 */
bool game_2048_move_left(game_2048_t *game);

/**
 * @brief 向右滑动
 * @param game 游戏状态指针
 * @return 是否发生了移动
 */
bool game_2048_move_right(game_2048_t *game);

/**
 * @brief 向上滑动
 * @param game 游戏状态指针
 * @return 是否发生了移动
 */
bool game_2048_move_up(game_2048_t *game);

/**
 * @brief 向下滑动
 * @param game 游戏状态指针
 * @return 是否发生了移动
 */
bool game_2048_move_down(game_2048_t *game);

/**
 * @brief 检查游戏是否结束（无法移动）
 * @param game 游戏状态指针
 * @return 游戏结束返回true，否则返回false
 */
bool game_2048_check_game_over(game_2048_t *game);

/**
 * @brief 重置游戏
 * @param game 游戏状态指针
 */
void game_2048_reset(game_2048_t *game);

#endif // GAME_2048_H

