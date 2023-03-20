use std::ops::DerefMut;

use egui::Memory;
use lazy_static::__Deref;

use crate::core::registerShard;
use crate::shard::Shard;
use crate::shards::gui::util;
use crate::shards::gui::CONTEXTS_NAME;
use crate::shards::gui::EGUI_CTX_TYPE;
use crate::types::Context;
use crate::types::ExposedInfo;
use crate::types::ExposedTypes;
use crate::types::ParamVar;
use crate::types::Var;
use crate::types::BYTES_TYPES;
use crate::types::NONE_TYPES;

pub struct Restore {
  instance: ParamVar,
  requiring: ExposedTypes,
}

pub struct Save {
  data: Option<Vec<u8>>,
  instance: ParamVar,
  requiring: ExposedTypes,
}

impl Default for Save {
  fn default() -> Self {
    let mut ctx = ParamVar::default();
    ctx.set_name(CONTEXTS_NAME);
    Self {
      data: None,
      instance: ctx,
      requiring: Vec::new(),
    }
  }
}

impl Shard for Save {
  fn registerName() -> &'static str {
    cstr!("UI.SaveState")
  }

  fn hash() -> u32
  where
    Self: Sized,
  {
    compile_time_crc32::crc32!("UI.SaveState-rust-0x20200101")
  }

  fn name(&mut self) -> &str {
    "UI.SaveState"
  }

  fn requiredVariables(&mut self) -> Option<&ExposedTypes> {
    self.requiring.clear();

    let exp_info = ExposedInfo {
      exposedType: EGUI_CTX_TYPE,
      name: self.instance.get_name(),
      ..ExposedInfo::default()
    };
    self.requiring.push(exp_info);

    Some(&self.requiring)
  }

  fn inputTypes(&mut self) -> &crate::types::Types {
    &NONE_TYPES
  }

  fn outputTypes(&mut self) -> &crate::types::Types {
    &BYTES_TYPES
  }

  fn warmup(&mut self, ctx: &Context) -> Result<(), &str> {
    self.instance.warmup(ctx);
    Ok(())
  }

  fn cleanup(&mut self) -> Result<(), &str> {
    self.instance.cleanup();
    Ok(())
  }

  fn activate(
    &mut self,
    _context: &crate::types::Context,
    _input: &crate::types::Var,
  ) -> Result<crate::types::Var, &str> {
    let ctx = util::get_current_context(&self.instance)?;

    self.data = ctx.memory_mut(|mem| {
      Ok::<_, &str>(Some(
        rmp_serde::to_vec(mem.deref()).map_err(|_| "Failed to serialize UI state")?,
      ))
    })?;

    let data: Var = self.data.as_ref().unwrap().as_slice().into();
    Ok(data)
  }
}

impl Default for Restore {
  fn default() -> Self {
    let mut ctx = ParamVar::default();
    ctx.set_name(CONTEXTS_NAME);
    Self {
      instance: ctx,
      requiring: Vec::new(),
    }
  }
}

impl Shard for Restore {
  fn registerName() -> &'static str {
    cstr!("UI.RestoreState")
  }

  fn hash() -> u32
  where
    Self: Sized,
  {
    compile_time_crc32::crc32!("UI.RestoreState-rust-0x20200101")
  }

  fn name(&mut self) -> &str {
    "UI.RestoreState"
  }

  fn requiredVariables(&mut self) -> Option<&ExposedTypes> {
    self.requiring.clear();

    let exp_info = ExposedInfo {
      exposedType: EGUI_CTX_TYPE,
      name: self.instance.get_name(),
      ..ExposedInfo::default()
    };
    self.requiring.push(exp_info);

    Some(&self.requiring)
  }

  fn inputTypes(&mut self) -> &crate::types::Types {
    &BYTES_TYPES
  }

  fn outputTypes(&mut self) -> &crate::types::Types {
    &NONE_TYPES
  }

  fn warmup(&mut self, ctx: &Context) -> Result<(), &str> {
    self.instance.warmup(ctx);
    Ok(())
  }

  fn cleanup(&mut self) -> Result<(), &str> {
    self.instance.cleanup();
    Ok(())
  }

  fn activate(
    &mut self,
    _context: &crate::types::Context,
    input: &crate::types::Var,
  ) -> Result<crate::types::Var, &str> {
    let gui_ctx = util::get_current_context(&self.instance)?;

    let bytes: &[u8] = input.try_into()?;
    gui_ctx.memory_mut(|mem| {
      Ok::<_, &str>(
        *mem = rmp_serde::from_slice(bytes).map_err(|_| "Failed to deserialize UI state")?,
      )
    })?;

    Ok(Var::default())
  }
}

pub fn registerShards() {
  registerShard::<Save>();
  registerShard::<Restore>();
}
