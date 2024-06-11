use super::*;
use egui::epaint;
use egui::ClippedPrimitive;
use egui::Context;
use egui::FullOutput;
use egui::TextureId;
use std::ffi::CString;
use std::marker::PhantomData;
use std::ptr;

pub struct Renderer {
  egui_renderer: *mut gfx_EguiRenderer,
}

unsafe impl Send for Renderer {}
unsafe impl Sync for Renderer {}

pub struct NativeIOOutput {
  pub open_url: Option<CString>,
  pub copied_text: Option<CString>,
  pub text_cursor_position: Option<egui_Pos2>,
  pub cursor_icon: egui_CursorIcon,
  pub mutable_text_under_cursor: bool,
  pub wants_pointer_input: bool,
  pub wants_keyboard_input: bool,
}

impl NativeIOOutput {
  pub fn get_c_output(&self) -> egui_IOOutput {
    egui_IOOutput {
      openUrl: self.open_url.as_ref().map_or(ptr::null(), |x| x.as_ptr()),
      copiedText: self
        .copied_text
        .as_ref()
        .map_or(ptr::null(), |x| x.as_ptr()),
      validCursorPosition: self.text_cursor_position.is_some(),
      textCursorPosition: self
        .text_cursor_position
        .unwrap_or(egui_Pos2 { x: 0.0, y: 0.0 }),
      cursorIcon: self.cursor_icon,
      mutableTextUnderCursor: self.mutable_text_under_cursor,
      wantsPointerInput: self.wants_pointer_input,
      wantsKeyboardInput: self.wants_keyboard_input,
    }
  }
}

pub enum PixelData {
  Color(Vec<egui::Color32>),
  Font(Vec<f32>),
}

pub struct NativeRenderOutput<'a> {
  marker: PhantomData<&'a i32>,
  pub texture_sets: Vec<egui_TextureSet>,
  pub texture_frees: Vec<egui_TextureId>,
  pub clipped_primitives: Vec<egui_ClippedPrimitive>,
  pub vertex_mem: Vec<egui_Vertex>,
  pub index_mem: Vec<u32>,
}

impl<'a> NativeRenderOutput<'a> {
  pub fn get_c_output(&self) -> egui_RenderOutput {
    egui_RenderOutput {
      numPrimitives: self.clipped_primitives.len(),
      primitives: self.clipped_primitives.as_ptr(),
      textureUpdates: egui_TextureUpdates {
        sets: self.texture_sets.as_ptr(),
        numSets: self.texture_sets.len(),
        frees: self.texture_frees.as_ptr(),
        numFrees: self.texture_frees.len(),
      },
    }
  }
}

fn convert_primitive(
  clipped_primitive: ClippedPrimitive,
  vertex_mem: &mut Vec<egui_Vertex>,
  index_mem: &mut Vec<u32>,
  ui_scale: f32,
) -> Result<egui_ClippedPrimitive, &'static str> {
  if let epaint::Primitive::Mesh(mesh) = &clipped_primitive.primitive {
    let start_vertex = vertex_mem.len();
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
    let num_verts = vertex_mem.len() - start_vertex;
    let verts_ptr = if num_verts > 0 {
      &vertex_mem[start_vertex]
    } else {
      ptr::null()
    };

    let start_index = index_mem.len();
    for ind in &mesh.indices {
      index_mem.push(*ind);
    }
    let num_indices = index_mem.len() - start_index;
    let indices_ptr = if num_indices > 0 {
      &index_mem[start_index]
    } else {
      ptr::null()
    };

    let clip_rect = egui::Rect {
      min: (clipped_primitive.clip_rect.min.to_vec2() * ui_scale).to_pos2(),
      max: (clipped_primitive.clip_rect.max.to_vec2() * ui_scale).to_pos2(),
    };

    Ok(egui_ClippedPrimitive {
      clipRect: clip_rect.into(),
      vertices: verts_ptr,
      numVertices: num_verts,
      indices: indices_ptr,
      numIndices: num_indices,
      textureId: mesh.texture_id.into(),
    })
  } else {
    Err("Only mesh primitives are supported")
  }
}

fn convert_texture_set(
  id: epaint::TextureId,
  delta: &epaint::ImageDelta,
) -> Result<egui_TextureSet, &'static str> {
  let (format, ptr, size) = match &delta.image {
    epaint::ImageData::Color(color) => {
      let ptr = color.pixels.as_ptr();
      let size = color.size;
      (egui_TextureFormat_RGBA8, ptr as *const u8, size)
    }
    epaint::ImageData::Font(font) => {
      let ptr = font.pixels.as_ptr();
      let size = font.size;
      (egui_TextureFormat_R32F, ptr as *const u8, size)
    }
  };

  let (offset, sub_region) = if let Some(pos) = delta.pos {
    (pos, true)
  } else {
    ([0usize, 0usize], false)
  };

  Ok(egui_TextureSet {
    format,
    pixels: ptr,
    id: id.into(),
    offset,
    subRegion: sub_region,
    size,
  })
}

