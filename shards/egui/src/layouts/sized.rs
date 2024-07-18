/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2022 Fragcolor Pte. Ltd. */

use crate::util;
use crate::HELP_OUTPUT_EQUAL_INPUT;
use crate::PARENTS_UI_NAME;
use egui::vec2;
use egui::Align;
use egui::Layout;
use egui::Ui;
use egui::Vec2;
use shards::shard::LegacyShard;
use shards::types::Context;
use shards::types::ExposedTypes;
use shards::types::InstanceData;
use shards::types::OptionalString;
use shards::types::ParamVar;
use shards::types::Parameters;
use shards::types::ShardsVar;
use shards::types::Type;
use shards::types::Types;
use shards::types::Var;
use shards::types::ANY_TYPES;
use shards::types::BOOL_TYPES;
use shards::types::FLOAT_TYPES;
use shards::types::SHARDS_OR_NONE_TYPES;

lazy_static! {
  static ref SIZED_PARAMETERS: Parameters = vec![
    (
      cstr!("Contents"),
      shccstr!("The UI contents."),
      &SHARDS_OR_NONE_TYPES[..],
    )
      .into(),
    (cstr!("Width"), shccstr!(""), &FLOAT_TYPES[..],).into(),
    (cstr!("Height"), shccstr!(""), &FLOAT_TYPES[..],).into(),
    (cstr!("FillWidth"), shccstr!(""), &BOOL_TYPES[..],).into(),
    (cstr!("FillHeight"), shccstr!(""), &BOOL_TYPES[..],).into(),
  ];
}

impl Default for super::Sized {
  fn default() -> Self {
    let mut parents = ParamVar::default();
    parents.set_name(PARENTS_UI_NAME);
    Self {
      parents,
      contents: ShardsVar::default(),
      requiring: Vec::new(),
      exposing: Vec::new(),
      width: (0.0).into(),
      height: (0.0).into(),
      fill_width: (false).into(),
      fill_height: (false).into(),
    }
  }
}

impl LegacyShard for super::Sized {
  fn registerName() -> &'static str
  where
    Self: Sized,
  {
    cstr!("UI.Sized")
  }

  fn hash() -> u32
  where
    Self: Sized,
  {
    compile_time_crc32::crc32!("UI.Sized-rust-0x20200101")
  }

  fn name(&mut self) -> &str {
    "UI.Sized"
  }

  fn help(&mut self) -> OptionalString {
    OptionalString(shccstr!("Layout the contents sizedly."))
  }

  fn inputTypes(&mut self) -> &Types {
    &ANY_TYPES
  }

  fn inputHelp(&mut self) -> OptionalString {
    OptionalString(shccstr!(
      "The value that will be passed to the Contents shards of the layout."
    ))
  }

  fn outputTypes(&mut self) -> &Types {
    &ANY_TYPES
  }

  fn outputHelp(&mut self) -> OptionalString {
    *HELP_OUTPUT_EQUAL_INPUT
  }

  fn exposedVariables(&mut self) -> Option<&ExposedTypes> {
    self.exposing.clear();

    if util::expose_contents_variables(&mut self.exposing, &self.contents) {
      Some(&self.exposing)
    } else {
      None
    }
  }

  fn requiredVariables(&mut self) -> Option<&ExposedTypes> {
    self.requiring.clear();
    util::require_parents(&mut self.requiring);
    Some(&self.requiring)
  }

  fn hasCompose() -> bool {
    true
  }

  fn compose(&mut self, data: &InstanceData) -> Result<Type, &str> {
    if !self.contents.is_empty() {
      self.contents.compose(data)?;
    }

    // Always passthrough the input
    Ok(data.inputType)
  }

  fn parameters(&mut self) -> Option<&Parameters> {
    Some(&SIZED_PARAMETERS)
  }

  fn setParam(&mut self, index: i32, value: &Var) -> Result<(), &str> {
    match index {
      0 => self.contents.set_param(value),
      1 => Ok(self.width = value.into()),
      2 => Ok(self.height = value.into()),
      3 => Ok(self.fill_width = value.into()),
      4 => Ok(self.fill_height = value.into()),
      _ => Err("Invalid parameter index"),
    }
  }

  fn getParam(&mut self, index: i32) -> Var {
    match index {
      0 => self.contents.get_param(),
      1 => self.width.0,
      2 => self.height.0,
      3 => self.fill_width.0,
      4 => self.fill_height.0,
      _ => Var::default(),
    }
  }

  fn warmup(&mut self, ctx: &Context) -> Result<(), &str> {
    self.parents.warmup(ctx);
    if !self.contents.is_empty() {
      self.contents.warmup(ctx)?;
    }

    Ok(())
  }

  fn activate(&mut self, context: &Context, input: &Var) -> Result<Var, &str> {
    if self.contents.is_empty() {
      return Ok(*input);
    }

    if let Some(ui) = util::get_current_parent_opt(self.parents.get())? {
      let mut max_size = vec2((&self.width.0).try_into()?, (&self.height.0).try_into()?);
      let min_size = ui.min_size();
      if max_size.x < min_size.x {
        max_size.x = min_size.x;
      }
      if max_size.y < min_size.y {
        max_size.y = min_size.y;
      }

      let mut sized: Vec2 = max_size;
      let available_size = ui.available_size();
      let fill_width: bool = (&self.fill_width.0).try_into()?;
      let fill_height: bool = (&self.fill_height.0).try_into()?;
      if fill_width {
        sized.x = available_size.x;
      }
      if fill_height {
        sized.y = available_size.y;
      }

      let mut resp: Result<Var, &str> = Err("undefined");
      ui.add_sized(sized, |ui: &mut Ui| {
        ui.set_min_size(vec2(0.0, 0.0));
        if max_size.x > 0.0 {
          ui.set_max_width(max_size.x);
        }
        if max_size.y > 0.0 {
          ui.set_max_height(max_size.y);
        }
        if fill_width {
          ui.set_width(available_size.x);
        } else {
          ui.set_width(max_size.x);
        }
        if fill_height {
          ui.set_height(available_size.y);
        } else {
          ui.set_height(max_size.y);
        }
        let response = ui
          .with_layout(
            ui.layout().clone(),
            // Layout::top_down(Align::Min).with_cross_justify(true),
            |ui| {
              resp =
                util::activate_ui_contents(context, input, ui, &mut self.parents, &self.contents);
            },
          )
          .response;
        ui.allocate_space(ui.available_size());
        response
      });

      resp
    } else {
      Err("No UI parent")
    }
  }

  fn cleanup(&mut self, ctx: Option<&Context>) -> Result<(), &str> {
    if !self.contents.is_empty() {
      self.contents.cleanup(ctx);
    }
    self.parents.cleanup(ctx);

    Ok(())
  }
}
