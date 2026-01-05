/**
 * @file game_2048.c
 * @brief 2048游戏逻辑实现
 */

#include "game_2048.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>

/**
 * @brief 获取随机数
 */
static int random_int(int min, int max) {
    return min + (rand() % (max - min + 1));
}

/**
 * @brief 初始化游戏
 */
void game_2048_init(game_2048_t *game) {
    if (!game) return;
    
    // 清空网格
    memset(game->grid, 0, sizeof(game->grid));
    game->score = 0;
    game->game_over = false;
    game->moved = false;
    
    // 添加两个初始方块
    game_2048_add_random_tile(game);
    game_2048_add_random_tile(game);
}

/**
 * @brief 在随机位置添加数字2
 */
bool game_2048_add_random_tile(game_2048_t *game) {
    if (!game) return false;
    
    // 查找所有空位置
    int empty_cells[GRID_SIZE * GRID_SIZE][2];
    int empty_count = 0;
    
    for (int i = 0; i < GRID_SIZE; i++) {
        for (int j = 0; j < GRID_SIZE; j++) {
            if (game->grid[i][j] == 0) {
                empty_cells[empty_count][0] = i;
                empty_cells[empty_count][1] = j;
                empty_count++;
            }
        }
    }
    
    if (empty_count == 0) {
        return false;  // 网格已满
    }
    
    // 随机选择一个空位置
    int random_index = random_int(0, empty_count - 1);
    int row = empty_cells[random_index][0];
    int col = empty_cells[random_index][1];
    
    // 添加数字2（90%概率）或4（10%概率）
    game->grid[row][col] = (random_int(1, 10) <= 9) ? 2 : 4;
    
    return true;
}

/**
 * @brief 向左移动一行
 */
static bool move_row_left(int *row) {
    bool moved = false;
    int write_pos = 0;
    
    // 移除零，向左压缩
    for (int i = 0; i < GRID_SIZE; i++) {
        if (row[i] != 0) {
            if (i != write_pos) {
                row[write_pos] = row[i];
                row[i] = 0;
                moved = true;
            }
            write_pos++;
        }
    }
    
    // 合并相同的数字
    for (int i = 0; i < GRID_SIZE - 1; i++) {
        if (row[i] != 0 && row[i] == row[i + 1]) {
            row[i] *= 2;
            row[i + 1] = 0;
            moved = true;
        }
    }
    
    // 再次压缩（移除合并后产生的零）
    write_pos = 0;
    for (int i = 0; i < GRID_SIZE; i++) {
        if (row[i] != 0) {
            if (i != write_pos) {
                row[write_pos] = row[i];
                row[i] = 0;
                moved = true;
            }
            write_pos++;
        }
    }
    
    return moved;
}

/**
 * @brief 向左滑动
 */
bool game_2048_move_left(game_2048_t *game) {
    if (!game || game->game_over) return false;
    
    game->moved = false;
    int old_grid[GRID_SIZE][GRID_SIZE];
    memcpy(old_grid, game->grid, sizeof(game->grid));
    
    // 对每一行进行左移
    for (int i = 0; i < GRID_SIZE; i++) {
        if (move_row_left(game->grid[i])) {
            game->moved = true;
        }
    }
    
    // 如果移动了，添加新方块并更新分数
    if (game->moved) {
        // 计算合并的分数（比较新旧网格，找出合并的数字）
        for (int i = 0; i < GRID_SIZE; i++) {
            for (int j = 0; j < GRID_SIZE; j++) {
                // 如果新位置的值是旧位置值的2倍，说明发生了合并
                // 需要检查是否真的发生了合并（值增加且不是移动）
                if (game->grid[i][j] != 0 && old_grid[i][j] != 0) {
                    if (game->grid[i][j] == old_grid[i][j] * 2) {
                        // 发生了合并，增加分数
                        game->score += game->grid[i][j];
                    }
                }
            }
        }
        game_2048_add_random_tile(game);
        game->game_over = game_2048_check_game_over(game);
    }
    
    return game->moved;
}

/**
 * @brief 向右滑动
 */
bool game_2048_move_right(game_2048_t *game) {
    if (!game || game->game_over) return false;
    
    game->moved = false;
    int old_grid[GRID_SIZE][GRID_SIZE];
    memcpy(old_grid, game->grid, sizeof(game->grid));
    
    // 对每一行进行右移（先反转，左移，再反转）
    for (int i = 0; i < GRID_SIZE; i++) {
        int temp[GRID_SIZE];
        for (int j = 0; j < GRID_SIZE; j++) {
            temp[j] = game->grid[i][GRID_SIZE - 1 - j];
        }
        if (move_row_left(temp)) {
            game->moved = true;
        }
        for (int j = 0; j < GRID_SIZE; j++) {
            game->grid[i][GRID_SIZE - 1 - j] = temp[j];
        }
    }
    
    // 如果移动了，添加新方块并更新分数
    if (game->moved) {
        // 计算合并的分数（比较新旧网格，找出合并的数字）
        for (int i = 0; i < GRID_SIZE; i++) {
            for (int j = 0; j < GRID_SIZE; j++) {
                // 如果新位置的值是旧位置值的2倍，说明发生了合并
                if (game->grid[i][j] != 0 && old_grid[i][j] != 0) {
                    if (game->grid[i][j] == old_grid[i][j] * 2) {
                        // 发生了合并，增加分数
                        game->score += game->grid[i][j];
                    }
                }
            }
        }
        game_2048_add_random_tile(game);
        game->game_over = game_2048_check_game_over(game);
    }
    
    return game->moved;
}

