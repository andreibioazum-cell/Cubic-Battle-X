#ifndef ENTITY_H
#define ENTITY_H

typedef struct {
    float x;
    float y;
    float speed;
    float angle;
} Player;

void entity_move(Player* p, float dx, float dy);

#endif
