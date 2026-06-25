#ifndef ENTITY_H
#define ENTITY_H
#include "utils.h"

typedef struct {
    float x, y, speed;
} Player;

static inline void entity_update_player(Player* p, float dx, float dy) {
    p->x += dx * p->speed;
    p->y += dy * p->speed;
    // Границы карты
    if(p->x < -1000) p->x = -1000; if(p->x > 1000) p->x = 1000;
    if(p->y < -1000) p->y = -1000; if(p->y > 1000) p->y = 1000;
}
#endif
