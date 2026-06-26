#ifndef ENTITY_H
#define ENTITY_H
#include "game_logic.h"
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define PLAYER_SIZE 120.0f
#define PLAYER_HITBOX_RADIUS 40.0f // Хитбокс чуть меньше визуала

typedef struct { float x, y, speed, angle; } Player;

static inline int is_wall(float x, float y) {
    int gx = (int)(x / TILE_SIZE);
    int gy = (int)(y / TILE_SIZE);
    if(gx < 0 || gx >= MAP_W || gy < 0 || gy >= MAP_H) return 1; // За картой - стена
    return WORLD_MAP[gy][gx] == 1;
}

static inline void entity_update_player(Player* p, float dx, float dy) {
    float next_x = p->x + dx * p->speed;
    float next_y = p->y + dy * p->speed;

    // СКОЛЬЖЕНИЕ ПО СТЕНАМ (Проверяем оси отдельно)
    // Движение по X
    if (!is_wall(next_x + PLAYER_HITBOX_RADIUS * (dx > 0 ? 1 : -1), p->y)) {
        p->x = next_x;
    }
    // Движение по Y
    if (!is_wall(p->x, next_y + PLAYER_HITBOX_RADIUS * (dy > 0 ? 1 : -1))) {
        p->y = next_y;
    }

    // Вращение (смещаем на 90 градусов, чтобы смотрел вперед, а не вбок)
    if(fabsf(dx) > 0.01f || fabsf(dy) > 0.01f) {
        p->angle = atan2f(dy, dx) - (M_PI / 2.0f);
    }
}
#endif
