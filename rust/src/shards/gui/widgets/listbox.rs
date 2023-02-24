/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2022 Fragcolor Pte. Ltd. */

use super::ListBox;
use crate::shard::Shard;
use crate::shards::gui::util;
use crate::shards::gui::INT_VAR_OR_NONE_SLICE;
use crate::shards::gui::PARENTS_UI_NAME;
use crate::shardsc;
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
use crate::types::STRINGS_TYPES;
use crate::types::ANY_TYPES;
use std::cmp::Ordering;
use std::ffi::CStr;

lazy_static! {
  static ref LISTBOX_PARAMETERS: Parameters = vec![(
    cstr!("Index"),
    shccstr!("The index of the selected item."),
    INT_VAR_OR_NONE_SLICE,
  )
    .into(),];
}

impl Default for ListBox {
  fn default() -> Self {
    let mut parents = ParamVar::default();
    parents.set_name(PARENTS_UI_NAME);
    Self {
      parents,
      requiring: Vec::new(),
      index: ParamVar::default(),
      exposing: Vec::new(),
      should_expose: false,
      tmp: 0,
    }
  }
}

impl Shard for ListBox {
  fn registerName() -> &'static str
  where
    Self: Sized,
  {
    cstr!("UI.ListBox")
  }

  fn hash() -> u32
  where
    Self: Sized,
  {
    compile_time_crc32::crc32!("UI.ListBox-rust-0x20200101")
  }

  fn name(&mut self) -> &str {
    "UI.ListBox"
  }

  fn help(&mut self) -> OptionalString {
    OptionalString(shccstr!("A list selection."))
  }

  fn inputTypes(&mut self) -> &Types {
    &STRINGS_TYPES
  }

  fn inputHelp(&mut self) -> OptionalString {
    OptionalString(shccstr!("A sequence of values."))
  }

  fn outputTypes(&mut self) -> &Types {
    &ANY_TYPES
  }

  fn outputHelp(&mut self) -> OptionalString {
    OptionalString(shccstr!("The selected value."))
  }

  fn parameters(&mut self) -> Option<&Parameters> {
    Some(&LISTBOX_PARAMETERS)
  }

  fn setParam(&mut self, index: i32, value: &Var) -> Result<(), &str> {
    match index {
      0 => Ok(self.index.set_param(value)),
      _ => Err("Invalid parameter index"),
    }
  }

  fn getParam(&mut self, index: i32) -> Var {
    match index {
      0 => self.index.get_param(),
      _ => Var::default(),
    }
  }

  fn hasCompose() -> bool {
    true
  }

  fn compose(&mut self, data: &InstanceData) -> Result<Type, &str> {
    if self.index.is_variable() {
      self.should_expose = true; // assume we expose a new variable

      let shared: ExposedTypes = data.shared.into();
      for var in shared {
        let (a, b) = unsafe {
          (
            CStr::from_ptr(var.name),
            CStr::from_ptr(self.index.get_name()),
          )
        };
        if CStr::cmp(a, b) == Ordering::Equal {
          self.should_expose = false;
          if var.exposedType.basicType != shardsc::SHType_Int {
            return Err("Combo: int variable required.");
          }
          break;
        }
      }
    }

    Ok(common_type::any)
  }

  fn exposedVariables(&mut self) -> Option<&ExposedTypes> {
    if self.index.is_variable() && self.should_expose {
      self.exposing.clear();

      let exp_info = ExposedInfo {
        exposedType: common_type::int,
        name: self.index.get_name(),
        help: cstr!("The exposed int variable").into(),
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

    self.index.warmup(ctx);

    if self.should_expose {
      self.index.get_mut().valueType = common_type::int.basicType;
    }

    Ok(())
  }

  fn cleanup(&mut self) -> Result<(), &str> {
    self.index.cleanup();

    self.parents.cleanup();

    Ok(())
  }

  fn activate(&mut self, _context: &Context, input: &Var) -> Result<Var, &str> {
    if let Some(ui) = util::get_current_parent(self.parents.get())? {
      let index = &mut if self.index.is_variable() {
        let index: i64 = self.index.get().try_into()?;
        index as usize
      } else {
        self.tmp
      };
      let seq: Seq = input.try_into()?;

      let mut changed = false;
      ui.group(|ui| {
        for i in 0..seq.len() {
          let str: &str = (&seq[i]).try_into()?;
          if ui.selectable_label(i == *index, str.to_owned()).clicked() {
            *index = i;
            changed = true;
          }
        }
        Ok::<(), &str>(())
      })
      .inner?;

      if changed {
        if self.index.is_variable() {
          self.index.set_fast_unsafe(&(*index as i64).into());
        } else {
          self.tmp = *index;
        }
      }

      if seq.is_empty() {
        Ok(Var::default())
      } else {
        // this is fine because we don't own input and seq is just a view of it in this case
        Ok(seq[*index])
      }
    } else {
      Err("No UI parent")
    }
  }
}
