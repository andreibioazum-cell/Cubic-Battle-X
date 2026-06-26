#ifndef GAME_LOGIC_H
#define GAME_LOGIC_H

#define MAP_W 32
#define MAP_H 20
#define TILE_SIZE 120.0f

static char WORLD_MAP[MAP_H][MAP_W]; // Просто объявляем

static inline void create_map_borders() {
    for(int y = 0; y < MAP_H; y++) {
        for(int x = 0; x < MAP_W; x++) {
            if(y == 0 || y == MAP_H - 1 || x == 0 || x == MAP_W - 1) {
                WORLD_MAP[y][x] = 1; // Стена
            } else {
                WORLD_MAP[y][x] = 0; // Пусто
            }
        }
    }
}
#endif
