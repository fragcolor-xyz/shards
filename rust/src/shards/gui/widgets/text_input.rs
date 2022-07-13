/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2022 Fragcolor Pte. Ltd. */

use super::TextInput;
use crate::core::cloneVar;
use crate::shard::Shard;
use crate::shards::gui::util;
use crate::shards::gui::EGUI_UI_SEQ_TYPE;
use crate::shards::gui::PARENTS_UI_NAME;
use crate::shards::gui::STRING_VAR_SLICE;
use crate::shardsc;
use crate::types::common_type;
use crate::types::Context;
use crate::types::ExposedInfo;
use crate::types::ExposedTypes;
use crate::types::InstanceData;
use crate::types::ParamVar;
use crate::types::Parameters;
use crate::types::Type;
use crate::types::Types;
use crate::types::Var;
use crate::types::NONE_TYPES;
use crate::types::STRING_TYPES;
use std::cmp::Ordering;
use std::ffi::CStr;

lazy_static! {
  static ref TEXTINPUT_PARAMETERS: Parameters = vec![(
    cstr!("Variable"),
    cstr!("The variable that holds the input value."),
    STRING_VAR_SLICE,
  )
    .into(),];
}

impl Default for TextInput {
  fn default() -> Self {
    let mut parents = ParamVar::default();
    parents.set_name(PARENTS_UI_NAME);
    Self {
      parents,
      requiring: Vec::new(),
      variable: ParamVar::default(),
      exposing: Vec::new(),
      should_expose: false,
      mutable_text: true,
    }
  }
}

enum VarTextBuffer<'a> {
  Editable(&'a mut Var),
  ReadOnly(&'a Var),
}

impl AsRef<str> for VarTextBuffer<'_> {
  fn as_ref(&self) -> &str {

    let var = match self {
      VarTextBuffer::Editable(var) => var.as_ref(),
      VarTextBuffer::ReadOnly(var) => var,
    };

    VarTextBuffer::as_str(var)
  }
}

impl egui::TextBuffer for VarTextBuffer<'_> {
  fn is_mutable(&self) -> bool {
    matches!(*self, VarTextBuffer::Editable(_))
  }

  fn insert_text(&mut self, text: &str, char_index: usize) -> usize {
    let byte_idx = if let VarTextBuffer::Editable(var) = self {
      if !var.is_string() {
        0usize
      } else {
        self.byte_index_from_char_index(char_index)
      }
    } else {
      0usize
    };

    if let VarTextBuffer::Editable(var) = self {
      let text_len = text.len();
      let (current_len, current_cap) = unsafe {
        (
          var.payload.__bindgen_anon_1.__bindgen_anon_2.stringLen as usize,
          var.payload.__bindgen_anon_1.__bindgen_anon_2.stringCapacity as usize,
        )
      };

      if current_cap == 0usize {
        // new string
        let tmp = Var::ephemeral_string(text);
        cloneVar(var, &tmp);
      } else if (current_cap - current_len) >= text_len {
        // text can fit within existing capacity
        unsafe {
          let base_ptr =
            var.payload.__bindgen_anon_1.__bindgen_anon_2.stringValue as *mut std::os::raw::c_char;
          // move the rest of the string to the end
          std::ptr::copy(
            base_ptr.add(byte_idx),
            base_ptr.add(byte_idx).add(text_len),
            current_len - byte_idx,
          );
          // insert the new text
          let bytes = text.as_ptr() as *const std::os::raw::c_char;
          std::ptr::copy_nonoverlapping(bytes, base_ptr.add(byte_idx), text_len);
          // update the length
          let new_len = current_len + text_len;
          var.payload.__bindgen_anon_1.__bindgen_anon_2.stringLen = new_len as u32;
          // fixup null-terminator
          *base_ptr.add(new_len) = 0;
        }
      } else {
        let mut str = String::from(VarTextBuffer::as_str_from_mut(var));
        str.insert_str(byte_idx, text);
        let tmp = Var::ephemeral_string(str.as_str());
        cloneVar(var, &tmp);
      }

      text.chars().count()
    } else {
      0usize
    }
  }

  fn delete_char_range(&mut self, char_range: std::ops::Range<usize>) {
    assert!(char_range.start <= char_range.end);

    let byte_start = self.byte_index_from_char_index(char_range.start);
    let byte_end = self.byte_index_from_char_index(char_range.end);

    if byte_start == byte_end {
      // nothing to do
      return;
    }

    if let VarTextBuffer::Editable(var) = self {
      unsafe {
        let current_len = var.payload.__bindgen_anon_1.__bindgen_anon_2.stringLen as usize;
        let base_ptr =
          var.payload.__bindgen_anon_1.__bindgen_anon_2.stringValue as *mut std::os::raw::c_char;
        // move rest of the text at the deletion location
        std::ptr::copy(
          base_ptr.add(byte_end),
          base_ptr.add(byte_start),
          current_len - byte_end,
        );
        // update the length
        let new_len = current_len - byte_end + byte_start;
        var.payload.__bindgen_anon_1.__bindgen_anon_2.stringLen = new_len as u32;
        // fixup null-terminator
        *base_ptr.add(new_len) = 0;
      }
    }
  }
}

