use macroquad::prelude::*;

#[macroquad::main("CubicBattle")]
async fn main() {
    let mut rotation = 0.0;

    loop {
        clear_background(BLACK);

        // Настройка 3D камеры
        set_camera(&Camera3D {
            position: vec3(5.0, 5.0, 5.0),
            up: vec3(0.0, 1.0, 0.0),
            target: vec3(0.0, 0.0, 0.0),
            ..Default::default()
        });

        // Рисуем сетку на полу
        draw_grid(20, 1.0, LIGHTGRAY, GRAY);

        // Рисуем вращающийся куб
        rotation += 0.01;
        draw_cube(
            vec3(0.0, 1.0, 0.0), 
            vec3(2.0, 2.0, 2.0), 
            None, 
            Color::from_rgba(100, 200, 255, 255)
        );
        
        // Рисуем линии куба поверх (wireframe)
        draw_cube_wires(vec3(0.0, 1.0, 0.0), vec3(2.0, 2.0, 2.0), BLACK);

        // Возвращаемся в 2D режим для текста/интерфейса
        set_default_camera();
        draw_text("Cubic Battle - No GC - Rust", 20.0, 30.0, 30.0, WHITE);
        draw_text(&format!("FPS: {}", get_fps()), 20.0, 60.0, 30.0, GREEN);

        next_frame().await
    }
}
