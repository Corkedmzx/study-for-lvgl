/**
 * @file game_2048_win.h
 * @brief 2048游戏窗口头文件
 */

#ifndef GAME_2048_WIN_H
#define GAME_2048_WIN_H

/**
 * @brief 显示2048游戏窗口
 */
void game_2048_win_show(void);

/**
 * @brief 隐藏2048游戏窗口
 */
void game_2048_win_hide(void);

/**
 * @brief 更新2048游戏显示（在主线程中调用）
 */
void game_2048_win_update_display(void);

#endif // GAME_2048_WIN_H

