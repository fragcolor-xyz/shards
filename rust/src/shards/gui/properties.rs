/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2022 Fragcolor Pte. Ltd. */

use crate::core::registerEnumType;
use crate::core::registerShard;
use crate::fourCharacterCode;
use crate::shard::Shard;
use crate::shards::gui::util;
use crate::shards::gui::EguiId;
use crate::shards::gui::CONTEXT_NAME;
use crate::shards::gui::EGUI_CTX_TYPE;
use crate::shards::gui::EGUI_UI_SEQ_TYPE;
use crate::shards::gui::PARENTS_UI_NAME;
use crate::types::common_type;
use crate::types::common_type::float4;
use crate::types::Context;
use crate::types::ExposedInfo;
use crate::types::ExposedTypes;
use crate::types::InstanceData;
use crate::types::OptionalString;
use crate::types::ParamVar;
use crate::types::Parameters;
use crate::types::RawString;
use crate::types::ShardsVar;
use crate::types::Table;
use crate::types::Type;
use crate::types::Types;
use crate::types::Var;
use crate::types::ANY_TYPES;
use crate::types::FLOAT2_TYPES_SLICE;
use crate::types::FRAG_CC;
use crate::types::NONE_TYPES;
use crate::types::SHARDS_OR_NONE_TYPES;
use crate::types::STRING_OR_NONE_SLICE;
use crate::SHType_Enum;
use egui::Context as EguiNativeContext;
use egui::Image;

shenum! {
  struct UIProperty {
    const RemainingSpace = 0x0;
  }
  struct UIPropertyInfo {}
}

shenum_types! {
  UIPropertyInfo,
  const UIPropertyCC = fourCharacterCode(*b"ppty");
  static ref UIPropertyEnumInfo;
  static ref UIPROPERTY_TYPE: Type;
  static ref UIPROPERTY_TYPES: Vec<Type>;
  static ref SEQ_OF_UIPROPERTY: Type;
  static ref SEQ_OF_UIPROPERTY_TYPES: Vec<Type>;
}

const FULL_OUTPUT_KEYS: &[RawString] = &[shstr!("ScreenRegion")];
lazy_static! {
  static ref PARAMETERS: Parameters = vec![(
    cstr!("Property"),
    shccstr!("Which property to read from the UI"),
    &UIPROPERTY_TYPES[..],
  )
    .into(),];
  static ref TABLE_VALUE_TYPES: Vec<Type> = vec![common_type::float4,];
  static ref OUTPUT_TABLE: Types = vec![Type::table(FULL_OUTPUT_KEYS, &TABLE_VALUE_TYPES)];
  static ref OUTPUT_TYPES: Types = vec![common_type::float4, common_type::float2,];
}

pub struct GetProperty {
  requiring: ExposedTypes,
  parents: ParamVar,
  property: Var,
  resultTable: Table,
}

impl Default for GetProperty {
  fn default() -> Self {
    let mut parents = ParamVar::default();
    parents.set_name(PARENTS_UI_NAME);
    Self {
      requiring: Vec::new(),
      parents,
      property: Var::default(),
      resultTable: Table::new(),
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
    OptionalString(shccstr!("The value is ignored."))
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

  fn compose(&mut self, _data: &InstanceData) -> Result<Type, &str> {
    let prop = self.get_ui_property()?;
    match prop {
      UIProperty::RemainingSpace => Ok(common_type::float4),
      _ => Err("Unknown property"),
    }
  }
  fn hasCompose() -> bool {
    true
  }

  fn activate(&mut self, context: &Context, input: &Var) -> Result<Var, &str> {
    if let Some(ui) = util::get_current_parent(*self.parents.get())? {
      match self.get_ui_property()? {
        UIProperty::RemainingSpace => {
          let target_size = ui.available_size();
          let cursor = ui.cursor();

          // Float4 rect as (X0, Y0, X1, Y1)
          let result_rect: Var = (
            cursor.min.x,
            cursor.min.y,
            cursor.min.x + target_size.x,
            cursor.min.y + target_size.y,
          )
            .into();
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
  registerEnumType(FRAG_CC, UIPropertyCC, UIPropertyEnumInfo.as_ref().into());
  registerShard::<GetProperty>();
}
