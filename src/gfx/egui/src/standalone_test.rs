use super::*;
use crate::Renderer;
use egui::RichText;
use std::sync::Mutex;

struct App {
    pub ctx: egui::Context,
    pub renderer: Renderer,
}

impl Default for App {
    fn default() -> Self {
        Self {
            ctx: egui::Context::default(),
            renderer: Renderer::new(),
        }
    }
}

#[derive(Default)]
struct State {
    pub color_test: color_test::ColorTest,
}

lazy_static! {
    static ref STATE: Mutex<State> = Mutex::new(State::default());
    static ref APP: Mutex<App> = Mutex::new(App::default());
}

#[no_mangle]
pub unsafe extern "C" fn render_egui_test_frame(
    context: *mut gfx_Context,
    queue: *mut gfx_DrawQueuePtr,
    delta_time: f32,
) {
    use egui::Color32;

    let app = APP.lock().unwrap();
    let mut input = egui::RawInput::default();

    let window = gfx_Context_getWindow(context);
    let screen_rect_size = gfx_Window_getVirtualDrawableSize_ext(window);
    input.screen_rect = Some(egui::Rect {
        min: egui::pos2(0.0, 0.0),
        max: egui::pos2(screen_rect_size.x, screen_rect_size.y),
    });

    let draw_scale_vec = gfx_Window_getDrawScale_ext(window);
    let draw_scale = f32::max(draw_scale_vec.x, draw_scale_vec.y);
    input.pixels_per_point = Some(draw_scale);

    let full_output = app.ctx.run(input, |ctx| {
        egui::CentralPanel::default().show(ctx, |ui| {
            let mut state = STATE.lock().unwrap();
            egui::ScrollArea::both()
                .auto_shrink([false; 2])
                .show(ui, |ui| {
                    let fps = 1.0 / delta_time;
                    ui.horizontal_wrapped(|ui| {
                        ui.label(RichText::new("FPS:"));
                        ui.label(RichText::from(format!("{}", fps)).color(if fps > 110.0 {
                            Color32::GREEN
                        } else {
                            Color32::RED
                        }));
                    });
                    state.color_test.ui(ui);
                });
        });
    });

    app.renderer
        .render(&app.ctx, full_output, queue, draw_scale);
}
