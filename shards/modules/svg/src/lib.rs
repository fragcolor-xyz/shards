/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2021 Fragcolor Pte. Ltd. */
#[macro_use]
extern crate shards;

#[macro_use]
extern crate lazy_static;

extern crate compile_time_crc32;


use shards::core::registerShard;
use shards::shard::LegacyShard;
use shards::shardsc::SHImage;
use shards::shardsc::SHVarPayload;
use shards::shardsc::SHVarPayload__bindgen_ty_1;
use shards::shardsc::SHIMAGE_FLAGS_PREMULTIPLIED_ALPHA;
use shards::shardsc::{SHType_Bytes, SHType_Image, SHType_String};
use shards::types::common_type;

use shards::types::Context;

use shards::types::Parameters;


use shards::types::Type;
use shards::types::IMAGE_TYPES;
use shards::types::INT2_TYPES_SLICE;


use shards::types::Var;

use std::convert::TryInto;
use tiny_skia::Pixmap;
use usvg::ScreenSize;

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
  static ref PARAMETERS: Parameters = vec![(
    cstr!("Size"),
    shccstr!(
      "The desired output size, if (0, 0) will default to the size defined in the svg data."
    ),
    INT2_TYPES_SLICE
  )
    .into()];
}

#[derive(Default)]
struct ToImage {
  pixmap: Option<Pixmap>,
  size: (i64, i64),
}

impl LegacyShard for ToImage {
  fn registerName() -> &'static str {
    cstr!("SVG.ToImage")
  }
  fn hash() -> u32 {
    compile_time_crc32::crc32!("SVG.ToImage-rust-0x20200101")
  }
  fn name(&mut self) -> &str {
    "SVG.ToImage"
  }
  fn inputTypes(&mut self) -> &std::vec::Vec<Type> {
    &INPUT_TYPES
  }
  fn outputTypes(&mut self) -> &std::vec::Vec<Type> {
    &IMAGE_TYPES
  }
  fn parameters(&mut self) -> Option<&Parameters> {
    Some(&PARAMETERS)
  }
  fn setParam(&mut self, index: i32, value: &Var) -> Result<(), &str> {
    match index {
      0 => Ok(self.size = value.try_into()?),
      _ => unreachable!(),
    }
  }
  fn getParam(&mut self, index: i32) -> Var {
    match index {
      0 => self.size.into(),
      _ => unreachable!(),
    }
  }
  fn activate(&mut self, _: &Context, input: &Var) -> Result<Var, &str> {
    let mut opt = usvg::Options::default();
    // Get file's absolute directory.
    // opt.resources_dir = std::fs::canonicalize(&args[1]).ok().and_then(|p| p.parent().map(|p| p.to_path_buf()));
    opt.fontdb.load_system_fonts();
    // opt.fontdb.set_generic_families();

    let opt = opt.to_ref();

    let rtree = match input.valueType {
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

    let (w, h): (u32, u32) = (
      self.size.0.try_into().map_err(|e| {
        shlog!("{}", e);
        "Invalid width"
      })?,
      self.size.1.try_into().map_err(|e| {
        shlog!("{}", e);
        "Invalid height"
      })?,
    );

    let pixmap_size = if w == 0 && h == 0 {
      Ok(rtree.svg_node().size.to_screen_size())
    } else {
      ScreenSize::new(w, h).ok_or("Invalid size")
    }?;

    if self.pixmap.is_none() {
      self.pixmap = Some(
        tiny_skia::Pixmap::new(pixmap_size.width(), pixmap_size.height())
          .ok_or("Failed to create pixmap")?,
      );
    } else {
      let pm = self.pixmap.as_ref().unwrap();
      if pixmap_size.width() != pm.width() || pixmap_size.height() != pm.height() {
        self.pixmap = Some(
          tiny_skia::Pixmap::new(pixmap_size.width(), pixmap_size.height())
            .ok_or("Failed to create pixmap")?,
        );
      }
    }

    resvg::render(
      &rtree,
      usvg::FitTo::Size(pixmap_size.width(), pixmap_size.height()),
      tiny_skia::Transform::default(),
      self.pixmap.as_mut().unwrap().as_mut(),
    )
    .ok_or("Failed to render SVG")?;

    Ok(pixmap_to_var(self.pixmap.as_mut().unwrap()))
  }
}

#[no_mangle]
pub extern "C" fn shardsRegister_svg_svg(core: *mut shards::shardsc::SHCore) {
  unsafe {
    shards::core::Core = core;
  }

  registerShard::<ToImage>();
}