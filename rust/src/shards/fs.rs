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
use crate::types::InstanceData;
use crate::types::ParamVar;
use crate::types::Parameters;
use crate::types::Seq;
use crate::types::Type;
use crate::types::Types;
use crate::types::Var;
use crate::types::BOOL_TYPES_SLICE;
use crate::types::BOOL_VAR_OR_NONE_SLICE;
use crate::types::NONE_TYPES;
use crate::types::STRING_TYPES;
use crate::types::STRING_VAR_OR_NONE_SLICE;

lazy_static! {
  pub static ref STRINGS_VAR_OR_NONE_TYPES: Types = vec![
    common_type::strings,
    common_type::strings_var,
    common_type::none
  ];
  pub static ref STRING_OR_STRINGS_TYPES: Types = vec![common_type::string, common_type::strings];
  static ref FILEDIALOG_PARAMETERS: Parameters = vec![
    (
      cstr!("Filters"),
      shccstr!("To filter files based on extensions."),
      &STRINGS_VAR_OR_NONE_TYPES[..],
    )
      .into(),
    (
      cstr!("CurrentDir"),
      shccstr!("Set the current directory"),
      STRING_VAR_OR_NONE_SLICE,
    )
      .into(),
    (
      cstr!("Multiple"),
      shccstr!("To select multiple files instead of just one."),
      BOOL_TYPES_SLICE,
    )
      .into(),
    (
      cstr!("Folder"),
      shccstr!("To select a folder instead of a file."),
      BOOL_VAR_OR_NONE_SLICE,
    )
      .into(),
  ];
  static ref SAVEFILEDIALOG_PARAMETERS: Parameters = vec![
    (
      cstr!("Filters"),
      shccstr!("To filter files based on extensions."),
      &STRINGS_VAR_OR_NONE_TYPES[..],
    )
      .into(),
    (
      cstr!("CurrentDir"),
      shccstr!("Set the current directory"),
      STRING_VAR_OR_NONE_SLICE,
    )
      .into(),
  ];
}

struct FileDialog {
  filters: ParamVar,
  current_dir: ParamVar,
  multiple: bool,
  folder: ParamVar,
  output: ClonedVar,
}

impl Default for FileDialog {
  fn default() -> Self {
    Self {
      filters: ParamVar::default(),
      current_dir: ParamVar::default(),
      multiple: false,
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
    &STRING_OR_STRINGS_TYPES
  }

  fn parameters(&mut self) -> Option<&Parameters> {
    Some(&FILEDIALOG_PARAMETERS)
  }

  fn setParam(&mut self, index: i32, value: &Var) -> Result<(), &str> {
    match index {
      0 => Ok(self.filters.set_param(value)),
      1 => Ok(self.current_dir.set_param(value)),
      2 => Ok(self.multiple = value.try_into()?),
      3 => Ok(self.folder.set_param(value)),
      _ => Err("Invalid parameter index"),
    }
  }

  fn getParam(&mut self, index: i32) -> Var {
    match index {
      0 => self.filters.get_param(),
      1 => self.current_dir.get_param(),
      2 => self.multiple.into(),
      3 => self.folder.get_param(),
      _ => Var::default(),
    }
  }

  fn hasCompose() -> bool {
    true
  }

  fn compose(&mut self, _data: &InstanceData) -> Result<Type, &str> {
    if self.multiple {
      Ok(common_type::strings)
    } else {
      Ok(common_type::string)
    }
  }

  fn warmup(&mut self, ctx: &Context) -> Result<(), &str> {
    self.filters.warmup(ctx);
    self.current_dir.warmup(ctx);
    self.folder.warmup(ctx);

    Ok(())
  }

  fn cleanup(&mut self) -> Result<(), &str> {
    self.folder.cleanup();
    self.current_dir.cleanup();
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
    let current_dir = self.current_dir.get();
    if !current_dir.is_none() {
      let cd: &str = current_dir.try_into()?;
      if let Ok(start_path) = std::path::PathBuf::from(cd).canonicalize() {
        dialog = dialog.set_directory(start_path);
      }
    }
    if self.multiple {
      self.pick_multiple(dialog, folder)
    } else {
      self.pick_single(dialog, folder)
    }
  }

  #[cfg(not(any(target_os = "windows", target_os = "macos", target_os = "linux")))]
  fn activate_blocking(&mut self, _context: &Context, _input: &Var) -> Result<Var, &str> {
    Err("Not supported")
  }
}

#[cfg(any(target_os = "windows", target_os = "macos", target_os = "linux"))]
impl FileDialog {
  fn pick_single(&mut self, dialog: rfd::FileDialog, folder: bool) -> Result<Var, &str> {
    let path = if folder {
      dialog.pick_folder()
    } else {
      dialog.pick_file()
    };

    if let Some(path) = path {
      let path = path.display().to_string();
      self.output.assign(&Var::ephemeral_string(path.as_str()));
      Ok(self.output.0)
    } else {
      Err("Operation was cancelled")
    }
  }

