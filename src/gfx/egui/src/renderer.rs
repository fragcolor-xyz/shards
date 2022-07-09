use super::*;
use egui::ClippedPrimitive;
use egui::Context;
use std::ptr;

pub struct Renderer {
    egui_renderer: *mut gfx_EguiRenderer,
}

unsafe impl Send for Renderer {}
unsafe impl Sync for Renderer {}

pub enum PixelData {
    Color(Vec<egui::Color32>),
    Font(Vec<f32>),
}

pub struct NativeFullOutput {
    pub texture_sets: Vec<egui_TextureSet>,
    pub texture_frees: Vec<egui_TextureId>,
    pub clipped_primitives: Vec<egui_ClippedPrimitive>,
    pub vertex_mem: Vec<egui_Vertex>,
    pub index_mem: Vec<u32>,
    pub image_mem: Vec<PixelData>,
    pub full_output: egui_FullOutput,
}

fn convert_primitive(
    clipped_primitive: ClippedPrimitive,
    vertex_mem: &mut Vec<egui_Vertex>,
    index_mem: &mut Vec<u32>,
    ui_scale: f32,
) -> Result<egui_ClippedPrimitive, &'static str> {
    if let epaint::Primitive::Mesh(mesh) = &clipped_primitive.primitive {
        let startVertex = vertex_mem.len();
        for vert in &mesh.vertices {
            let vert_native = egui_Vertex {
                pos: egui_Pos2 {
                    x: vert.pos.x * ui_scale,
                    y: vert.pos.y * ui_scale,
                },
                color: vert.color.into(),
                uv: vert.uv.into(),
            };
            vertex_mem.push(vert_native);
        }
        let numVerts = vertex_mem.len() - startVertex;
        let vertsPtr = if numVerts > 0 {
            &vertex_mem[startVertex]
        } else {
            ptr::null()
        };

        let startIndex = index_mem.len();
        for ind in &mesh.indices {
            index_mem.push(*ind);
        }
        let numIndices = index_mem.len() - startIndex;
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
    let (format, ptr, size, pixels) = match delta.image {
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
    draw_scale: f32,
) -> Result<NativeFullOutput, &'static str> {
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
        .map(|prim| convert_primitive(prim, &mut vertex_mem, &mut index_mem, draw_scale))
        .collect::<Result<Vec<_>, _>>()?;

    let mut image_mem: Vec<PixelData> = Vec::new();
    let texture_sets: Vec<_> = input
        .textures_delta
        .set
        .into_iter()
        .map(|pair| convert_texture_set(pair.0, pair.1, &mut image_mem))
        .collect::<Result<Vec<_>, _>>()?;

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

impl Drop for Renderer {
    fn drop(&mut self) {
        unsafe {
            gfx_EguiRenderer_destroy(self.egui_renderer);
        }
    }
}

impl Renderer {
    pub fn new() -> Self {
        unsafe {
            Self {
                egui_renderer: gfx_EguiRenderer_create(),
            }
        }
    }

    pub fn render(
        self: &Self,
        ctx: &egui::Context,
        egui_output: egui::FullOutput,
        queue: *const gfx_DrawQueuePtr,
        draw_scale: f32,
    ) -> Result<(), &str> {
        unsafe {
            let native_egui_output = make_native_full_output(ctx, egui_output, draw_scale)?;
            gfx_EguiRenderer_render(self.egui_renderer, &native_egui_output.full_output, queue);
            Ok(())
        }
    }
}
