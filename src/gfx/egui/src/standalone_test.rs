use super::*;
use crate::InputTranslator;
use crate::Renderer;
use egui::RichText;
use epaint::Color32;
use std::boxed::Box;
use std::sync::Mutex;

pub struct App {
    pub ctx: egui::Context,
    pub renderer: Renderer,
    pub input_translator: InputTranslator,
}

impl App {
    fn new() -> Self {
        Self {
            ctx: egui::Context::default(),
            renderer: Renderer::new(),
            input_translator: InputTranslator::new(),
        }
    }
}

#[derive(Default)]
struct State {
    pub color_test: color_test::ColorTest,
    pub multiline_text: String,
}

lazy_static! {
    static ref STATE: Mutex<State> = Mutex::new(State::default());
}

#[no_mangle]
pub unsafe extern "C" fn new_app() -> *mut App {
    Box::into_raw(Box::new(App::new()))
}

#[no_mangle]
pub unsafe extern "C" fn render_egui_test_frame(
    app_ptr: *mut App,
    context: *mut gfx_Context,
    queue: *mut gfx_DrawQueuePtr,
    sdl_events: *const u8,
    time: f64,
    delta_time: f32,
) {
    let app = &mut *app_ptr;
    let window = gfx_Context_getWindow(context);

    let raw_input = app
        .input_translator
        .translate(window, sdl_events, time, delta_time, 1.0)
        .unwrap();
    let draw_scale = raw_input.pixels_per_point.unwrap_or(1.0);

    let full_output = app.ctx.run(raw_input, |ctx| {
        egui::SidePanel::right("input").show(ctx, |ui| {
            let mut state = STATE.lock().unwrap();
            ui.code_editor(&mut state.multiline_text);
        });
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

    app.input_translator
        .update_text_cursor_position(window, full_output.platform_output.text_cursor_pos);

    if !full_output.platform_output.copied_text.is_empty() {
        app.input_translator
            .copy_text(&full_output.platform_output.copied_text);
    }

    app.input_translator
        .update_cursor_icon(full_output.platform_output.cursor_icon);

    app.renderer
        .render(&app.ctx, full_output, queue, draw_scale)
        .unwrap();
}
