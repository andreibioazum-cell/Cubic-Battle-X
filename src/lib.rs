use macroquad::prelude::*;
use std::fs::OpenOptions;
use std::io::Write;

fn write_to_log(msg: &str) {
    // Путь к папке, куда приложению РАЗРЕШЕНО писать без разрешений на Android
    // Файл будет лежать в: Android/data/com.cubicbattle.x/files/crash_log.txt
    let path = "/sdcard/Android/data/com.cubicbattle.x/files/crash_log.txt";
    
    // Создаем папку, если её нет
    let _ = std::fs::create_dir_all("/sdcard/Android/data/com.cubicbattle.x/files");

    if let Ok(mut file) = OpenOptions::new().create(true).append(true).open(path) {
        let _ = writeln!(file, "[{}]", msg);
    }
}

#[macroquad::main("CubicBattle")]
async fn main() {
    // Перехват паники (краша)
    std::panic::set_hook(Box::new(|info| {
        let err_msg = format!("CRASH: {}", info);
        write_to_log(&err_msg);
    }));

    write_to_log("App Started successfully");

    loop {
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
        draw_text("If you see this, it works!", 10.0, 20.0, 20.0, GREEN);

        next_frame().await
    }
}
