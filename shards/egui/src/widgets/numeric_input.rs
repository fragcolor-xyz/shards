/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2022 Fragcolor Pte. Ltd. */

use super::Float2Input;
use super::Float3Input;
use super::Float4Input;
use super::FloatInput;
use super::Int2Input;
use super::Int3Input;
use super::Int4Input;
use super::IntInput;
use crate::util;
use crate::widgets::drag_value::CustomDragValue;

use crate::HELP_VALUE_IGNORED;
use crate::PARENTS_UI_NAME;
use shards::shard::LegacyShard;
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
use shards::types::FLOAT2_TYPES;
use shards::types::FLOAT3_TYPES;
use shards::types::FLOAT4_TYPES;
use shards::types::FLOAT_TYPES;
use shards::types::INT2_TYPES;
use shards::types::INT3_TYPES;
use shards::types::INT4_TYPES;
use shards::types::INT_TYPES;
use shards::types::NONE_TYPES;
use shards::types::STRING_OR_NONE_SLICE;
use std::cmp::Ordering;
use std::ffi::CStr;

macro_rules! impl_ui_input {
  ($shard_name:ident, $name_str:literal, $hash:literal, $static:ident, $common_type:ident, $var_type:ident, $native_type:ident, $parameters:ident, $output_type:ident) => {
    static $static: &[Type] = &[common_type::$common_type, common_type::$var_type];

    lazy_static! {
      static ref $parameters: Parameters = vec![
        (
          cstr!("Variable"),
          shccstr!("The variable that holds the input value."),
          $static,
        )
          .into(),
        (
          cstr!("Prefix"),
          shccstr!("Display a prefix before the number."),
          STRING_OR_NONE_SLICE,
        )
          .into(),
      ];
    }

    impl Default for $shard_name {
      fn default() -> Self {
        let mut parents = ParamVar::default();
        parents.set_name(PARENTS_UI_NAME);
        Self {
          parents,
          requiring: Vec::new(),
          variable: ParamVar::default(),
          prefix: ParamVar::default(),
          exposing: Vec::new(),
          should_expose: false,
          tmp: Default::default(),
        }
      }
    }

    impl LegacyShard for $shard_name {
      fn registerName() -> &'static str
      where
        Self: Sized,
      {
        cstr!($name_str)
      }

      fn hash() -> u32
      where
        Self: Sized,
      {
        compile_time_crc32::crc32!($hash)
      }

      fn name(&mut self) -> &str {
        $name_str
      }

      fn help(&mut self) -> OptionalString {
        OptionalString(shccstr!("A numeric input."))
      }

      fn inputTypes(&mut self) -> &Types {
        &NONE_TYPES
      }

      fn inputHelp(&mut self) -> OptionalString {
        *HELP_VALUE_IGNORED
      }

      fn outputTypes(&mut self) -> &Types {
        &$output_type
      }

      fn outputHelp(&mut self) -> OptionalString {
        OptionalString(shccstr!("The value produced."))
      }

      fn parameters(&mut self) -> Option<&Parameters> {
        Some(&$parameters)
      }

      fn setParam(&mut self, index: i32, value: &Var) -> Result<(), &str> {
        match index {
          0 => self.variable.set_param(value),
          1 => self.prefix.set_param(value),
          _ => Err("Invalid parameter index"),
        }
      }

      fn getParam(&mut self, index: i32) -> Var {
        match index {
          0 => self.variable.get_param(),
          1 => self.prefix.get_param(),
          _ => Var::default(),
        }
      }

      fn hasCompose() -> bool {
        true
      }

      fn compose(&mut self, data: &InstanceData) -> Result<Type, &str> {
        if self.variable.is_variable() {
          self.should_expose = true; // assume we expose a new variable

          let shared: ExposedTypes = data.shared.into();
          for var in shared {
            let (a, b) = unsafe {
              (
                CStr::from_ptr(var.name),
                CStr::from_ptr(self.variable.get_name()),
              )
            };
            if CStr::cmp(a, b) == Ordering::Equal {
              self.should_expose = false;
              let t = common_type::$common_type;
              if var.exposedType.basicType != t.basicType {
                return Err(concat!($name_str, ": incorrect type of variable."));
              }
              break;
            }
          }
        }

        Ok(common_type::$common_type)
      }

      fn exposedVariables(&mut self) -> Option<&ExposedTypes> {
        if self.variable.is_variable() && self.should_expose {
          self.exposing.clear();

          let exp_info = ExposedInfo {
            exposedType: common_type::$common_type,
            name: self.variable.get_name(),
            help: shccstr!("The exposed variable"),
            ..ExposedInfo::default()
          };

          self.exposing.push(exp_info);
          Some(&self.exposing)
        } else {
          None
        }
      }

      fn requiredVariables(&mut self) -> Option<&ExposedTypes> {
        self.requiring.clear();

        // Add UI.Parents to the list of required variables
        util::require_parents(&mut self.requiring);

        Some(&self.requiring)
      }

      fn warmup(&mut self, ctx: &Context) -> Result<(), &str> {
        self.parents.warmup(ctx);
        self.variable.warmup(ctx);
        self.prefix.warmup(ctx);

        if self.should_expose {
          self.variable.get_mut().valueType = common_type::$common_type.basicType;
        }

        Ok(())
      }

      fn cleanup(&mut self, ctx: Option<&Context>) -> Result<(), &str> {
        self.prefix.cleanup(ctx);
        self.variable.cleanup(ctx);
        self.parents.cleanup(ctx);

        Ok(())
      }

      fn activate(&mut self, _context: &Context, _input: &Var) -> Result<Var, &str> {
        if let Some(ui) = util::get_current_parent_opt(self.parents.get())? {
          let value: &mut $native_type = if self.variable.is_variable() {
            self.variable.get_mut().try_into()?
          } else {
            &mut self.tmp
          };

          let mut drag_value = CustomDragValue::new(value);
          let prefix = self.prefix.get();
          if !prefix.is_none() {
            let prefix: &str = prefix.try_into()?;
            drag_value.drag_value = drag_value.drag_value.prefix(prefix);
          }

          ui.add(drag_value);

          Ok((*value).into())
        } else {
          Err("No UI parent")
        }
      }
    }
  };
}

