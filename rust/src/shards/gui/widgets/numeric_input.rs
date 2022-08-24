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
use crate::core::cloneVar;
use crate::shard::Shard;
use crate::shards::gui::util;
use crate::shards::gui::EGUI_UI_TYPE;
use crate::shards::gui::PARENTS_UI_NAME;
use crate::types::common_type;
use crate::types::Context;
use crate::types::ExposedInfo;
use crate::types::ExposedTypes;
use crate::types::InstanceData;
use crate::types::OptionalString;
use crate::types::ParamVar;
use crate::types::Parameters;
use crate::types::Seq;
use crate::types::Type;
use crate::types::Types;
use crate::types::Var;
use crate::types::FLOAT2_TYPES;
use crate::types::FLOAT3_TYPES;
use crate::types::FLOAT4_TYPES;
use crate::types::FLOAT_TYPES;
use crate::types::INT2_TYPES;
use crate::types::INT3_TYPES;
use crate::types::INT4_TYPES;
use crate::types::INT_TYPES;
use crate::types::NONE_TYPES;
use std::cmp::Ordering;
use std::ffi::CStr;

macro_rules! impl_ui_input {
  ($shard_name:ident, $name_str:literal, $hash:literal, $static:ident, $common_type:ident, $var_type:ident, $native_type:ident, $parameters:ident, $output_type:ident) => {
    static $static: &[Type] = &[common_type::$common_type, common_type::$var_type];

    lazy_static! {
      static ref $parameters: Parameters = vec![(
        cstr!("Variable"),
        cstr!("The variable that holds the input value."),
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

    impl Shard for $shard_name {
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
        OptionalString(shccstr!("The value is ignored."))
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
          0 => Ok(self.variable.set_param(value)),
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
            help: cstr!("The exposed variable").into(),
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
        util::require_parents(&mut self.requiring, &self.parents);

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

      fn cleanup(&mut self) -> Result<(), &str> {
        self.variable.cleanup();
        self.parents.cleanup();

        Ok(())
      }

      fn activate(&mut self, _context: &Context, _input: &Var) -> Result<Var, &str> {
        if let Some(ui) = util::get_current_parent(*self.parents.get())? {
          let value: &mut $native_type = if self.variable.is_variable() {
            self.variable.get_mut().try_into()?
          } else {
            &mut self.tmp
          };

          ui.add(egui::DragValue::new(value));

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
        cstr!("The variable that holds the input value."),
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

    impl Shard for $shard_name {
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
        OptionalString(shccstr!("The value is ignored."))
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
          0 => Ok(self.variable.set_param(value)),
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
            help: cstr!("The exposed variable").into(),
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
        util::require_parents(&mut self.requiring, &self.parents);

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

      fn cleanup(&mut self) -> Result<(), &str> {
        self.variable.cleanup();
        self.parents.cleanup();

        Ok(())
      }

      fn activate(&mut self, _context: &Context, _input: &Var) -> Result<Var, &str> {
        if let Some(ui) = util::get_current_parent(*self.parents.get())? {
          ui.horizontal(|ui| {
            let values: &mut [$native_type; $n] = if self.variable.is_variable() {
              self.variable.get_mut().try_into()?
            } else {
              &mut self.tmp
            };
            for i in 0..$n {
              ui.add(egui::DragValue::new(&mut values[i]));
            }

            Ok::<(), &str>(())
          });

          if self.variable.is_variable() {
            let mut var = Var::default();
            cloneVar(&mut var, self.variable.get());
            Ok(var)
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
