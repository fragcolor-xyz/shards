extern crate egui;
extern crate egui_gfx;
use std::boxed::Box;

use egui::{Pos2, Rect, Vec2};
use egui_gfx::make_native_full_output;

pub struct Instance {
    pub renderer: egui_gfx::Renderer,
    pub ctx: egui::Context,
    pub text: String,
}

impl Instance {
    fn new() -> Self {
        Self {
            renderer: egui_gfx::Renderer::new(),
            ctx: egui::Context::default(),
            text: String::new(),
        }
    }
}

type RenderCallback = unsafe extern "C" fn(ctx: *mut u8, output: *const egui_gfx::egui_FullOutput);

#[repr(C)]
pub struct VUIArgs {
    instance: *mut Instance,
    render_context: *mut u8,
    render: RenderCallback,
    input: *const egui_gfx::egui_Input,
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
            ui.label(":)");
        });
    });

    let native_egui_output = make_native_full_output(ctx, output, 1.0).unwrap();
    (args.render)(args.render_context, &native_egui_output.full_output);
}

#[no_mangle]
pub unsafe extern "C" fn vui_1(args: &mut VUIArgs) {
    let ctx = &(*args.instance).ctx;

    let raw_input = egui_gfx::translate_raw_input(&*args.input).unwrap();
    let output = ctx.run(raw_input, |ctx| {
        let area = egui::Area::new("Main");
        area.show(ctx, |ui| {
            ui.label("Hello world");
            ui.label(":)");
            if ui.button("Press me").clicked() {}
        });
    });

    let native_egui_output = make_native_full_output(ctx, output, 1.0).unwrap();
    (args.render)(args.render_context, &native_egui_output.full_output);
}

#[no_mangle]
pub unsafe extern "C" fn vui_2(args: &mut VUIArgs) {
    let ctx = &(*args.instance).ctx;

    let raw_input = egui_gfx::translate_raw_input(&*args.input).unwrap();
    let output = ctx.run(raw_input, |ctx| {
        let area = egui::Area::new("Main")
            // .drag_bounds(Rect::from_min_size(
            //     Pos2::new(0.0, 0.0),
            //     Vec2::new(0.0, 0.0),
            // ))
            .current_pos(Pos2::ZERO)
            // .fixed_pos(Pos2::ZERO)
            ;

        let resp = area
            .show(ctx, |ui| {
                ui.label("Text control");
                ui.text_edit_multiline(&mut (*args.instance).text);
            })
            .response;

        if resp.dragged() {
            let delta = resp.drag_delta();

            println!("Dragging ({}, {})", delta.x, delta.y);
        }
    });

    let native_egui_output = make_native_full_output(ctx, output, 1.0).unwrap();
    (args.render)(args.render_context, &native_egui_output.full_output);
}
