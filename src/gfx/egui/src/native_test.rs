
use egui::ClippedPrimitive;
use egui::Context;
use egui::FullOutput;
use egui::ImageData;
use egui::Mesh;
use egui::TextureId;
use epaint::image;
use epaint::Primitive;
use epaint::Vertex;
use std::error::Error;

#[macro_use]
extern crate lazy_static;
use std::ptr;
use std::sync::Mutex;

mod color_test;

#[cfg(feature = "egui_test")]
mod egui_test {
    use egui::{color, RichText};

    use super::*;

    struct App {
        pub ctx: egui::Context,
    }

    impl Default for App {
        fn default() -> Self {
            let mut ctx = egui::Context::default();
            // ctx.set_pixels_per_point(1.0);
            // ctx.set_
            Self { ctx: ctx }
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
        renderer: *mut egui_EguiRenderer,
        deltaTime: f32,
    ) {
        use egui::Color32;

        let app = APP.lock().unwrap();
        let mut input = egui::RawInput::default();

        let mut screen_rect: egui_Rect = egui_Rect {
            min: egui_Pos2 { x: 0.0, y: 0.0 },
            max: egui_Pos2 { x: 0.0, y: 0.0 },
        };
        egui_EguiRenderer_getScreenRect(renderer, &mut screen_rect);
        input.screen_rect = Some(screen_rect.into());

        let ui_scale = egui_EguiRenderer_getScale(renderer);
        input.pixels_per_point = Some(ui_scale);

        let full_output = app.ctx.run(input, |ctx| {
            egui::CentralPanel::default().show(ctx, |ui| {
                let mut state = STATE.lock().unwrap();
                egui::ScrollArea::both()
                    .auto_shrink([false; 2])
                    .show(ui, |ui| {
                        let fps = 1.0 / deltaTime;
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
            // egui::CentralPanel::default().show(ctx, |ui| {
            //     let mut state = STATE.lock().unwrap();
            //     ui.heading("My egui Application");
            //     ui.horizontal(|ui| {
            //         ui.label("Your name: ");
            //         ui.text_edit_singleline(&mut state.name);
            //     });
            //     ui.add(egui::Slider::new(&mut state.age, 0..=120).text("age"));
            //     if ui.button("Click each year").clicked() {
            //         state.age += 1;
            //     }
            //     ui.label(format!("Hello '{}', age {}", state.name, state.age));
            //     ui.colored_label(Color32::GOLD, "Golden label");
            // });
        });

        let native_full_output = make_native_full_output(&app.ctx, full_output, ui_scale).unwrap();
        egui_EguiRenderer_render(renderer, &native_full_output.full_output);
    }
}
