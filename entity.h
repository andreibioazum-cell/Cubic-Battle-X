#ifndef ENTITY_H
#define ENTITY_H
#include "game_logic.h"
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define PLAYER_SIZE 120.0f
#define PLAYER_HITBOX_RADIUS 40.0f

typedef struct { float x, y, speed, angle; } Player;

static inline int is_wall(float x, float y) {
    int gx = floorf(x / TILE_SIZE);
    int gy = floorf(y / TILE_SIZE);
    if(gx < 0 || gx >= MAP_W || gy < 0 || gy >= MAP_H) return 1;
    return WORLD_MAP[gy][gx] == 1;
}

static inline void entity_update_player(Player* p, float dx, float dy) {
    float next_x = p->x + dx * p->speed;
    float next_y = p->y + dy * p->speed;

    // Скольжение по стенам
    if (!is_wall(next_x + (dx > 0 ? PLAYER_HITBOX_RADIUS : -PLAYER_HITBOX_RADIUS), p->y)) {
        p->x = next_x;
    }
    if (!is_wall(p->x, next_y + (dy > 0 ? PLAYER_HITBOX_RADIUS : -PLAYER_HITBOX_RADIUS))) {
        p->y = next_y;
    }

    // ИСПРАВЛЕНО: Правильный угол, чтобы игрок смотрел вперед
    // (Предполагается, что в файле ordinary.png игрок нарисован смотрящим ВНИЗ)
    if(fabsf(dx) > 0.01f || fabsf(dy) > 0.01f) {
        p->angle = atan2f(dy, dx) + (M_PI / 2.0f);
    }
}
#endif
