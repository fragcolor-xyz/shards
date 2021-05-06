use crate::block::Block;
use crate::chainblocksc::CBImage;
use crate::chainblocksc::CBVarPayload;
use crate::chainblocksc::CBVarPayload__bindgen_ty_1;
use crate::chainblocksc::{CBType_Bytes, CBType_Image, CBType_String};
use crate::core::do_blocking;
use crate::core::log;
use crate::core::registerBlock;
use crate::types::common_type;
use crate::types::ClonedVar;
use crate::types::Context;
use crate::types::ParamVar;
use crate::types::Parameters;
use crate::types::Seq;
use crate::types::Table;
use crate::types::Type;
use crate::CString;
use crate::Types;
use crate::Var;
use core::ptr::null_mut;
use std::convert::TryInto;
use tiny_skia::Pixmap;
use usvg::SystemFontDB;

impl From<&mut Pixmap> for Var {
  fn from(pmap: &mut Pixmap) -> Self {
    Var {
      valueType: CBType_Image,
      payload: CBVarPayload {
        __bindgen_anon_1: CBVarPayload__bindgen_ty_1 {
          imageValue: CBImage {
            width: pmap.width() as u16,
            height: pmap.height() as u16,
            channels: 4,
            flags: 0,
            data: pmap.as_mut().data_mut().as_mut_ptr(),
          },
        },
      },
      ..Default::default()
    }
  }
}

lazy_static! {
  static ref INPUT_TYPES: Vec<Type> = vec![common_type::string, common_type::bytes];
  static ref OUTPUT_TYPES: Vec<Type> = vec![common_type::image];
}

#[derive(Default)]
struct ToImage {
  pixmap: Option<Pixmap>,
}

impl Block for ToImage {
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
    &OUTPUT_TYPES
  }
  fn activate(&mut self, _: &Context, input: &Var) -> Result<Var, &str> {
    let mut opt = usvg::Options::default();
    // Get file's absolute directory.
    // opt.resources_dir = std::fs::canonicalize(&args[1]).ok().and_then(|p| p.parent().map(|p| p.to_path_buf()));
    opt.fontdb.load_system_fonts();
    opt.fontdb.set_generic_families();

    let rtree = match input.valueType {
      CBType_Bytes => usvg::Tree::from_data(input.try_into().unwrap(), &opt).map_err(|e| {
        cblog!("{}", e);
        "Failed parse SVG bytes"
      }),
      CBType_String => usvg::Tree::from_str(input.try_into().unwrap(), &opt).map_err(|e| {
        cblog!("{}", e);
        "Failed to parse SVG string"
      }),
      _ => Err("Invalid input type"),
    }?;

    let pixmap_size = rtree.svg_node().size.to_screen_size();

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
      usvg::FitTo::Original,
      self.pixmap.as_mut().unwrap().as_mut(),
    )
    .ok_or("Failed to render SVG")?;

    Ok(self.pixmap.as_mut().unwrap().into())
  }
}

pub fn registerBlocks() {
  registerBlock::<ToImage>();
}
