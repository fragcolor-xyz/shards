extern crate egui;
extern crate egui_gfx;
use std::boxed::Box;

pub struct Instance {
    pub renderer: egui_gfx::Renderer,
    pub ctx: egui::Context,
}

impl Instance {
    fn new() -> Self {
        Self {
            renderer: egui_gfx::Renderer::new(),
            ctx: egui::Context::default(),
        }
    }
}

#[repr(C)]
pub struct VUIArgs {
    instance: *mut Instance,
    queue: *const egui_gfx::gfx_DrawQueuePtr,
    input: *const egui_gfx::egui_Input,
    draw_scale: f32,
    time: f64,
    delta_time: f32,
}

#[no_mangle]
pub unsafe extern "C" fn vui_new_instance() -> *mut Instance {
    Box::into_raw(Box::new(Instance::new()))
}

#[no_mangle]
pub unsafe extern "C" fn vui_drop_instance(inst: *mut Instance) {
    drop(Box::from_raw(inst))
}


#[no_mangle]
pub unsafe extern "C" fn vui_0(args: &mut VUIArgs) {
    let ctx = &(*args.instance).ctx;

    let raw_input = egui_gfx::translate_raw_input(&*args.input).unwrap();

    let output = ctx.run(raw_input, |ctx| {
        egui::Window::new("My Window 0").show(ctx, |ui| {
            ui.label("Hello world");
            ui.button("Press me");
            ui.label(":)");
        });
    });

    let renderer = &(*args.instance).renderer;
    renderer.render(ctx, output, args.queue, args.draw_scale).unwrap();
}

#[no_mangle]
pub unsafe extern "C" fn vui_1(args: &mut VUIArgs) {
    let ctx = &(*args.instance).ctx;

    let raw_input = egui_gfx::translate_raw_input(&*args.input).unwrap();
    let output = ctx.run(raw_input, |ctx| {
        egui::Window::new("My Window 1").show(ctx, |ui| {
            ui.label("Hello world");
            ui.button("Press me");
            ui.label(":)");
        });
    });

    let renderer = &(*args.instance).renderer;
    renderer.render(ctx, output, args.queue, args.draw_scale).unwrap();
}
