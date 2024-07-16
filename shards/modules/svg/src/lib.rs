/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2021 Fragcolor Pte. Ltd. */
#[macro_use]
extern crate shards;

#[macro_use]
extern crate lazy_static;

extern crate compile_time_crc32;

use resvg::tiny_skia::Pixmap;
use shards::core::register_legacy_shard;
use shards::core::register_shard;
use shards::shard;
use shards::shard::LegacyShard;
use shards::shard::Shard;
use shards::shardsc::SHImage;
use shards::shardsc::SHVarPayload;
use shards::shardsc::SHVarPayload__bindgen_ty_1;
use shards::shardsc::SHIMAGE_FLAGS_PREMULTIPLIED_ALPHA;
use shards::shardsc::{SHType_Bytes, SHType_Image, SHType_String};
use shards::types::common_type;
use shards::types::ClonedVar;
use shards::types::Context;
use shards::types::ExposedTypes;
use shards::types::ParamVar;
use shards::types::Parameters;
use shards::types::Type;
use shards::types::Var;
use shards::types::FLOAT2_TYPES;
use shards::types::FLOAT2_TYPES_SLICE;
use shards::types::IMAGE_TYPES;
use shards::types::INT2_TYPES;
use shards::types::INT2_TYPES_SLICE;
use std::convert::TryInto;
use std::mem::size_of;
use usvg::tiny_skia_path::IntSize;
use usvg::Transform;
use usvg::TreeParsing;

#[repr(C)]
struct PixmapImageContainer {
  image: SHImage,
  pixmap: Pixmap,
}

pub extern "C" fn free_pixmap_image(img: *mut SHImage) {
  unsafe {
    let container = img as *mut PixmapImageContainer;
    std::ptr::drop_in_place(&mut (*container).pixmap);
  }
}

pub fn pixmap_to_var(pmap: Pixmap) -> ClonedVar {
  unsafe {
    let alloc_size = size_of::<PixmapImageContainer>() - size_of::<SHImage>();
    let image = (*shards::core::Core).imageNew.unwrap_unchecked()(alloc_size as u32);
    let container = image as *mut PixmapImageContainer;
    std::ptr::write(&mut (*container).pixmap, pmap);
    let pixmap = &(*container).pixmap;
    (*image).data = pixmap.data().as_ptr() as *mut u8;
    (*image).width = pixmap.width() as u16;
    (*image).height = pixmap.height() as u16;
    (*image).channels = 4;
    (*image).flags = SHIMAGE_FLAGS_PREMULTIPLIED_ALPHA as u8;
    (*image).free = Some(free_pixmap_image);
    ClonedVar(Var {
      valueType: SHType_Image,
      payload: SHVarPayload {
        __bindgen_anon_1: SHVarPayload__bindgen_ty_1 { imageValue: image },
      },
      ..Default::default()
    })
  }
}

lazy_static! {
  static ref INPUT_TYPES: Vec<Type> = vec![common_type::string, common_type::bytes];
  static ref SIZE_TYPES: Vec<Type> =
    vec![common_type::int2, common_type::int2_var, common_type::none];
}

#[derive(shard)]
#[shard_info("SVG.ToImage", "Converts an SVG string or bytes to an image.")]
struct ToImage {
  output: ClonedVar,
  #[shard_param(
    "Size",
    "The desired output size, if (0, 0) will default to the size defined in the svg data.",
    SIZE_TYPES
  )]
  size: ParamVar,
  #[shard_param("Offset", "A positive x and y value offsets towards the right and the bottom of the screen respectively. (0.0, 0.0) by default.", [common_type::none, common_type::float2, common_type::float2_var])]
  offset: ParamVar,
  #[shard_param("Padding", "Pixels of padding to add", INT2_TYPES)]
  padding: ParamVar,
  #[shard_required]
  required: ExposedTypes,
}

impl Default for ToImage {
  fn default() -> Self {
    Self {
      output: ClonedVar::default(),
      size: ParamVar::default(),
      offset: ParamVar::default(),
      padding: ParamVar::default(),
      required: ExposedTypes::default(),
    }
  }
}

#[shard_impl]
impl Shard for ToImage {
  fn input_types(&mut self) -> &std::vec::Vec<Type> {
    &INPUT_TYPES
  }
  fn output_types(&mut self) -> &std::vec::Vec<Type> {
    &IMAGE_TYPES
  }
  fn warmup(&mut self, context: &Context) -> Result<(), &str> {
    self.warmup_helper(context)?;
    Ok(())
  }
  fn cleanup(&mut self, ctx: Option<&Context>) -> Result<(), &str> {
    self.cleanup_helper(ctx)?;
    Ok(())
  }
  fn activate(&mut self, _: &Context, input: &Var) -> Result<Var, &str> {
    let mut opt = usvg::Options::default();

    let ntree = match input.valueType {
      SHType_Bytes => usvg::Tree::from_data(input.try_into().unwrap(), &opt).map_err(|e| {
        shlog!("{}", e);
        "Failed parse SVG bytes"
      }),
      SHType_String => usvg::Tree::from_str(input.try_into().unwrap(), &opt).map_err(|e| {
        shlog!("{}", e);
        "Failed to parse SVG string"
      }),
      _ => Err("Invalid input type"),
    }?;

    let size_var = self.size.get();
    let (sx, sy) = if size_var.is_none() {
      let default_size = ntree.size.to_int_size();
      (default_size.width() as i32, default_size.height() as i32)
    } else {
      let (sx, sy): (i32, i32) = size_var.try_into().unwrap_or_default();
      (sx, sy)
    };

    let (w, h) = if sx <= 0 || sy <= 0 {
      (2, 2) // Fallback for negative size
    } else {
      (sx as u32, sy as u32)
    };

    let (offset_x, offset_y): (f32, f32) = self.offset.get().try_into().unwrap_or_default();

    let (pad_x, pad_y) = {
      let (x, y): (i32, i32) = self.padding.get().try_into().unwrap_or_default();
      (x as u32, y as u32)
    };

    let pixmap_size = if w == 0 && h == 0 {
      ntree.size.to_int_size()
    } else {
      IntSize::from_wh(w, h).expect("Invalid size")
    };

    let mut pixmap =
      Pixmap::new(pixmap_size.width(), pixmap_size.height()).ok_or("Failed to create pixmap")?;

    let mut rtree = resvg::Tree::from_usvg(&ntree);

    let padx_2 = pad_x * 2;
    let pady_2 = pad_y * 2;
    let padded_size = if padx_2 >= pixmap_size.width() || pady_2 >= pixmap_size.height() {
      IntSize::from_wh(2, 2).unwrap() // Fallback
    } else {
      IntSize::from_wh(
        pixmap_size.width() - pad_x * 2,
        pixmap_size.height() - pad_y * 2,
      )
      .unwrap_or(IntSize::from_wh(2, 2).unwrap()) // Add a fallback to make small/negative sizes not fail
    };
    rtree.size = padded_size.to_size();

    rtree.render(
      Transform::from_translate(offset_x + pad_x as f32, offset_y + pad_y as f32),
      &mut pixmap.as_mut(),
    );

    self.output = pixmap_to_var(pixmap);
    Ok(self.output.0)
  }
}

#[no_mangle]
pub extern "C" fn shardsRegister_svg_svg(core: *mut shards::shardsc::SHCore) {
  unsafe {
    shards::core::Core = core;
  }

  register_shard::<ToImage>();
}