impl_ui_input!(
  IntInput,
  "UI.IntInput",
  "UI.IntInput-rust-0x20200101",
  INT_VAR_SLICE,
  int,
  int_var,
  i64,
  INT_INPUT_PARAMETERS,
  INT_TYPES
);

impl_ui_input!(
  FloatInput,
  "UI.FloatInput",
  "UI.FloatInput-rust-0x20200101",
  FLOAT_VAR_SLICE,
  float,
  float_var,
  f64,
  FLOAT_INPUT_PARAMETERS,
  FLOAT_TYPES
);

macro_rules! impl_ui_n_input {
  ($n:literal, $shard_name:ident, $name_str:literal, $hash:literal, $static:ident, $common_type:ident, $var_type:ident, $native_type:ident, $parameters:ident, $output_type:ident) => {
    static $static: &[Type] = &[common_type::$common_type, common_type::$var_type];

    lazy_static! {
      static ref $parameters: Parameters = vec![(
        cstr!("Variable"),
        shccstr!("The variable that holds the input value."),
        $static,
      )
        .into(),];
    }

    impl Default for $shard_name {
      fn default() -> Self {
        let mut parents = ParamVar::default();
        parents.set_name(PARENTS_UI_NAME);
        Self {
          parents,
          requiring: Vec::new(),
          variable: ParamVar::default(),
          exposing: Vec::new(),
          should_expose: false,
          tmp: Default::default(),
        }
      }
    }

    impl LegacyShard for $shard_name {
      fn registerName() -> &'static str
      where
        Self: Sized,
      {
        cstr!($name_str)
      }

      fn hash() -> u32
      where
        Self: Sized,
      {
        compile_time_crc32::crc32!($hash)
      }

      fn name(&mut self) -> &str {
        $name_str
      }

      fn help(&mut self) -> OptionalString {
        OptionalString(shccstr!("A numeric input."))
      }

      fn inputTypes(&mut self) -> &Types {
        &NONE_TYPES
      }

      fn inputHelp(&mut self) -> OptionalString {
        *HELP_VALUE_IGNORED
      }

      fn outputTypes(&mut self) -> &Types {
        &$output_type
      }

      fn outputHelp(&mut self) -> OptionalString {
        OptionalString(shccstr!("The value produced."))
      }

      fn parameters(&mut self) -> Option<&Parameters> {
        Some(&$parameters)
      }

      fn setParam(&mut self, index: i32, value: &Var) -> Result<(), &str> {
        match index {
          0 => self.variable.set_param(value),
          _ => Err("Invalid parameter index"),
        }
      }

      fn getParam(&mut self, index: i32) -> Var {
        match index {
          0 => self.variable.get_param(),
          _ => Var::default(),
        }
      }

      fn hasCompose() -> bool {
        true
      }

      fn compose(&mut self, data: &InstanceData) -> Result<Type, &str> {
        if self.variable.is_variable() {
          self.should_expose = true; // assume we expose a new variable

          let shared: ExposedTypes = data.shared.into();
          for var in shared {
            let (a, b) = unsafe {
              (
                CStr::from_ptr(var.name),
                CStr::from_ptr(self.variable.get_name()),
              )
            };
            if CStr::cmp(a, b) == Ordering::Equal {
              self.should_expose = false;
              let t = common_type::$common_type;
              if var.exposedType.basicType != t.basicType {
                return Err(concat!($name_str, ": incorrect type of variable."));
              }
              break;
            }
          }
        }

        Ok(common_type::$common_type)
      }

      fn exposedVariables(&mut self) -> Option<&ExposedTypes> {
        if self.variable.is_variable() && self.should_expose {
          self.exposing.clear();

          let exp_info = ExposedInfo {
            exposedType: common_type::$common_type,
            name: self.variable.get_name(),
            help: shccstr!("The exposed variable"),
            ..ExposedInfo::default()
          };

          self.exposing.push(exp_info);
          Some(&self.exposing)
        } else {
          None
        }
      }

      fn requiredVariables(&mut self) -> Option<&ExposedTypes> {
        self.requiring.clear();

        // Add UI.Parents to the list of required variables
        util::require_parents(&mut self.requiring);

        Some(&self.requiring)
      }

      fn warmup(&mut self, ctx: &Context) -> Result<(), &str> {
        self.parents.warmup(ctx);
        self.variable.warmup(ctx);

        if self.should_expose {
          self.variable.get_mut().valueType = common_type::$common_type.basicType;
        }

        Ok(())
      }

      fn cleanup(&mut self, ctx: Option<&Context>) -> Result<(), &str> {
        self.variable.cleanup(ctx);
        self.parents.cleanup(ctx);

        Ok(())
      }

      fn activate(&mut self, _context: &Context, _input: &Var) -> Result<Var, &str> {
        if let Some(ui) = util::get_current_parent_opt(self.parents.get())? {
          ui.horizontal(|ui| {
            let values: &mut [$native_type; $n] = if self.variable.is_variable() {
              self.variable.get_mut().try_into()?
            } else {
              &mut self.tmp
            };
            for i in 0..$n {
              ui.add(CustomDragValue::new(&mut values[i]));
            }

            Ok::<(), &str>(())
          });

          if self.variable.is_variable() {
            Ok(*self.variable.get())
          } else {
            Ok((&self.tmp).into())
          }
        } else {
          Err("No UI parent")
        }
      }
    }
  };
}

