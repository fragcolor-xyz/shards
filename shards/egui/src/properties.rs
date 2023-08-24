/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2022 Fragcolor Pte. Ltd. */

use crate::util;
use crate::CONTEXTS_NAME;
use crate::EGUI_CTX_TYPE;
use crate::HELP_VALUE_IGNORED;
use crate::PARENTS_UI_NAME;
use shards::core::registerEnumType;
use shards::core::registerShard;
use shards::fourCharacterCode;
use shards::shard::Shard;
use shards::types::common_type;
use shards::types::Context;
use shards::types::ExposedInfo;
use shards::types::ExposedTypes;
use shards::types::InstanceData;
use shards::types::OptionalString;
use shards::types::ParamVar;
use shards::types::Parameters;
use shards::types::Type;
use shards::types::Types;
use shards::types::Var;
use shards::types::FRAG_CC;
use shards::types::NONE_TYPES;
use shards::SHType_Enum;

shenum! {
  struct UIProperty {
    [description("Return the remaining space within an UI widget.")]
    const RemainingSpace = 0x0;
    [description("The screen size of the UI.")]
    const ScreenSize = 0x1;
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

    let (require_ui_parent) = match self.get_ui_property().unwrap() {
      UIProperty::RemainingSpace => true,
      _ => false,
    };

    if require_ui_parent {
      // Add UI.Parents to the list of required variables
      util::require_parents(&mut self.requiring, &self.parents);
    }

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
      UIProperty::ScreenSize => Ok(common_type::float2),
      _ => Err("Unknown property"),
    }
  }

  fn activate(&mut self, _context: &Context, _input: &Var) -> Result<Var, &str> {
    let ui = if (!self.parents.is_variable()) {
      util::get_current_parent(self.parents.get()).map_err(|_| "No parent UI")
    } else {
      Err("No parent UI")
    };

    match self.get_ui_property()? {
      UIProperty::RemainingSpace => {
        let ui = ui?.ok_or("No parent UI")?;
        let target_size = ui.available_size();
        let target_pos = ui.next_widget_position().to_vec2();

        let draw_scale = ui.ctx().pixels_per_point();

        let min = target_pos * draw_scale;
        let max = (target_pos + target_size) * draw_scale;

        // Float4 rect as (X0, Y0, X1, Y1)
        let result_rect: Var = (min.x, min.y, max.x, max.y).into();
        Ok(result_rect)
      }
      UIProperty::ScreenSize => {
        let ctx = util::get_current_context(&self.instance)?;
        let size = ctx.screen_rect().size();
        Ok((size.x, size.y).into())
      }
      _ => Err("Unknown property"),
    }
  }
}

pub fn registerShards() {
  registerEnumType(FRAG_CC, UIPROPERTY_CC, UIPropertyEnumInfo.as_ref().into());
  registerShard::<GetProperty>();
}
