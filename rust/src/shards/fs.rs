/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2023 Fragcolor Pte. Ltd. */

use crate::core::registerShard;
use crate::core::run_blocking;
use crate::core::BlockingShard;
use crate::shard::Shard;
use crate::types::ClonedVar;
use crate::types::Context;
use crate::types::ExposedTypes;
use crate::types::ParamVar;
use crate::types::Parameters;
use crate::types::Types;
use crate::types::Var;
use crate::types::BOOL_VAR_OR_NONE_SLICE;
use crate::types::NONE_TYPES;
use crate::types::STRING_TYPES;

lazy_static! {
  static ref FILEDIALOG_PARAMETERS: Parameters = vec![
    (
      cstr!("Folder"),
      shccstr!("To select a folder instead of a file."),
      BOOL_VAR_OR_NONE_SLICE,
    )
      .into(),
  ];
}

struct FileDialog {
  folder: ParamVar,
  output: ClonedVar,
}

impl Default for FileDialog {
  fn default() -> Self {
    Self {
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
      0 => Ok(self.folder.set_param(value)),
      _ => Err("Invalid parameter index"),
    }
  }

  fn getParam(&mut self, index: i32) -> Var {
    match index {
      0 => self.folder.get_param(),
      _ => Var::default(),
    }
  }

  fn warmup(&mut self, ctx: &Context) -> Result<(), &str> {
    self.folder.warmup(ctx);

    Ok(())
  }

  fn cleanup(&mut self) -> Result<(), &str> {
    self.folder.cleanup();

    Ok(())
  }

  fn activate(&mut self, context: &Context, input: &Var) -> Result<Var, &str> {
    Ok(run_blocking(self, context, input))
  }
}

impl BlockingShard for FileDialog {
  #[cfg(any(target_os = "windows", target_os = "macos", target_os = "linux"))]
  fn activate_blocking(&mut self, _context: &Context, _input: &Var) -> Result<Var, &str> {
    let path = if self.folder.get().try_into()? {
      rfd::FileDialog::new().pick_folder()
    } else {
      rfd::FileDialog::new().pick_file()
    };
    if let Some(path) = path {
      let path = path.display().to_string();
      let output = Var::ephemeral_string(path.as_str());
      self.output = output.as_ref().into();
      Ok(self.output.0)
    } else {
      // FIXME should we return an empty string instead?
      Ok(Var::default())
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
