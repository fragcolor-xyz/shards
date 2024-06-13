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
use usvg::tiny_skia_path::IntSize;
use usvg::Transform;
use usvg::TreeParsing;

pub fn pixmap_to_var(pmap: &mut Pixmap) -> Var {
  Var {
    valueType: SHType_Image,
    payload: SHVarPayload {
      __bindgen_anon_1: SHVarPayload__bindgen_ty_1 {
        imageValue: SHImage {
          width: pmap.width() as u16,
          height: pmap.height() as u16,
          channels: 4,
          flags: SHIMAGE_FLAGS_PREMULTIPLIED_ALPHA as u8,
          data: pmap.as_mut().data_mut().as_mut_ptr(),
        },
      },
    },
    ..Default::default()
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
  pixmap: Option<Pixmap>,
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
      pixmap: None,
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
    // Get file's absolute directory.
    // opt.resources_dir = std::fs::canonicalize(&args[1]).ok().and_then(|p| p.parent().map(|p| p.to_path_buf()));
    // opt.fontdb.load_system_fonts();
    // opt.fontdb.set_generic_families();

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

    let (sx, sy): (i32, i32) = self.size.get().try_into().unwrap_or_default();
    if sx < 0 || sy < 0 {
      return Err("Invalid size");
    }
    let (w, h) = (sx as u32, sy as u32);

    let (offset_x, offset_y): (f32, f32) = self.offset.get().try_into().unwrap_or_default();

    let (pad_x, pad_y) = {
      let (x, y): (i32, i32) = self.padding.get().try_into().unwrap_or_default();
      (x as u32, y as u32)
    };

    let pixmap_size = if w == 0 && h == 0 {
      Ok(ntree.size.to_int_size())
    } else {
      IntSize::from_wh(w, h).ok_or("Invalid size")
    }?;

    if self.pixmap.is_none() {
      self.pixmap = Some(
        Pixmap::new(pixmap_size.width(), pixmap_size.height()).ok_or("Failed to create pixmap")?,
      );
    } else {
      let pm = self.pixmap.as_ref().unwrap();
      if pixmap_size.width() != pm.width() || pixmap_size.height() != pm.height() {
        self.pixmap = Some(
          Pixmap::new(pixmap_size.width(), pixmap_size.height())
            .ok_or("Failed to create pixmap")?,
        );
      }
    }

    let mut rtree = resvg::Tree::from_usvg(&ntree);

    let padded_size = IntSize::from_wh(
      pixmap_size.width() - pad_x * 2,
      pixmap_size.height() - pad_y * 2,
    )
    .unwrap_or(IntSize::from_wh(2, 2).unwrap()); // Add a fallback to make small/negative sizes not fail
    rtree.size = padded_size.to_size();

    rtree.render(
      Transform::from_translate(offset_x + pad_x as f32, offset_y + pad_y as f32),
      &mut self.pixmap.as_mut().unwrap().as_mut(),
    );

    Ok(pixmap_to_var(self.pixmap.as_mut().unwrap()))
  }
}

#[no_mangle]
pub extern "C" fn shardsRegister_svg_svg(core: *mut shards::shardsc::SHCore) {
  unsafe {
    shards::core::Core = core;
  }

  register_shard::<ToImage>();
}