/**
 * @brief 向上滑动
 */
bool game_2048_move_up(game_2048_t *game) {
    if (!game || game->game_over) return false;
    
    game->moved = false;
    int old_grid[GRID_SIZE][GRID_SIZE];
    memcpy(old_grid, game->grid, sizeof(game->grid));
    
    // 对每一列进行上移（转置，左移，再转置）
    for (int j = 0; j < GRID_SIZE; j++) {
        int temp[GRID_SIZE];
        for (int i = 0; i < GRID_SIZE; i++) {
            temp[i] = game->grid[i][j];
        }
        if (move_row_left(temp)) {
            game->moved = true;
        }
        for (int i = 0; i < GRID_SIZE; i++) {
            game->grid[i][j] = temp[i];
        }
    }
    
    // 如果移动了，添加新方块并更新分数
    if (game->moved) {
        // 计算合并的分数（比较新旧网格，找出合并的数字）
        for (int i = 0; i < GRID_SIZE; i++) {
            for (int j = 0; j < GRID_SIZE; j++) {
                // 如果新位置的值是旧位置值的2倍，说明发生了合并
                if (game->grid[i][j] != 0 && old_grid[i][j] != 0) {
                    if (game->grid[i][j] == old_grid[i][j] * 2) {
                        // 发生了合并，增加分数
                        game->score += game->grid[i][j];
                    }
                }
            }
        }
        game_2048_add_random_tile(game);
        game->game_over = game_2048_check_game_over(game);
    }
    
    return game->moved;
}

/**
 * @brief 向下滑动
 */
bool game_2048_move_down(game_2048_t *game) {
    if (!game || game->game_over) return false;
    
    game->moved = false;
    int old_grid[GRID_SIZE][GRID_SIZE];
    memcpy(old_grid, game->grid, sizeof(game->grid));
    
    // 对每一列进行下移（转置，反转，左移，反转，再转置）
    for (int j = 0; j < GRID_SIZE; j++) {
        int temp[GRID_SIZE];
        for (int i = 0; i < GRID_SIZE; i++) {
            temp[i] = game->grid[GRID_SIZE - 1 - i][j];
        }
        if (move_row_left(temp)) {
            game->moved = true;
        }
        for (int i = 0; i < GRID_SIZE; i++) {
            game->grid[GRID_SIZE - 1 - i][j] = temp[i];
        }
    }
    
    // 如果移动了，添加新方块并更新分数
    if (game->moved) {
        // 计算合并的分数（比较新旧网格，找出合并的数字）
        for (int i = 0; i < GRID_SIZE; i++) {
            for (int j = 0; j < GRID_SIZE; j++) {
                // 如果新位置的值是旧位置值的2倍，说明发生了合并
                if (game->grid[i][j] != 0 && old_grid[i][j] != 0) {
                    if (game->grid[i][j] == old_grid[i][j] * 2) {
                        // 发生了合并，增加分数
                        game->score += game->grid[i][j];
                    }
                }
            }
        }
        game_2048_add_random_tile(game);
        game->game_over = game_2048_check_game_over(game);
    }
    
    return game->moved;
}

/**
 * @brief 检查游戏是否结束
 */
bool game_2048_check_game_over(game_2048_t *game) {
    if (!game) return true;
    
    // 检查是否有空位置
    for (int i = 0; i < GRID_SIZE; i++) {
        for (int j = 0; j < GRID_SIZE; j++) {
            if (game->grid[i][j] == 0) {
                return false;  // 有空位置，游戏继续
            }
        }
    }
    
    // 检查是否可以合并
    for (int i = 0; i < GRID_SIZE; i++) {
        for (int j = 0; j < GRID_SIZE; j++) {
            int current = game->grid[i][j];
            // 检查右边
            if (j < GRID_SIZE - 1 && game->grid[i][j + 1] == current) {
                return false;  // 可以合并
            }
            // 检查下边
            if (i < GRID_SIZE - 1 && game->grid[i + 1][j] == current) {
                return false;  // 可以合并
            }
        }
    }
    
    return true;  // 无法移动，游戏结束
}

/**
 * @brief 重置游戏
 */
void game_2048_reset(game_2048_t *game) {
    game_2048_init(game);
}

