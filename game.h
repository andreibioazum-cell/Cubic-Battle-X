#ifndef GAME_H
#define GAME_H
#include "math_util.h"

typedef struct {
    float x, z, rot_x, rot_y;
    float j_sx, j_sy, j_cx, j_cy; int touch_j;
    float l_sx, l_sy; int touch_l;
    int task_status; // 0 - найти рычаг, 1 - выйти
} GameState;

// Локация хоррора (0 - пусто, 1 - стена, 2 - цель)
int MAP[10][10] = {
    {1,1,1,1,1,1,1,1,1,1},
    {1,0,0,0,1,0,0,0,2,1},
    {1,0,1,0,1,0,1,1,0,1},
    {1,0,1,0,0,0,0,1,0,1},
    {1,0,1,1,1,1,0,1,0,1},
    {1,0,0,0,0,1,0,0,0,1},
    {1,1,1,1,0,1,1,1,0,1},
    {1,0,0,0,0,0,0,1,0,1},
    {1,0,1,1,1,1,0,0,0,1},
    {1,1,1,1,1,1,1,1,1,1}
};
#endif
