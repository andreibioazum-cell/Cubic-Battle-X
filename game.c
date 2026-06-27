#include "game.h"
#include <math.h>

void game_init(Game* game) {
    for(int y = 0; y < MAP_H; y++)
        for(int x = 0; x < MAP_W; x++)
            game->map[y][x] =
                (y==0 || y==MAP_H-1 || x==0 || x==MAP_W-1);

    game->player.x = 300;
    game->player.y = 300;
    game->player.speed = 8.0f;
}

void game_update(Game* game) {
    // Пока пусто
}

int32_t game_handle_input(Game* game,
                          struct android_app* app,
                          AInputEvent* event) {
    return 1;
}
