/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2022 Fragcolor Pte. Ltd. */

use super::CloseMenu;
use super::Menu;
use super::MenuBar;
use crate::shard::Shard;
use crate::shards::gui::util;
use crate::shards::gui::EGUI_UI_SEQ_TYPE;
use crate::shards::gui::PARENTS_UI_NAME;
use crate::types::common_type;
use crate::types::Context;
use crate::types::ExposedInfo;
use crate::types::ExposedTypes;
use crate::types::InstanceData;
use crate::types::ParamVar;
use crate::types::Parameters;
use crate::types::ShardsVar;
use crate::types::Type;
use crate::types::Types;
use crate::types::Var;
use crate::types::ANY_TYPES;
use crate::types::BOOL_TYPES;
use crate::types::SHARDS_OR_NONE_TYPES;
use crate::types::STRING_TYPES;

lazy_static! {
  static ref MENU_PARAMETERS: Parameters = vec![
    (
      cstr!("Title"),
      cstr!("The title of the menu."),
      &STRING_TYPES[..],
    )
      .into(),
    (
      cstr!("Contents"),
      cstr!("The UI contents."),
      &SHARDS_OR_NONE_TYPES[..],
    )
      .into(),
  ];
  static ref MENUBAR_PARAMETERS: Parameters = vec![(
    cstr!("Contents"),
    cstr!("The UI contents."),
    &SHARDS_OR_NONE_TYPES[..],
  )
    .into(),];
}

impl Default for CloseMenu {
  fn default() -> Self {
    let mut parents = ParamVar::default();
    parents.set_name(PARENTS_UI_NAME);
    Self {
      parents,
      requiring: Vec::new(),
    }
  }
}

impl Shard for CloseMenu {
  fn registerName() -> &'static str
  where
    Self: Sized,
  {
    cstr!("UI.CloseMenu")
  }

  fn hash() -> u32
  where
    Self: Sized,
  {
    compile_time_crc32::crc32!("UI.CloseMenu-rust-0x20200101")
  }

  fn name(&mut self) -> &str {
    "UI.CloseMenu"
  }

  fn inputTypes(&mut self) -> &Types {
    &ANY_TYPES
  }

  fn outputTypes(&mut self) -> &Types {
    &ANY_TYPES
  }

  fn requiredVariables(&mut self) -> Option<&ExposedTypes> {
    self.requiring.clear();

    // Add UI.Parents to the list of required variables
    let exp_info = ExposedInfo {
      exposedType: EGUI_UI_SEQ_TYPE,
      name: self.parents.get_name(),
      help: cstr!("The parent UI objects.").into(),
      ..ExposedInfo::default()
    };
    self.requiring.push(exp_info);

    Some(&self.requiring)
  }

  fn warmup(&mut self, ctx: &Context) -> Result<(), &str> {
    self.parents.warmup(ctx);
    Ok(())
  }

  fn cleanup(&mut self) -> Result<(), &str> {
    self.parents.cleanup();
    Ok(())
  }

  fn activate(&mut self, _context: &Context, input: &Var) -> Result<Var, &str> {
    if let Some(ui) = util::get_current_parent(*self.parents.get())? {
      ui.close_menu();
      Ok(*input)
    } else {
      Err("No UI parent")
    }
  }
}

impl Default for Menu {
  fn default() -> Self {
    let mut parents = ParamVar::default();
    parents.set_name(PARENTS_UI_NAME);
    Self {
      parents,
      requiring: Vec::new(),
      title: ParamVar::default(),
      contents: ShardsVar::default(),
    }
  }
}

impl Shard for Menu {
  fn registerName() -> &'static str
  where
    Self: Sized,
  {
    cstr!("UI.Menu")
  }

  fn hash() -> u32
  where
    Self: Sized,
  {
    compile_time_crc32::crc32!("UI.Menu-rust-0x20200101")
  }

  fn name(&mut self) -> &str {
    "UI.Menu"
  }

  fn inputTypes(&mut self) -> &Types {
    &ANY_TYPES
  }

  fn outputTypes(&mut self) -> &Types {
    &BOOL_TYPES
  }

  fn parameters(&mut self) -> Option<&Parameters> {
    Some(&MENU_PARAMETERS)
  }

  fn setParam(&mut self, index: i32, value: &Var) -> Result<(), &str> {
    match index {
      0 => Ok(self.title.set_param(value)),
      1 => self.contents.set_param(value),
      _ => Err("Invalid parameter index"),
    }
  }

  fn getParam(&mut self, index: i32) -> Var {
    match index {
      0 => self.title.get_param(),
      1 => self.contents.get_param(),
      _ => Var::default(),
    }
  }

