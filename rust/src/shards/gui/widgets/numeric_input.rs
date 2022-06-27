/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2022 Fragcolor Pte. Ltd. */

use super::FloatInput;
use super::IntInput;
use crate::shard::Shard;
use crate::shards::gui::util;
use crate::shards::gui::EGUI_UI_SEQ_TYPE;
use crate::shards::gui::EGUI_UI_TYPE;
use crate::shards::gui::PARENTS_UI_NAME;
use crate::types::common_type;
use crate::types::Context;
use crate::types::ExposedInfo;
use crate::types::ExposedTypes;
use crate::types::InstanceData;
use crate::types::ParamVar;
use crate::types::Parameters;
use crate::types::Seq;
use crate::types::Type;
use crate::types::Types;
use crate::types::Var;
use crate::types::FLOAT_TYPES;
use crate::types::INT_TYPES;
use crate::types::NONE_TYPES;
use std::cmp::Ordering;
use std::ffi::CStr;

macro_rules! impl_ui_input {
  ($shard_name:ident, $name_str:literal, $hash:literal, $static:ident, $type:ident, $var_type:ident, $field:ident, $parameters:ident, $output_type:ident) => {
    static $static: &[Type] = &[common_type::$type, common_type::$var_type];

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

      fn inputTypes(&mut self) -> &Types {
        &NONE_TYPES
      }

      fn outputTypes(&mut self) -> &Types {
        &$output_type
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
            if CStr::cmp(&a, &b) == Ordering::Equal {
              self.should_expose = false;
              let t = common_type::$type;
              if var.exposedType.basicType != t.basicType {
                return Err(concat!($name_str, ": incorrect type of variable."));
              }
              break;
            }
          }
        }

        Ok(common_type::bool)
      }

      fn exposedVariables(&mut self) -> Option<&ExposedTypes> {
        if self.variable.is_variable() && self.should_expose {
          self.exposing.clear();

          let exp_info = ExposedInfo {
            exposedType: common_type::$type,
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
        self.variable.warmup(ctx);

        if self.should_expose {
          self.variable.get_mut().valueType = common_type::$type.basicType;
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
          let value = &mut if self.variable.is_variable() {
            unsafe { self.variable.get_mut().payload.__bindgen_anon_1.$field }
          } else {
            self.tmp
          };

          let drag = egui::DragValue::new(value);
          let response = ui.add(drag);
          if response.changed() || response.lost_focus() {
            if self.variable.is_variable() {
              self.variable.set((*value).into());
            } else {
              self.tmp = *value;
            }
          }

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
  intValue,
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
  floatValue,
  FLOAT_INPUT_PARAMETERS,
  FLOAT_TYPES
);