  fn pick_multiple(&mut self, dialog: rfd::FileDialog, folder: bool) -> Result<Var, &str> {
    let paths = if folder {
      dialog.pick_folders()
    } else {
      dialog.pick_files()
    };

    if let Some(paths) = paths {
      let paths: Vec<Var> = paths
        .iter()
        .map(|path| {
          let path = path.display().to_string();
          Var::ephemeral_string(path.as_str())
        })
        .collect();
      let output: Var = paths.as_slice().into();
      self.output.assign(&output);
      Ok(self.output.0)
    } else {
      Err("Operation was cancelled")
    }
  }
}

struct SaveFileDialog {
  filters: ParamVar,
  current_dir: ParamVar,
  output: ClonedVar,
}

impl Default for SaveFileDialog {
  fn default() -> Self {
    Self {
      filters: ParamVar::default(),
      current_dir: ParamVar::default(),
      output: ClonedVar::default(),
    }
  }
}

impl Shard for SaveFileDialog {
  fn registerName() -> &'static str
  where
    Self: Sized,
  {
    cstr!("FS.SaveFileDialog")
  }

  fn hash() -> u32
  where
    Self: Sized,
  {
    compile_time_crc32::crc32!("FS.SaveFileDialog-rust-0x20200101")
  }

  fn name(&mut self) -> &str {
    "FS.SaveFileDialog"
  }

  fn inputTypes(&mut self) -> &Types {
    &NONE_TYPES
  }

  fn outputTypes(&mut self) -> &Types {
    &STRING_TYPES
  }

  fn parameters(&mut self) -> Option<&Parameters> {
    Some(&SAVEFILEDIALOG_PARAMETERS)
  }

  fn setParam(&mut self, index: i32, value: &Var) -> Result<(), &str> {
    match index {
      0 => Ok(self.filters.set_param(value)),
      1 => Ok(self.current_dir.set_param(value)),
      _ => Err("Invalid parameter index"),
    }
  }

  fn getParam(&mut self, index: i32) -> Var {
    match index {
      0 => self.filters.get_param(),
      1 => self.current_dir.get_param(),
      _ => Var::default(),
    }
  }

  fn warmup(&mut self, ctx: &Context) -> Result<(), &str> {
    self.filters.warmup(ctx);
    self.current_dir.warmup(ctx);

    Ok(())
  }

  fn cleanup(&mut self) -> Result<(), &str> {
    self.current_dir.cleanup();
    self.filters.cleanup();

    Ok(())
  }

  fn activate(&mut self, context: &Context, input: &Var) -> Result<Var, &str> {
    Ok(run_blocking(self, context, input))
  }
}

impl BlockingShard for SaveFileDialog {
  #[cfg(any(target_os = "windows", target_os = "macos", target_os = "linux"))]
  fn activate_blocking(&mut self, _context: &Context, _input: &Var) -> Result<Var, &str> {
    let mut dialog = rfd::FileDialog::new();
    let filters = self.filters.get();
    if !filters.is_none() {
      let filters: Seq = filters.try_into()?;
      for filter in filters.iter() {
        let filter: &str = filter.try_into()?;
        dialog = dialog.add_filter(filter, &[filter]);
      }
    }
    let current_dir = self.current_dir.get();
    if !current_dir.is_none() {
      let cd: &str = current_dir.try_into()?;
      if let Ok(start_path) = std::path::PathBuf::from(cd).canonicalize() {
        dialog = dialog.set_directory(start_path);
      }
    }

    if let Some(path) = dialog.save_file() {
      let path = path.display().to_string();
      self.output.assign(&Var::ephemeral_string(path.as_str()));
      Ok(self.output.0)
    } else {
      Err("Operation was cancelled")
    }
  }

  #[cfg(not(any(target_os = "windows", target_os = "macos", target_os = "linux")))]
  fn activate_blocking(&mut self, _context: &Context, _input: &Var) -> Result<Var, &str> {
    Err("Not supported")
  }
}

pub fn registerShards() {
  registerShard::<FileDialog>();
  registerShard::<SaveFileDialog>();
}
