#ifndef ENTITY_H
#define ENTITY_H
#include <math.h>

typedef struct { float x, y, speed, angle; } Player;

static inline void entity_update_player(Player* p, float dx, float dy) {
    p->x += dx * p->speed;
    p->y += dy * p->speed;
    p->angle = atan2f(dy, dx);
}
#endif