pub fn make_texture_updates(
  td: &egui::TexturesDelta,
) -> Result<(Vec<egui_TextureSet>, Vec<egui_TextureId>), &'static str> {
  let texture_sets: Vec<_> = td
    .set
    .iter()
    .map(|pair| convert_texture_set(pair.0, &pair.1))
    .collect::<Result<Vec<_>, _>>()?;

  let texture_frees: Vec<egui_TextureId> = td.free.iter().map(|id| (*id).into()).collect();

  Ok((texture_sets, texture_frees))
}

pub fn make_native_render_output<'a>(
  ctx: &Context,
  input: &'a egui::FullOutput,
  draw_scale: f32,
) -> Result<NativeRenderOutput<'a>, &'static str> {
  // Sad clone here, might be able to remove upstream
  let primitives: Vec<ClippedPrimitive> = if input.shapes.len() > 0 {
    ctx.tessellate(input.shapes.clone(), input.pixels_per_point)
  } else {
    Vec::new()
  };
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

  let (texture_sets, texture_frees) = make_texture_updates(&input.textures_delta)?;

  Ok(NativeRenderOutput {
    marker: PhantomData,
    clipped_primitives,
    texture_sets,
    texture_frees,
    vertex_mem,
    index_mem,
  })
}

pub fn collect_texture_references(input: &egui::FullOutput, out_refs: &mut Vec<GenericSharedPtr>) {
  for shape in &input.shapes {
    let tex_id = shape.shape.texture_id();
    match tex_id {
      TextureId::User(id) => unsafe {
        let texture_ptr = id as *const gfx_TexturePtr;
        let mut newRef = GenericSharedPtr::default();
        gfx_TexturePtr_refAt(&mut newRef, texture_ptr);
        out_refs.push(newRef);
      },
      _ => {}
    }
  }
}

pub fn make_native_io_output(
  ctx: &Context,
  input: &egui::FullOutput,
) -> Result<NativeIOOutput, &'static str> {
  let platform_output = &input.platform_output;
  let open_url = match &platform_output.open_url {
    Some(url) => Some(CString::new(url.url.as_str()).unwrap()),
    None => None,
  };

  let copied_text = if platform_output.copied_text.len() > 0 {
    Some(CString::new(platform_output.copied_text.as_str()).unwrap())
  } else {
    None
  };

  let text_cursor_position = platform_output.ime.map(|ime| egui_Pos2 {
    x: ime.cursor_rect.min.x,
    y: ime.cursor_rect.min.y,
  });

  Ok(NativeIOOutput {
    open_url,
    copied_text,
    text_cursor_position,
    cursor_icon: to_egui_cursor_icon(platform_output.cursor_icon),
    wants_keyboard_input: ctx.wants_keyboard_input(),
    wants_pointer_input: ctx.wants_pointer_input(),
    mutable_text_under_cursor: platform_output.mutable_text_under_cursor,
  })
}

impl Drop for Renderer {
  fn drop(&mut self) {
    unsafe {
      gfx_EguiRenderer_destroy(self.egui_renderer);
    }
  }
}

impl Default for Renderer {
  fn default() -> Self {
    Self::new()
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

  pub fn apply_texture_updates_only(
    &self,
    egui_output: &egui::FullOutput,
  ) -> Result<(), &'static str> {
    let (texture_sets, texture_frees) = make_texture_updates(&egui_output.textures_delta)?;
    let render_output = NativeRenderOutput {
      marker: PhantomData,
      texture_sets,
      texture_frees,
      clipped_primitives: Vec::new(),
      vertex_mem: Vec::new(),
      index_mem: Vec::new(),
    };
    let c_output = render_output.get_c_output();
    unsafe {
      gfx_EguiRenderer_applyTextureUpdatesOnly(self.egui_renderer, &c_output);
    }
    Ok(())
  }

  pub fn render(
    &self,
    ctx: &egui::Context,
    egui_output: &egui::FullOutput,
    queue: *const gfx_DrawQueuePtr,
    draw_scale: f32,
  ) -> Result<(), &'static str> {
    let render_output = make_native_render_output(ctx, egui_output, draw_scale)?;
    let c_output = render_output.get_c_output();
    self.render_with_native_render_output(&c_output, queue);
    Ok(())
  }

  pub fn render_with_native_render_output(
    &self,
    egui_output: *const egui_RenderOutput,
    queue: *const gfx_DrawQueuePtr,
  ) {
    unsafe {
      gfx_EguiRenderer_renderNoTransform(self.egui_renderer, egui_output, queue);
    }
  }
}
