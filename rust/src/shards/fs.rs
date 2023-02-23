/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2023 Fragcolor Pte. Ltd. */

use crate::core::registerShard;
use crate::core::run_blocking;
use crate::core::BlockingShard;
use crate::shard::Shard;
use crate::types::common_type;
use crate::types::ClonedVar;
use crate::types::Context;
use crate::types::ExposedTypes;
use crate::types::ParamVar;
use crate::types::Parameters;
use crate::types::Seq;
use crate::types::Types;
use crate::types::Var;
use crate::types::BOOL_VAR_OR_NONE_SLICE;
use crate::types::NONE_TYPES;
use crate::types::STRING_TYPES;

lazy_static! {
  pub static ref STRINGS_VAR_OR_NONE_TYPES: Types = vec![
    common_type::strings,
    common_type::strings_var,
    common_type::none
  ];
  static ref FILEDIALOG_PARAMETERS: Parameters = vec![
    (
      cstr!("Filters"),
      shccstr!("To filter files based on extensions."),
      &STRINGS_VAR_OR_NONE_TYPES[..],
    )
      .into(),
    (
      cstr!("Folder"),
      shccstr!("To select a folder instead of a file."),
      BOOL_VAR_OR_NONE_SLICE,
    )
      .into(),
  ];
}

struct FileDialog {
  filters: ParamVar,
  folder: ParamVar,
  output: ClonedVar,
}

impl Default for FileDialog {
  fn default() -> Self {
    Self {
      filters: ParamVar::default(),
      folder: ParamVar::new(false.into()),
      output: ClonedVar::default(),
    }
  }
}

impl Shard for FileDialog {
  fn registerName() -> &'static str
  where
    Self: Sized,
  {
    cstr!("FS.FileDialog")
  }

  fn hash() -> u32
  where
    Self: Sized,
  {
    compile_time_crc32::crc32!("FS.FileDialog-rust-0x20200101")
  }

  fn name(&mut self) -> &str {
    "FS.FileDialog"
  }

  fn inputTypes(&mut self) -> &Types {
    &NONE_TYPES
  }

  fn outputTypes(&mut self) -> &Types {
    &STRING_TYPES
  }

  fn parameters(&mut self) -> Option<&Parameters> {
    Some(&FILEDIALOG_PARAMETERS)
  }

  fn setParam(&mut self, index: i32, value: &Var) -> Result<(), &str> {
    match index {
      0 => Ok(self.filters.set_param(value)),
      1 => Ok(self.folder.set_param(value)),
      _ => Err("Invalid parameter index"),
    }
  }

  fn getParam(&mut self, index: i32) -> Var {
    match index {
      0 => self.filters.get_param(),
      1 => self.folder.get_param(),
      _ => Var::default(),
    }
  }

  fn warmup(&mut self, ctx: &Context) -> Result<(), &str> {
    self.filters.warmup(ctx);
    self.folder.warmup(ctx);

    Ok(())
  }

  fn cleanup(&mut self) -> Result<(), &str> {
    self.folder.cleanup();
    self.filters.cleanup();

    Ok(())
  }

  fn activate(&mut self, context: &Context, input: &Var) -> Result<Var, &str> {
    Ok(run_blocking(self, context, input))
  }
}

impl BlockingShard for FileDialog {
  #[cfg(any(target_os = "windows", target_os = "macos", target_os = "linux"))]
  fn activate_blocking(&mut self, _context: &Context, _input: &Var) -> Result<Var, &str> {
    let mut dialog = rfd::FileDialog::new();
    let folder: bool = self.folder.get().try_into()?;
    let filters = self.filters.get();
    if !folder && !filters.is_none() {
      let filters: Seq = filters.try_into()?;
      for filter in filters.iter() {
        let filter: &str = filter.try_into()?;
        dialog = dialog.add_filter(filter, &[filter]);
      }
    }
    let path = if folder {
      dialog.pick_folder()
    } else {
      dialog.pick_file()
    };
    if let Some(path) = path {
      let path = path.display().to_string();
      let output = Var::ephemeral_string(path.as_str());
      self.output = output.as_ref().into();
      Ok(self.output.0)
    } else {
      Err("Operation was cancelled")
    }
  }

  #[cfg(not(any(target_os = "windows", target_os = "macos", target_os = "linux")))]
  fn activate_blocking(&mut self, _context: &Context, _input: &Var) -> Result<Var, &str> {
    Ok(Var::default())
  }
}

pub fn registerShards() {
  registerShard::<FileDialog>();
}
