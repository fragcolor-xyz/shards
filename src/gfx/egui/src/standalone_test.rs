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
    input_ptr: *const egui_Input,
) {
    use egui::Color32;

    let raw_input = translate_raw_input(&*input_ptr);
    let app = APP.lock().unwrap();

    let draw_scale = (*input_ptr).pixelsPerPoint;
    let delta_time = (*input_ptr).predictedDeltaTime;

    let full_output = app.ctx.run(raw_input, |ctx| {
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
