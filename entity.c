#include "entity.h"
#include <math.h>

void entity_move(Player* p, float dx, float dy) {
    float len2 = dx*dx + dy*dy;

    if (len2 > 0.0001f) {
        float invLen = 1.0f / sqrtf(len2);
        dx *= invLen;
        dy *= invLen;

        p->x += dx * p->speed;
        p->y += dy * p->speed;

        p->angle = atan2f(dy, dx);
    }
}
