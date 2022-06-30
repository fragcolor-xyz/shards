#![allow(non_upper_case_globals)]
#![allow(non_camel_case_types)]
#![allow(non_snake_case)]
include!(concat!(env!("OUT_DIR"), "/bindings.rs"));

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

enum PixelData {
    Color(Vec<egui::Color32>),
    Font(Vec<f32>),
}

struct NativeFullOutput {
    texture_sets: Vec<egui_TextureSet>,
    texture_frees: Vec<egui_TextureId>,
    clipped_primitives: Vec<egui_ClippedPrimitive>,
    vertex_mem: Vec<egui_Vertex>,
    index_mem: Vec<u32>,
    image_mem: Vec<PixelData>,
    pub full_output: egui_FullOutput,
}

impl From<egui::Pos2> for egui_Pos2 {
    fn from(pos: egui::Pos2) -> Self {
        Self { x: pos.x, y: pos.y }
    }
}

impl From<egui::Rect> for egui_Rect {
    fn from(r: egui::Rect) -> Self {
        Self {
            min: r.min.into(),
            max: r.max.into(),
        }
    }
}

impl From<epaint::Color32> for egui_Color32 {
    fn from(c: epaint::Color32) -> Self {
        Self {
            r: c.r(),
            g: c.g(),
            b: c.b(),
            a: c.a(),
        }
    }
}

impl From<epaint::Vertex> for egui_Vertex {
    fn from(v: epaint::Vertex) -> Self {
        Self {
            pos: v.pos.into(),
            uv: v.uv.into(),
            color: v.color.into(),
        }
    }
}

impl From<epaint::TextureId> for egui_TextureId {
    fn from(v: epaint::TextureId) -> Self {
        Self {
            id: if let epaint::TextureId::Managed(managed) = v {
                managed
            } else {
                0
            },
        }
    }
}

fn convert_primitive(
    clipped_primitive: ClippedPrimitive,
    vertex_mem: &mut Vec<egui_Vertex>,
    index_mem: &mut Vec<u32>,
) -> Result<egui_ClippedPrimitive, &'static str> {
    if let epaint::Primitive::Mesh(mesh) = &clipped_primitive.primitive {
        let startVertex = vertex_mem.len();
        for vert in &mesh.vertices {
            vertex_mem.push(vert.clone().into());
        }
        let numVerts = (vertex_mem.len() - startVertex);
        let vertsPtr = if numVerts > 0 {
            &vertex_mem[startVertex]
        } else {
            ptr::null()
        };

        let startIndex = index_mem.len();
        for ind in &mesh.indices {
            index_mem.push(*ind);
        }
        let numIndices = (index_mem.len() - startIndex);
        let indicesPtr = if numIndices > 0 {
            &index_mem[startIndex]
        } else {
            ptr::null()
        };

        Ok(egui_ClippedPrimitive {
            clipRect: clipped_primitive.clip_rect.into(),
            vertices: vertsPtr,
            numVertices: numVerts,
            indices: indicesPtr,
            numIndices: numIndices,
            textureId: mesh.texture_id.into(),
        })
    } else {
        Err("Only mesh primitives are supported")
    }
}

fn convert_texture_set(
    id: epaint::TextureId,
    delta: epaint::ImageDelta,
    image_data: &mut Vec<PixelData>,
) -> Result<egui_TextureSet, &'static str> {
    let (format, ptr, size, pixels) = match (delta.image) {
        epaint::ImageData::Color(color) => {
            let ptr = color.pixels.as_ptr();
            let size = color.size.clone();
            (
                egui_TextureFormat_RGBA8,
                ptr as *const u8,
                size,
                PixelData::Color(color.pixels),
            )
        }
        epaint::ImageData::Font(font) => {
            let ptr = font.pixels.as_ptr();
            let size = font.size.clone();
            (
                egui_TextureFormat_R32F,
                ptr as *const u8,
                size,
                PixelData::Font(font.pixels),
            )
        }
    };

    image_data.push(pixels);

    let (offset, sub_region) = if let Some(pos) = delta.pos {
        (pos, true)
    } else {
        ([0usize, 0usize], false)
    };

    Ok(egui_TextureSet {
        format: format,
        pixels: ptr,
        id: id.into(),
        offset: offset,
        subRegion: sub_region,
        size: size,
    })
}

fn make_native_full_output(
    ctx: &Context,
    input: egui::FullOutput,
) -> Result<NativeFullOutput, &str> {
    let primitives = ctx.tessellate(input.shapes);
    let mut numVerts = 0;
    let mut numIndices = 0;
    for prim in primitives.iter() {
        if let epaint::Primitive::Mesh(mesh) = &prim.primitive {
            numVerts += mesh.vertices.len();
            numIndices += mesh.indices.len();
        } else {
            return Err("Only mesh primitives are supported");
        }
    }

    let mut vertex_mem: Vec<egui_Vertex> = Vec::with_capacity(numVerts);
    let mut index_mem: Vec<u32> = Vec::with_capacity(numIndices);
    let clipped_primitives: Vec<_> = primitives
        .into_iter()
        .map(|prim| convert_primitive(prim, &mut vertex_mem, &mut index_mem).unwrap())
        .collect();

    let mut image_mem: Vec<PixelData> = Vec::new();
    let texture_sets: Vec<_> = input
        .textures_delta
        .set
        .into_iter()
        .map(|pair| convert_texture_set(pair.0, pair.1, &mut image_mem).unwrap())
        .collect();

    let texture_frees: Vec<egui_TextureId> = input
        .textures_delta
        .free
        .into_iter()
        .map(|id| id.into())
        .collect();

    let full_output = egui_FullOutput {
        numPrimitives: clipped_primitives.len(),
        primitives: clipped_primitives.as_ptr(),
        textureUpdates: egui_TextureUpdates {
            sets: texture_sets.as_ptr(),
            numSets: texture_sets.len(),
            frees: texture_frees.as_ptr(),
            numFrees: texture_frees.len(),
        },
    };

    Ok(NativeFullOutput {
        clipped_primitives: clipped_primitives,
        texture_sets: texture_sets,
        texture_frees: texture_frees,
        vertex_mem: vertex_mem,
        index_mem: index_mem,
        image_mem: image_mem,
        full_output: full_output,
    })
}

mod color_test;

#[cfg(feature = "egui_test")]
mod egui_test {
    use egui::{color, RichText};

    use super::*;

    #[derive(Default)]
    struct App {
        pub ctx: egui::Context,
    }

    #[derive(Default)]
    struct State {
        pub age: i32,
        pub name: String,
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
        let input = egui::RawInput::default();

        let full_output = app.ctx.run(input, |ctx| {
            egui::CentralPanel::default().show(ctx, |ui| {
                let mut state = STATE.lock().unwrap();
                egui::ScrollArea::both()
                    .auto_shrink([false; 2])
                    .show(ui, |ui| {
                        let fps = 1.0 / deltaTime;
                        ui.horizontal_wrapped(|ui| {
                            ui.label(RichText::new("FPS:"));
                            ui.label(RichText::from(format!("{}", fps)).color(if (fps > 110.0) {
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

        let native_full_output = make_native_full_output(&app.ctx, full_output).unwrap();
        egui_EguiRenderer_render(renderer, &native_full_output.full_output);
    }
}
