#include "raylib.h"

// Эта функция нужна для интеграции с Android NativeActivity
void android_main(struct android_app* app) {
    // Инициализация окна (Raylib сам определит размер экрана)
    InitWindow(0, 0, "Cubic Battle Raylib");

    // Настройка камеры
    Camera3D camera = { 0 };
    camera.position = (Vector3){ 10.0f, 10.0f, 10.0f }; // Позиция камеры
    camera.target = (Vector3){ 0.0f, 0.0f, 0.0f };      // Куда смотрит
    camera.up = (Vector3){ 0.0f, 1.0f, 0.0f };          // Верх камеры
    camera.fovy = 45.0f;                                // Угол обзора
    camera.projection = CAMERA_PERSPECTIVE;             // Перспектива

    SetTargetFPS(60);

    while (!WindowShouldClose()) {
        // Обновление (здесь можно добавить вращение)
        UpdateCamera(&camera, CAMERA_ORBITAL);

        // Отрисовка
        BeginDrawing();
            ClearBackground(RAYWHITE); // Белый фон

            BeginMode3D(camera);
                // Рисуем куб
                DrawCube((Vector3){ 0.0f, 0.0f, 0.0f }, 2.0f, 2.0f, 2.0f, RED);
                // Рисуем сетку на полу
                DrawGrid(10, 1.0f);
            EndMode3D();

            DrawFPS(10, 10);
            DrawText("Cubic Battle: Raylib 3D", 10, 40, 20, DARKGRAY);
        EndDrawing();
    }

    CloseWindow();
}
