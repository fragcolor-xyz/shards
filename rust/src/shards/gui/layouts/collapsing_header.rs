/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2022 Fragcolor Pte. Ltd. */

use super::CollapsingHeader;
use crate::shard::Shard;
use crate::shards::gui::util;
use crate::shards::gui::EGUI_UI_SEQ_TYPE;
use crate::shards::gui::PARENTS_UI_NAME;
use crate::types::Context;
use crate::types::ExposedInfo;
use crate::types::ExposedTypes;
use crate::types::InstanceData;
use crate::types::OptionalString;
use crate::types::ParamVar;
use crate::types::Parameters;
use crate::types::ShardsVar;
use crate::types::Type;
use crate::types::Types;
use crate::types::Var;
use crate::types::ANY_TYPES;
use crate::types::BOOL_TYPES_SLICE;
use crate::types::SHARDS_OR_NONE_TYPES;
use crate::types::STRING_OR_NONE_SLICE;

lazy_static! {
  static ref COLLAPSING_PARAMETERS: Parameters = vec![
    (
      cstr!("Heading"),
      cstr!("The heading text for this collapsing header."),
      STRING_OR_NONE_SLICE,
    )
      .into(),
    (
      cstr!("Contents"),
      cstr!("The UI contents."),
      &SHARDS_OR_NONE_TYPES[..],
    )
      .into(),
    (
      cstr!("DefaultOpen"),
      cstr!("Whether the collapsing header is opened by default."),
      BOOL_TYPES_SLICE,
    )
      .into(),
  ];
}

impl Default for CollapsingHeader {
  fn default() -> Self {
    let mut parents = ParamVar::default();
    parents.set_name(PARENTS_UI_NAME);
    Self {
      parents,
      requiring: Vec::new(),
      heading: ParamVar::default(),
      contents: ShardsVar::default(),
      defaultOpen: ParamVar::new(false.into()),
    }
  }
}

impl Shard for CollapsingHeader {
  fn registerName() -> &'static str
  where
    Self: Sized,
  {
    cstr!("UI.Collapsing")
  }

  fn hash() -> u32
  where
    Self: Sized,
  {
    compile_time_crc32::crc32!("UI.Collapsing-rust-0x20200101")
  }

  fn name(&mut self) -> &str {
    "UI.Collapsing"
  }

  fn help(&mut self) -> OptionalString {
    OptionalString(shccstr!(
      "A header which can be collapsed/expanded, revealing a contained UI region."
    ))
  }

  fn inputTypes(&mut self) -> &Types {
    &ANY_TYPES
  }

  fn inputHelp(&mut self) -> OptionalString {
    OptionalString(shccstr!(
      "The value that will be passed to the Contents shards of the collapsing header."
    ))
  }

  fn outputTypes(&mut self) -> &Types {
    &ANY_TYPES
  }

  fn outputHelp(&mut self) -> OptionalString {
    OptionalString(shccstr!("The output of this shard will be its input."))
  }

  fn parameters(&mut self) -> Option<&Parameters> {
    Some(&COLLAPSING_PARAMETERS)
  }

  fn setParam(&mut self, index: i32, value: &Var) -> Result<(), &str> {
    match index {
      0 => Ok(self.heading.set_param(value)),
      1 => self.contents.set_param(value),
      2 => Ok(self.defaultOpen.set_param(value)),
      _ => Err("Invalid parameter index"),
    }
  }

  fn getParam(&mut self, index: i32) -> Var {
    match index {
      0 => self.heading.get_param(),
      1 => self.contents.get_param(),
      2 => self.defaultOpen.get_param(),
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

    // Always passthrough the input
    Ok(data.inputType)
  }

  fn warmup(&mut self, ctx: &Context) -> Result<(), &str> {
    self.parents.warmup(ctx);
    self.heading.warmup(ctx);
    if !self.contents.is_empty() {
      self.contents.warmup(ctx)?;
    }
    self.defaultOpen.warmup(ctx);

    Ok(())
  }

  fn cleanup(&mut self) -> Result<(), &str> {
    self.defaultOpen.cleanup();
    if !self.contents.is_empty() {
      self.contents.cleanup();
    }
    self.heading.cleanup();
    self.parents.cleanup();

    Ok(())
  }

  fn activate(&mut self, context: &Context, input: &Var) -> Result<Var, &str> {
    if self.contents.is_empty() {
      return Ok(*input);
    }

    if let Some(ui) = util::get_current_parent(*self.parents.get())? {
      let heading: &str = self.heading.get().try_into()?;
      let defaultOpen: bool = self.defaultOpen.get().try_into()?;
      let header = egui::CollapsingHeader::new(heading).default_open(defaultOpen);
      if let Some(result) = header
        .show(ui, |ui| {
          util::activate_ui_contents(context, input, ui, &mut self.parents, &mut self.contents)
        })
        .body_returned
      {
        match result {
          Err(err) => Err(err),
          Ok(_) => Ok(*input),
        }
      } else {
        Ok(*input)
      }
    } else {
      Err("No UI parent")
    }
  }
}