impl VarTextBuffer<'_> {
  fn as_str_from_mut(var: &mut Var) -> &str {
    if var.valueType != shardsc::SHType_String
      && var.valueType != shardsc::SHType_Path
      && var.valueType != shardsc::SHType_ContextVar
      && var.valueType != shardsc::SHType_None
    {
      panic!("Expected None, String, Path or ContextVar variable, but casting failed.")
    }

    if var.valueType == shardsc::SHType_None {
      return "";
    }

    unsafe {
      CStr::from_ptr(
        var.payload.__bindgen_anon_1.__bindgen_anon_2.stringValue as *mut std::os::raw::c_char,
      )
      .to_str()
      .unwrap()
    }
  }

  fn as_str(var: &Var) -> &str {
    if var.valueType != shardsc::SHType_String
      && var.valueType != shardsc::SHType_Path
      && var.valueType != shardsc::SHType_ContextVar
      && var.valueType != shardsc::SHType_None
    {
      panic!("Expected None, String, Path or ContextVar variable, but casting failed.")
    }

    if var.valueType == shardsc::SHType_None {
      return "";
    }

    unsafe {
      CStr::from_ptr(
        var.payload.__bindgen_anon_1.__bindgen_anon_2.stringValue as *mut std::os::raw::c_char,
      )
      .to_str()
      .unwrap()
    }
  }
}

impl Shard for TextInput {
  fn registerName() -> &'static str
  where
    Self: Sized,
  {
    cstr!("UI.TextInput")
  }

  fn hash() -> u32
  where
    Self: Sized,
  {
    compile_time_crc32::crc32!("UI.TextInput-rust-0x20200101")
  }

  fn name(&mut self) -> &str {
    "UI.TextInput"
  }

  fn inputTypes(&mut self) -> &Types {
    &NONE_TYPES
  }

  fn outputTypes(&mut self) -> &Types {
    &STRING_TYPES
  }

  fn parameters(&mut self) -> Option<&Parameters> {
    Some(&TEXTINPUT_PARAMETERS)
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
          self.mutable_text = var.isMutable;
          if var.exposedType.basicType != shardsc::SHType_String {
            return Err("TextInput: string variable required.");
          }
          break;
        }
      }
    } else {
      self.mutable_text = false;
    }

    Ok(common_type::string)
  }

  fn exposedVariables(&mut self) -> Option<&ExposedTypes> {
    if self.variable.is_variable() && self.should_expose {
      self.exposing.clear();

      let exp_info = ExposedInfo {
        exposedType: common_type::string,
        name: self.variable.get_name(),
        help: cstr!("The exposed string variable").into(),
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

    Ok(())
  }

  fn cleanup(&mut self) -> Result<(), &str> {
    self.variable.cleanup();
    self.parents.cleanup();

    Ok(())
  }

  fn activate(&mut self, _context: &Context, _input: &Var) -> Result<Var, &str> {
    if let Some(ui) = util::get_current_parent(*self.parents.get())? {
      let text = &mut if self.mutable_text {
        VarTextBuffer::Editable(self.variable.get_mut())
      } else {
        VarTextBuffer::ReadOnly(self.variable.get())
      };
      let text_edit = egui::TextEdit::singleline(text);
      let response = ui.add(text_edit);

      if response.changed() || response.lost_focus() {
        Ok(*self.variable.get())
      } else {
        Ok(Var::default())
      }
    } else {
      Err("No UI parent")
    }
  }
}
