#ifndef GAME_H
#define GAME_H

#include <android_native_app_glue.h>
#include "entity.h"

#define MAP_W 32
#define MAP_H 20
#define TILE_SIZE 120.0f

typedef struct {
    Player player;
    char map[MAP_H][MAP_W];
} Game;

void game_init(Game* game);
void game_update(Game* game);
int32_t game_handle_input(Game* game,
                          struct android_app* app,
                          AInputEvent* event);

#endif
