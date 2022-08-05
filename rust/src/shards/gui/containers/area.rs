/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2022 Fragcolor Pte. Ltd. */

use super::Anchor;
use super::Area;
use crate::shard::Shard;
use crate::shards::gui::containers::ANCHOR_TYPES;
use crate::shards::gui::util;
use crate::shards::gui::EguiId;
use crate::shards::gui::CONTEXT_NAME;
use crate::shards::gui::EGUI_CTX_TYPE;
use crate::shards::gui::EGUI_UI_SEQ_TYPE;
use crate::shards::gui::PARENTS_UI_NAME;
use crate::types::Context;
use crate::types::ExposedInfo;
use crate::types::ExposedTypes;
use crate::types::InstanceData;
use crate::types::OptionalString;
use crate::types::ParamVar;
use crate::types::Parameters;
use crate::types::RawString;
use crate::types::ShardsVar;
use crate::types::Type;
use crate::types::Types;
use crate::types::Var;
use crate::types::ANY_TYPES;
use crate::types::FLOAT2_TYPES_SLICE;
use crate::types::SHARDS_OR_NONE_TYPES;
use crate::types::STRING_OR_NONE_SLICE;
use egui::Context as EguiNativeContext;

lazy_static! {
  static ref AREA_PARAMETERS: Parameters = vec![
    (
      cstr!("Position"),
      cstr!("Absolute position; or when anchor is set, relative offset."),
      FLOAT2_TYPES_SLICE,
    )
      .into(),
    (
      cstr!("Anchor"),
      cstr!("Corner or center of the screen."),
      &ANCHOR_TYPES[..],
    )
      .into(),
    (
      cstr!("Contents"),
      cstr!("The UI contents."),
      &SHARDS_OR_NONE_TYPES[..],
    )
      .into(),
  ];
}

impl Default for Area {
  fn default() -> Self {
    let mut ctx = ParamVar::default();
    ctx.set_name(CONTEXT_NAME);
    let mut parents = ParamVar::default();
    parents.set_name(PARENTS_UI_NAME);
    Self {
      instance: ctx,
      requiring: Vec::new(),
      position: ParamVar::new((0.0, 0.0).into()),
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
    OptionalString(shccstr!("The output of this shard will be its input."))
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

    // Add UI.Context to the list of required variables
    let exp_info = ExposedInfo {
      exposedType: EGUI_CTX_TYPE,
      name: self.instance.get_name(),
      help: cstr!("The exposed UI context.").into(),
      ..ExposedInfo::default()
    };
    self.requiring.push(exp_info);

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
    // we need to inject UI variable to the inner shards
    let mut data = *data;
    // clone shared into a new vector we can append things to
    let mut shared: ExposedTypes = data.shared.into();
    // append to shared ui vars
    let ui_info = ExposedInfo {
      exposedType: EGUI_UI_SEQ_TYPE,
      name: self.parents.get_name(),
      help: cstr!("The parent UI objects.").into(),
      isMutable: false,
      isProtected: true, // don't allow to be used in code/wires
      isTableEntry: false,
      global: false,
      isPushTable: false,
    };
    shared.push(ui_info);
    // update shared
    data.shared = (&shared).into();

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
    let gui_ctx = {
      let ctx_ptr: &mut EguiNativeContext =
        Var::from_object_ptr_mut_ref(*self.instance.get(), &EGUI_CTX_TYPE)?;
      &*ctx_ptr
    };

    let mut failed = false;
    if !self.contents.is_empty() {
      let area = egui::Area::new(EguiId::new(self, 0));
      let area = if self.anchor.get().is_none() {
        let pos: (f32, f32) = self.position.get().try_into()?;
        area.fixed_pos(pos)
      } else {
        let offset: (f32, f32) = self.position.get().try_into()?;
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