  fn requiredVariables(&mut self) -> Option<&ExposedTypes> {
    self.requiring.clear();

    // Add UI.Parents to the list of required variables
    let exp_info = ExposedInfo {
      exposedType: EGUI_UI_SEQ_TYPE,
      name: self.parents.get_name(),
      help: cstr!("The parent UI objects.").into(),
      ..ExposedInfo::default()
    };
    self.requiring.push(exp_info);

    Some(&self.requiring)
  }

  fn hasCompose() -> bool {
    true
  }

  fn compose(&mut self, data: &InstanceData) -> Result<Type, &str> {
    if !self.contents.is_empty() {
      self.contents.compose(&data)?;
    }

    Ok(common_type::bool)
  }

  fn warmup(&mut self, ctx: &Context) -> Result<(), &str> {
    self.parents.warmup(ctx);
    self.title.warmup(ctx);
    if !self.contents.is_empty() {
      self.contents.warmup(ctx)?;
    }

    Ok(())
  }

  fn cleanup(&mut self) -> Result<(), &str> {
    if !self.contents.is_empty() {
      self.contents.cleanup();
    }
    self.title.cleanup();
    self.parents.cleanup();

    Ok(())
  }

  fn activate(&mut self, context: &Context, input: &Var) -> Result<Var, &str> {
    if self.contents.is_empty() {
      // no contents, same as inactive
      return Ok(false.into());
    }

    if let Some(ui) = util::get_current_parent(*self.parents.get())? {
      let title: &str = self.title.get().try_into()?;
      if let Some(output) = ui
        .menu_button(title, |ui| {
          util::activate_ui_contents(context, input, ui, &mut self.parents, &mut self.contents)
        })
        .inner
      {
        if output.is_err() {
          // propagate the error
          output
        } else {
          // menu is active
          Ok(true.into())
        }
      } else {
        // menu is inactive
        Ok(false.into())
      }
    } else {
      Err("No UI parent")
    }
  }
}

impl Default for MenuBar {
  fn default() -> Self {
    let mut parents = ParamVar::default();
    parents.set_name(PARENTS_UI_NAME);
    Self {
      parents,
      requiring: Vec::new(),
      contents: ShardsVar::default(),
    }
  }
}

impl Shard for MenuBar {
  fn registerName() -> &'static str
  where
    Self: Sized,
  {
    cstr!("UI.MenuBar")
  }

  fn hash() -> u32
  where
    Self: Sized,
  {
    compile_time_crc32::crc32!("UI.MenuBar-rust-0x20200101")
  }

  fn name(&mut self) -> &str {
    "UI.MenuBar"
  }

  fn inputTypes(&mut self) -> &Types {
    &ANY_TYPES
  }

  fn outputTypes(&mut self) -> &Types {
    &BOOL_TYPES
  }

  fn parameters(&mut self) -> Option<&Parameters> {
    Some(&MENUBAR_PARAMETERS)
  }

  fn setParam(&mut self, index: i32, value: &Var) -> Result<(), &str> {
    match index {
      0 => self.contents.set_param(value),
      _ => Err("Invalid parameter index"),
    }
  }

  fn getParam(&mut self, index: i32) -> Var {
    match index {
      0 => self.contents.get_param(),
      _ => Var::default(),
    }
  }

  fn requiredVariables(&mut self) -> Option<&ExposedTypes> {
    self.requiring.clear();

    // Add UI.Parents to the list of required variables
    let exp_info = ExposedInfo {
      exposedType: EGUI_UI_SEQ_TYPE,
      name: self.parents.get_name(),
      help: cstr!("The parent UI objects.").into(),
      ..ExposedInfo::default()
    };
    self.requiring.push(exp_info);

    Some(&self.requiring)
  }

  fn hasCompose() -> bool {
    true
  }

  fn compose(&mut self, data: &InstanceData) -> Result<Type, &str> {
    if !self.contents.is_empty() {
      self.contents.compose(&data)?;
    }

    Ok(common_type::bool)
  }

  fn warmup(&mut self, ctx: &Context) -> Result<(), &str> {
    self.parents.warmup(ctx);
    if !self.contents.is_empty() {
      self.contents.warmup(ctx)?;
    }

    Ok(())
  }

  fn cleanup(&mut self) -> Result<(), &str> {
    if !self.contents.is_empty() {
      self.contents.cleanup();
    }
    self.parents.cleanup();

    Ok(())
  }

  fn activate(&mut self, context: &Context, input: &Var) -> Result<Var, &str> {
    if self.contents.is_empty() {
      // no contents, same as inactive
      return Ok(false.into());
    }

    if let Some(ui) = util::get_current_parent(*self.parents.get())? {
      let output = egui::menu::bar(ui, |ui| {
        util::activate_ui_contents(context, input, ui, &mut self.parents, &mut self.contents)
      })
      .inner;
      if output.is_err() {
        // propagate the error
        output
      } else {
        // menubar is active
        Ok(true.into())
      }
    } else {
      Err("No UI parent")
    }
  }
}
