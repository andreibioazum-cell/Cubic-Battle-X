#ifndef ENTITY_H
#define ENTITY_H

typedef struct { float x, y, speed; } Player;

static inline void entity_update_player(Player* p, float dx, float dy) {
    p->x += dx * p->speed;
    p->y += dy * p->speed;
    // Границы мира (необязательно, но для порядка)
    if(p->x < -2000) p->x = -2000; if(p->x > 2000) p->x = 2000;
    if(p->y < -2000) p->y = -2000; if(p->y > 2000) p->y = 2000;
}
#endif
