/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2022 Fragcolor Pte. Ltd. */

use super::Anchor;
use super::Area;
use crate::containers::ANCHOR_TYPES;
use crate::util;
use crate::EguiId;
use crate::CONTEXTS_NAME;
use crate::EGUI_CTX_TYPE;
use crate::FLOAT2_VAR_SLICE;
use crate::HELP_OUTPUT_EQUAL_INPUT;
use crate::PARENTS_UI_NAME;
use shards::shard::Shard;
use shards::types::Context;
use shards::types::ExposedInfo;
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
use shards::types::SHARDS_OR_NONE_TYPES;


lazy_static! {
  static ref AREA_PARAMETERS: Parameters = vec![
    (
      cstr!("Position"),
      shccstr!("Absolute position; or when anchor is set, relative offset."),
      FLOAT2_VAR_SLICE,
    )
      .into(),
    (
      cstr!("Anchor"),
      shccstr!("Corner or center of the screen."),
      &ANCHOR_TYPES[..],
    )
      .into(),
    (
      cstr!("Contents"),
      shccstr!("The UI contents."),
      &SHARDS_OR_NONE_TYPES[..],
    )
      .into(),
  ];
}

impl Default for Area {
  fn default() -> Self {
    let mut ctx = ParamVar::default();
    ctx.set_name(CONTEXTS_NAME);
    let mut parents = ParamVar::default();
    parents.set_name(PARENTS_UI_NAME);
    Self {
      instance: ctx,
      requiring: Vec::new(),
      position: ParamVar::default(),
      anchor: ParamVar::default(),
      contents: ShardsVar::default(),
      parents,
      exposing: Vec::new(),
    }
  }
}

impl Shard for Area {
  fn registerName() -> &'static str
  where
    Self: Sized,
  {
    cstr!("UI.Area")
  }

  fn hash() -> u32
  where
    Self: Sized,
  {
    compile_time_crc32::crc32!("UI.Area-rust-0x20200101")
  }

  fn name(&mut self) -> &str {
    "UI.Area"
  }

  fn help(&mut self) -> OptionalString {
    OptionalString(shccstr!("Places UI element at a specific position."))
  }

  fn inputTypes(&mut self) -> &Types {
    &ANY_TYPES
  }

  fn inputHelp(&mut self) -> OptionalString {
    OptionalString(shccstr!(
      "The value that will be passed to the Contents shards of the area."
    ))
  }

  fn outputTypes(&mut self) -> &Types {
    &ANY_TYPES
  }

  fn outputHelp(&mut self) -> OptionalString {
    *HELP_OUTPUT_EQUAL_INPUT
  }

  fn parameters(&mut self) -> Option<&Parameters> {
    Some(&AREA_PARAMETERS)
  }

  fn setParam(&mut self, index: i32, value: &Var) -> Result<(), &str> {
    match index {
      0 => Ok(self.position.set_param(value)),
      1 => Ok(self.anchor.set_param(value)),
      2 => self.contents.set_param(value),
      _ => Err("Invalid parameter index"),
    }
  }

  fn getParam(&mut self, index: i32) -> Var {
    match index {
      0 => self.position.get_param(),
      1 => self.anchor.get_param(),
      2 => self.contents.get_param(),
      _ => Var::default(),
    }
  }

  fn requiredVariables(&mut self) -> Option<&ExposedTypes> {
    self.requiring.clear();

    // Add UI.Contexts to the list of required variables
    let exp_info = ExposedInfo {
      exposedType: EGUI_CTX_TYPE,
      name: self.instance.get_name(),
      help: cstr!("The exposed UI context.").into(),
      ..ExposedInfo::default()
    };
    self.requiring.push(exp_info);
    // Add UI.Parents to the list of required variables
    util::require_parents(&mut self.requiring, &self.parents);

    Some(&self.requiring)
  }

  fn exposedVariables(&mut self) -> Option<&ExposedTypes> {
    self.exposing.clear();

    if util::expose_contents_variables(&mut self.exposing, &self.contents) {
      Some(&self.exposing)
    } else {
      None
    }
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
    self.instance.warmup(ctx);
    self.position.warmup(ctx);
    self.anchor.warmup(ctx);
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
    self.anchor.cleanup();
    self.position.cleanup();
    self.instance.cleanup();

    Ok(())
  }

  fn activate(&mut self, context: &Context, input: &Var) -> Result<Var, &str> {
    let gui_ctx = util::get_current_context(&self.instance)?;

    let mut failed = false;
    if !self.contents.is_empty() {
      let area = egui::Area::new(EguiId::new(self, 0));
      let area = if self.anchor.get().is_none() {
        let position = self.position.get();
        if !position.is_none() {
          let pos: (f32, f32) = self.position.get().try_into()?;
          area.fixed_pos(pos)
        } else {
          area.movable(false)
        }
      } else {
        let offset: (f32, f32) = self.position.get().try_into().unwrap_or_default();
        area.anchor(
          Anchor {
            bits: unsafe {
              self
                .anchor
                .get()
                .payload
                .__bindgen_anon_1
                .__bindgen_anon_3
                .enumValue
            },
          }
          .try_into()?,
          offset,
        )
      };
      area.show(gui_ctx, |ui| {
        if util::activate_ui_contents(context, input, ui, &mut self.parents, &mut self.contents)
          .is_err()
        {
          failed = true;
        }
      });

      if failed {
        return Err("Failed to activate window contents");
      }
    }

    // Always passthrough the input
    Ok(*input)
  }
}

impl TryFrom<Anchor> for egui::Align2 {
  type Error = &'static str;

  fn try_from(value: Anchor) -> Result<Self, Self::Error> {
    match value {
      Anchor::TopLeft => Ok(egui::Align2::LEFT_TOP),
      Anchor::Left => Ok(egui::Align2::LEFT_CENTER),
      Anchor::BottomLeft => Ok(egui::Align2::LEFT_BOTTOM),
      Anchor::Top => Ok(egui::Align2::CENTER_TOP),
      Anchor::Center => Ok(egui::Align2::CENTER_CENTER),
      Anchor::Bottom => Ok(egui::Align2::CENTER_BOTTOM),
      Anchor::TopRight => Ok(egui::Align2::RIGHT_TOP),
      Anchor::Right => Ok(egui::Align2::RIGHT_CENTER),
      Anchor::BottomRight => Ok(egui::Align2::RIGHT_BOTTOM),
      _ => Err("Invalid value for Anchor"),
    }
  }
}
