#ifndef ENTITY_H
#define ENTITY_H
#include "game_logic.h"
#include <math.h>

typedef struct { float x, y, speed, angle; } Player;

static inline void entity_update_player(Player* p, float dx, float dy) {
    float nx = p->x + dx * p->speed;
    float ny = p->y + dy * p->speed;

    // Простая проверка коллизий по тайлам
    if(WORLD_MAP[(int)(ny/TILE_SIZE)][(int)(nx/TILE_SIZE)] == 0) {
        p->x = nx; p->y = ny;
    }
    p->angle = atan2f(dy, dx);
}
#endif
