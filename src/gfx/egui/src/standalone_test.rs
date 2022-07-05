use super::*;
use crate::input::to_egui_cursor_icon;
use crate::Renderer;
use egui::{FontData, RichText};
use epaint::Color32;
use std::{collections::BTreeMap, ffi::CString, os::raw::c_char, ptr::null, sync::Mutex};

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
    pub multiline_text: String,
}

lazy_static! {
    static ref STATE: Mutex<State> = Mutex::new(State::default());
    static ref APP: Mutex<App> = Mutex::new(App::default());
}

#[no_mangle]
pub unsafe extern "C" fn render_egui_test_frame(
    context: *mut gfx_Context,
    queue: *mut gfx_DrawQueuePtr,
    input_translator: *mut gfx_EguiInputTranslator,
    input_ptr: *const egui_Input,
) {
    let raw_input = translate_raw_input(&*input_ptr).unwrap();
    let app = APP.lock().unwrap();
    let window = gfx_Context_getWindow(context);

    let draw_scale = (*input_ptr).pixelsPerPoint;
    let delta_time = (*input_ptr).predictedDeltaTime;

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

    match full_output.platform_output.text_cursor_pos {
        Some(pos) => {
            let native_pos: egui_Pos2 = pos.into();
            gfx_EguiInputTranslator_updateTextCursorPosition(input_translator, window, &native_pos);
        }
        None => gfx_EguiInputTranslator_updateTextCursorPosition(input_translator, window, null()),
    };

    if !full_output.platform_output.copied_text.is_empty() {
        if let Ok(c_str) = CString::new(full_output.platform_output.copied_text.to_owned()) {
            gfx_EguiInputTranslator_copyText(input_translator, c_str.as_ptr());
        }
    }

    gfx_EguiInputTranslator_updateCursorIcon(
        input_translator,
        to_egui_cursor_icon(full_output.platform_output.cursor_icon),
    );

    app.renderer
        .render(&app.ctx, full_output, queue, draw_scale);
}
