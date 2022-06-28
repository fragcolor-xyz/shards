/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2022 Fragcolor Pte. Ltd. */

use super::FloatSlider;
use super::IntSlider;
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
use crate::types::STRING_OR_NONE_SLICE;
use std::cmp::Ordering;
use std::ffi::CStr;

macro_rules! impl_ui_slider {
  ($shard_name:ident, $name_str:literal, $hash:literal, $static:ident, $type:ident, $var_type:ident, $field:ident, $parameters:ident, $output_type:ident) => {
    static $static: &[Type] = &[common_type::$type, common_type::$var_type];

    lazy_static! {
      static ref $parameters: Parameters = vec![
        (
          cstr!("Label"),
          cstr!("The label for this widget."),
          STRING_OR_NONE_SLICE
        )
          .into(),
        (
          cstr!("Variable"),
          cstr!("The variable that holds the input value."),
          $static,
        )
          .into(),
        (cstr!("Min"), cstr!("The minimum value."), $static,).into(),
        (cstr!("Max"), cstr!("The maximum value."), $static,).into(),
      ];
    }

    impl Default for $shard_name {
      fn default() -> Self {
        let mut parents = ParamVar::default();
        parents.set_name(PARENTS_UI_NAME);
        Self {
          parents,
          requiring: Vec::new(),
          label: ParamVar::default(),
          variable: ParamVar::default(),
          min: ParamVar::default(),
          max: ParamVar::default(),
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
          0 => Ok(self.label.set_param(value)),
          1 => Ok(self.variable.set_param(value)),
          2 => Ok(self.min.set_param(value)),
          3 => Ok(self.max.set_param(value)),
          _ => Err("Invalid parameter index"),
        }
      }

      fn getParam(&mut self, index: i32) -> Var {
        match index {
          0 => self.label.get_param(),
          1 => self.variable.get_param(),
          2 => self.min.get_param(),
          3 => self.max.get_param(),
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
        self.label.warmup(ctx);
        self.variable.warmup(ctx);
        self.min.warmup(ctx);
        self.max.warmup(ctx);

        if self.should_expose {
          self.variable.get_mut().valueType = common_type::$type.basicType;
        }

        Ok(())
      }

      fn cleanup(&mut self) -> Result<(), &str> {
        self.max.cleanup();
        self.min.cleanup();
        self.variable.cleanup();
        self.label.cleanup();
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
          let max = self.max.get().try_into()?;
          let min = self.min.get().try_into()?;

          let mut slider = egui::Slider::new(value, min..=max);
          if !self.label.get().is_none() {
            let text: &str = self.label.get().try_into()?;
            slider = slider.text(text);
          }

          let response = ui.add(slider);
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

impl_ui_slider!(
  IntSlider,
  "UI.IntSlider",
  "UI.IntSlider-rust-0x20200101",
  INT_VAR_SLICE,
  int,
  int_var,
  intValue,
  INT_SLIDER_PARAMETERS,
  INT_TYPES
);

impl_ui_slider!(
  FloatSlider,
  "UI.FloatSlider",
  "UI.FloatSlider-rust-0x20200101",
  FLOAT_VAR_SLICE,
  float,
  float_var,
  floatValue,
  FLOAT_SLIDER_PARAMETERS,
  FLOAT_TYPES
);
