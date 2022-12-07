/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2022 Fragcolor Pte. Ltd. */

use crate::core::registerEnumType;
use crate::core::registerShard;
use crate::fourCharacterCode;
use crate::shard::Shard;
use crate::shards::gui::util;
use crate::shards::gui::CONTEXTS_NAME;
use crate::shards::gui::EGUI_CTX_TYPE;
use crate::shards::gui::HELP_VALUE_IGNORED;
use crate::shards::gui::PARENTS_UI_NAME;
use crate::types::common_type;
use crate::types::Context;
use crate::types::ExposedInfo;
use crate::types::ExposedTypes;
use crate::types::InstanceData;
use crate::types::OptionalString;
use crate::types::ParamVar;
use crate::types::Parameters;
use crate::types::Type;
use crate::types::Types;
use crate::types::Var;
use crate::types::FRAG_CC;
use crate::types::NONE_TYPES;
use crate::SHType_Enum;

shenum! {
  struct UIProperty {
    const RemainingSpace = 0x0;
  }
  struct UIPropertyInfo {}
}

shenum_types! {
  UIPropertyInfo,
  const UIPROPERTY_CC = fourCharacterCode(*b"ppty");
  static ref UIPropertyEnumInfo;
  static ref UIPROPERTY_TYPE: Type;
  static ref UIPROPERTY_TYPES: Vec<Type>;
  static ref SEQ_OF_UIPROPERTY: Type;
  static ref SEQ_OF_UIPROPERTY_TYPES: Vec<Type>;
}

lazy_static! {
  static ref PARAMETERS: Parameters = vec![(
    cstr!("Property"),
    shccstr!("Which property to read from the UI."),
    &UIPROPERTY_TYPES[..],
  )
    .into(),];
  static ref OUTPUT_TYPES: Types = vec![common_type::float4,];
}

pub struct GetProperty {
  instance: ParamVar,
  requiring: ExposedTypes,
  parents: ParamVar,
  property: Var,
}

impl Default for GetProperty {
  fn default() -> Self {
    let mut instance = ParamVar::default();
    instance.set_name(CONTEXTS_NAME);
    let mut parents = ParamVar::default();
    parents.set_name(PARENTS_UI_NAME);
    Self {
      instance,
      requiring: Vec::new(),
      parents,
      property: Var::default(),
    }
  }
}

impl GetProperty {
  fn get_ui_property(&self) -> Result<UIProperty, &str> {
    match self.property.valueType {
      SHType_Enum => Ok(UIProperty {
        bits: unsafe {
          self
            .property
            .payload
            .__bindgen_anon_1
            .__bindgen_anon_3
            .enumValue
        },
      }),
      _ => Err(":Property is required"),
    }
  }
}

impl Shard for GetProperty {
  fn registerName() -> &'static str
  where
    Self: Sized,
  {
    cstr!("UI.GetProperty")
  }

  fn hash() -> u32
  where
    Self: Sized,
  {
    compile_time_crc32::crc32!("UI.GetProperty-rust-0x20200101")
  }

  fn name(&mut self) -> &str {
    "UI.GetProperty"
  }

  fn help(&mut self) -> OptionalString {
    OptionalString(shccstr!(
      "Retrieves values from the current state of the UI."
    ))
  }

  fn inputTypes(&mut self) -> &Types {
    &NONE_TYPES
  }

  fn inputHelp(&mut self) -> OptionalString {
    *HELP_VALUE_IGNORED
  }

  fn outputTypes(&mut self) -> &Types {
    &OUTPUT_TYPES
  }

  fn outputHelp(&mut self) -> OptionalString {
    OptionalString(shccstr!("The value produced."))
  }

  fn parameters(&mut self) -> Option<&Parameters> {
    Some(&PARAMETERS)
  }

  fn setParam(&mut self, index: i32, value: &Var) -> Result<(), &str> {
    match index {
      0 => Ok(self.property = *value),
      _ => Err("Invalid parameter index"),
    }
  }

  fn getParam(&mut self, index: i32) -> Var {
    match index {
      0 => self.property,
      _ => Var::default(),
    }
  }

  fn requiredVariables(&mut self) -> Option<&ExposedTypes> {
    self.requiring.clear();

    // Add UI.Parents to the list of required variables
    util::require_parents(&mut self.requiring, &self.parents);

    let exp_info = ExposedInfo {
      exposedType: EGUI_CTX_TYPE,
      name: self.instance.get_name(),
      help: cstr!("The exposed UI context.").into(),
      ..ExposedInfo::default()
    };
    self.requiring.push(exp_info);

    Some(&self.requiring)
  }

  fn warmup(&mut self, ctx: &Context) -> Result<(), &str> {
    self.instance.warmup(ctx);
    self.parents.warmup(ctx);

    Ok(())
  }

  fn cleanup(&mut self) -> Result<(), &str> {
    self.parents.cleanup();
    self.instance.cleanup();

    Ok(())
  }

  fn hasCompose() -> bool {
    true
  }

  fn compose(&mut self, _data: &InstanceData) -> Result<Type, &str> {
    let prop = self.get_ui_property()?;
    match prop {
      UIProperty::RemainingSpace => Ok(common_type::float4),
      _ => Err("Unknown property"),
    }
  }

  fn activate(&mut self, _context: &Context, _input: &Var) -> Result<Var, &str> {
    if let Some(ui) = util::get_current_parent(*self.parents.get())? {
      match self.get_ui_property()? {
        UIProperty::RemainingSpace => {
          let target_size = ui.available_size();
          let target_pos = ui.next_widget_position().to_vec2();

          let draw_scale = ui.ctx().pixels_per_point();

          let min = target_pos * draw_scale;
          let max = (target_pos + target_size) * draw_scale;

          // Float4 rect as (X0, Y0, X1, Y1)
          let result_rect: Var = (min.x, min.y, max.x, max.y).into();
          Ok(result_rect)
        }
        _ => Err("Unknown property"),
      }
    } else {
      Err("No parent UI")
    }
  }
}

pub fn registerShards() {
  registerEnumType(FRAG_CC, UIPROPERTY_CC, UIPropertyEnumInfo.as_ref().into());
  registerShard::<GetProperty>();
}
