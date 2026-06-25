#ifndef GAME_LOGIC_H
#define GAME_LOGIC_H
#include "math_util.h"

typedef struct {
    float x, y, z, rx, ry;
    float r_x, r_y, r_z, r_dx, r_dz, r_dy; int r_act; 
    float jsx, jsy, jcx, jcy; int tj;
    float lsx, lsy; int tl;
    int fps; long last_t;
} Game;

static char WORLD_MAP[16][16] = {
    {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1}, {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1}, {1,0,1,1,1,1,1,1,1,1,1,1,1,1,0,1}, {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
    {1,0,1,1,1,1,1,1,1,1,1,1,1,1,0,1}, {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1}, {1,0,1,1,1,1,1,1,1,1,1,1,1,1,0,1}, {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
    {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1}
};
#endif
