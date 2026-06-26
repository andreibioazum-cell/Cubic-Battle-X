#ifndef ENTITY_H
#define ENTITY_H
#include "game_logic.h"
#include <math.h>

typedef struct { float x, y, speed, angle; } Player;

static inline void entity_update_player(Player* p, float dx, float dy) {
    float nx = p->x + dx * p->speed;
    float ny = p->y + dy * p->speed;

    // Конвертируем координаты в индексы сетки
    int grid_x = (int)(nx / TILE_SIZE);
    int grid_y = (int)(ny / TILE_SIZE);

    // БЕЗОПАСНОСТЬ: Проверяем, что индексы внутри массива 16x9
    if (grid_x >= 0 && grid_x < MAP_W && grid_y >= 0 && grid_y < MAP_H) {
        // Проверяем, что там не стена
        if (WORLD_MAP[grid_y][grid_x] == 0) {
            p->x = nx;
            p->y = ny;
        }
    }
    
    if (fabsf(dx) > 0.01f || fabsf(dy) > 0.01f) {
        p->angle = atan2f(dy, dx);
    }
}
#endif
