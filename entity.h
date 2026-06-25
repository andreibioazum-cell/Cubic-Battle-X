#ifndef ENTITY_H
#define ENTITY_H
#include "game_logic.h"

typedef struct { float x, y, speed; } Player;

static inline void entity_update_player(Player* p, float dx, float dy) {
    float next_x = p->x + dx * p->speed;
    float next_y = p->y + dy * p->speed;

    // Коллизия по X
    int mx = (int)(next_x / TILE_SIZE);
    int my = (int)(p->y / TILE_SIZE);
    if(mx >= 0 && mx < MAP_W && my >= 0 && my < MAP_H && WORLD_MAP[my][mx] == 0) {
        p->x = next_x;
    }
    
    // Коллизия по Y
    mx = (int)(p->x / TILE_SIZE);
    my = (int)(next_y / TILE_SIZE);
    if(mx >= 0 && mx < MAP_W && my >= 0 && my < MAP_H && WORLD_MAP[my][mx] == 0) {
        p->y = next_y;
    }
}
#endif
