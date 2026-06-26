#ifndef ENTITY_H
#define ENTITY_H
#include "game_logic.h"

typedef struct { float x, y, speed, angle; } Player;

static inline void entity_update_player(Player* p, float dx, float dy) {
    float nx = p->x + dx * p->speed;
    float ny = p->y + dy * p->speed;
    int gx = (int)(nx / TILE_SIZE);
    int gy = (int)(ny / TILE_SIZE);

    if(gx >= 0 && gx < MAP_W && gy >= 0 && gy < MAP_H) {
        if(WORLD_MAP[gy][gx] == 0) { p->x = nx; p->y = ny; }
    }
    if(fabsf(dx) > 0.1f || fabsf(dy) > 0.1f) p->angle = atan2f(dy, dx);
}
#endif
