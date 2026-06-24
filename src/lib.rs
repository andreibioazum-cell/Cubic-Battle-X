use macroquad::prelude::*;
use std::sync::{Arc, Mutex};

// Создаем хранилище для текста ошибки
lazy_static::lazy_static! {
    static ref PANIC_MESSAGE: Arc<Mutex<Option<String>>> = Arc::new(Mutex::new(None));
}

#[macroquad::main("CubicBattle")]
async fn main() {
    // Настраиваем перехватчик ошибок
    std::panic::set_hook(Box::new(|info| {
        let msg = format!("CRASH! Error: {}", info);
        if let Ok(mut storage) = PANIC_MESSAGE.lock() {
            *storage = Some(msg);
        }
    }));

    loop {
        // Проверяем, не случилась ли ошибка
        if let Ok(storage) = PANIC_MESSAGE.lock() {
            if let Some(ref msg) = *storage {
                clear_background(RED);
                draw_text("GAME CRASHED!", 20.0, 50.0, 40.0, WHITE);
                draw_text(msg, 20.0, 100.0, 20.0, YELLOW);
                next_frame().await;
                continue; // Не идем дальше в игру
            }
        }

        // Сама игра
        clear_background(BLACK);
        
        set_camera(&Camera3D {
            position: vec3(5.0, 5.0, 5.0),
            up: vec3(0.0, 1.0, 0.0),
            target: vec3(0.0, 0.0, 0.0),
            ..Default::default()
        });

        draw_grid(10, 1.0, BLUE, DARKBLUE);
        draw_cube(vec3(0.0, 1.0, 0.0), vec3(2.0, 2.0, 2.0), None, WHITE);

        set_default_camera();
        draw_text(&format!("FPS: {}", get_fps()), 10.0, 20.0, 20.0, GREEN);

        next_frame().await
    }
}