impl_ui_n_input!(
  2,
  Float2Input,
  "UI.Float2Input",
  "UI.Float2Input-rust-0x20200101",
  FLOAT2_VAR_SLICE,
  float2,
  float2_var,
  f64,
  FLOAT2_INPUT_PARAMETERS,
  FLOAT2_TYPES
);

impl_ui_n_input!(
  3,
  Float3Input,
  "UI.Float3Input",
  "UI.Float3Input-rust-0x20200101",
  FLOAT3_VAR_SLICE,
  float3,
  float3_var,
  f32,
  FLOAT3_INPUT_PARAMETERS,
  FLOAT3_TYPES
);

impl_ui_n_input!(
  4,
  Float4Input,
  "UI.Float4Input",
  "UI.Float4Input-rust-0x20200101",
  FLOAT4_VAR_SLICE,
  float4,
  float4_var,
  f32,
  FLOAT4_INPUT_PARAMETERS,
  FLOAT4_TYPES
);

impl_ui_n_input!(
  2,
  Int2Input,
  "UI.Int2Input",
  "UI.Int2Input-rust-0x20200101",
  INT2_VAR_SLICE,
  int2,
  int2_var,
  i64,
  INT2_INPUT_PARAMETERS,
  INT2_TYPES
);

impl_ui_n_input!(
  3,
  Int3Input,
  "UI.Int3Input",
  "UI.Int3Input-rust-0x20200101",
  INT3_VAR_SLICE,
  int3,
  int3_var,
  i32,
  INT3_INPUT_PARAMETERS,
  INT3_TYPES
);

impl_ui_n_input!(
  4,
  Int4Input,
  "UI.Int4Input",
  "UI.Int4Input-rust-0x20200101",
  INT4_VAR_SLICE,
  int4,
  int4_var,
  i32,
  INT4_INPUT_PARAMETERS,
  INT4_TYPES
);
