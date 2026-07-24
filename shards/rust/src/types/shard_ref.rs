/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2020 Fragcolor Pte. Ltd. */

//! Shard reference types and related functionality.

use super::*;
use crate::core::Core;
use crate::shardsc::{
  SHOptionalString, ShardPtr, SHType_Bytes, SHType_ContextVar, SHType_Path, SHType_Seq,
  SHType_String,
};
use crate::SHStringWithLen;
use std::ffi::CStr;
use std::os::raw::c_char;
use std::slice;

#[repr(transparent)] // force it same size of original
#[derive(Copy, Clone)]
pub struct ShardRef(pub ShardPtr);

pub struct AutoShardRef(pub ShardRef);

impl Drop for AutoShardRef {
  fn drop(&mut self) {
    self.0.destroy();
  }
}

impl AutoShardRef {
  pub fn create(name: &str, debug_info: Option<(u32, u32, u32)>) -> Option<Self> {
    unsafe {
      let ptr = (*Core).createShard.unwrap_unchecked()(SHStringWithLen {
        string: name.as_ptr() as *const c_char,
        len: name.len() as u64,
      });
      if ptr.is_null() {
        None
      } else {
        if let Some(debug_info) = debug_info {
          (*ptr).line = debug_info.0;
          (*ptr).column = debug_info.1;
          (*ptr).file = debug_info.2;
        }
        (*ptr).setup.unwrap_unchecked()(ptr);
        Some(AutoShardRef(ShardRef(ptr)))
      }
    }
  }
}

impl ShardRef {
  pub fn output_types(&self) -> &[Type] {
    unsafe {
      let info = (*self.0).outputTypes.unwrap_unchecked()(self.0);
      if info.len == 0 {
        return &[];
      }
      core::slice::from_raw_parts(info.elements, info.len as usize)
    }
  }

  pub fn input_types(&self) -> &[Type] {
    unsafe {
      let info = (*self.0).inputTypes.unwrap_unchecked()(self.0);
      if info.len == 0 {
        return &[];
      }
      core::slice::from_raw_parts(info.elements, info.len as usize)
    }
  }

  /// Resolve an SHOptionalString: prefer the inline string pointer, fall back
  /// to the crc-keyed registry (null e.g. in SH_STRIP_HELP_STRINGS builds).
  unsafe fn resolve_optional_string(help: SHOptionalString) -> Option<&'static str> {
    if !help.string.is_null() {
      return CStr::from_ptr(help.string as *const c_char).to_str().ok();
    }
    if help.crc != 0 {
      let c_str = (*Core).getCompressedString.unwrap_unchecked()(help.crc);
      if !c_str.is_null() {
        return CStr::from_ptr(c_str).to_str().ok();
      }
    }
    None
  }

  pub fn input_help(&self) -> Option<&str> {
    unsafe {
      let help = (*self.0).inputHelp.unwrap_unchecked()(self.0);
      Self::resolve_optional_string(help)
    }
  }

  pub fn output_help(&self) -> Option<&str> {
    unsafe {
      let help = (*self.0).outputHelp.unwrap_unchecked()(self.0);
      Self::resolve_optional_string(help)
    }
  }

  pub fn help(&self) -> Option<&str> {
    unsafe {
      let help = (*self.0).help.unwrap_unchecked()(self.0);
      Self::resolve_optional_string(help)
    }
  }

  pub fn name(&self) -> &str {
    unsafe {
      let c_name = (*self.0).name.unwrap_unchecked()(self.0);
      CStr::from_ptr(c_name).to_str().unwrap()
    }
  }

  pub fn destroy(&self) {
    unsafe {
      (*Core).releaseShard.unwrap_unchecked()(self.0);
    }
  }

  pub fn cleanup(&self, context: Option<&Context>) -> Result<(), &'static str> {
    unsafe {
      let result = (*self.0).cleanup.unwrap_unchecked()(
        self.0,
        if let Some(_ref) = context {
          &_ref as *const _ as *mut _
        } else {
          std::ptr::null_mut()
        },
      );
      if result.code == 0 {
        Ok(())
      } else {
        if result.message.len == 0 {
          return Err("Unknown error");
        }
        let cstr = std::str::from_utf8(slice::from_raw_parts(
          result.message.string as *const u8,
          result.message.len as usize,
        ));
        Err(cstr.unwrap())
      }
    }
  }

  pub fn warmup(&self, context: &Context) -> Result<(), &'static str> {
    unsafe {
      if (*self.0).warmup.is_some() {
        let result = (*self.0).warmup.unwrap_unchecked()(self.0, context as *const _ as *mut _);
        if result.code == 0 {
          Ok(())
        } else {
          if result.message.len == 0 {
            return Err("Unknown error");
          }
          let cstr = std::str::from_utf8(slice::from_raw_parts(
            result.message.string as *const u8,
            result.message.len as usize,
          ));
          Err(cstr.unwrap())
        }
      } else {
        Ok(())
      }
    }
  }

  pub fn parameters(&self) -> &[ParameterInfo] {
    unsafe {
      let params = (*self.0).parameters.unwrap_unchecked()(self.0);
      if params.len == 0 {
        return &[];
      } else {
        std::slice::from_raw_parts(params.elements, params.len as usize)
      }
    }
  }

  pub fn set_parameter(&self, index: i32, value: Var) -> Result<(), &'static str> {
    unsafe {
      let result = (*Core).validateSetParam.unwrap_unchecked()(self.0, index, &value);
      if result.code != 0 {
        Err(result.message.static_str())
      } else {
        let err = (*self.0).setParam.unwrap_unchecked()(self.0, index, &value);

        #[cfg(debug_assertions)]
        {
          // In debug mode, verify that the shard has properly copied the value
          let written_value = (*self.0).getParam.unwrap_unchecked()(self.0, index);
          match written_value.valueType {
            SHType_String | SHType_ContextVar | SHType_Bytes | SHType_Path => {
              let ptr = written_value
                .payload
                .__bindgen_anon_1
                .__bindgen_anon_2
                .stringValue;
              let ptr2 = value.payload.__bindgen_anon_1.__bindgen_anon_2.stringValue;
              assert_ne!(ptr, ptr2);
            }
            SHType_Seq => {
              let len = written_value.payload.__bindgen_anon_1.seqValue.len;
              let len2 = value.payload.__bindgen_anon_1.seqValue.len;
              assert_eq!(len, len2);
              if len > 0 {
                let ptr = written_value.payload.__bindgen_anon_1.seqValue.elements;
                let ptr2 = value.payload.__bindgen_anon_1.seqValue.elements;
                assert_ne!(ptr, ptr2);
              }
            }
            _ => {}
          }
        }

        if err.code != 0 {
          Err(err.message.static_str())
        } else {
          Ok(())
        }
      }
    }
  }

  pub fn get_parameter(&self, index: i32) -> Var {
    unsafe { (*self.0).getParam.unwrap_unchecked()(self.0, index) }
  }

  pub fn get_line_info(&self) -> (u32, u32, u32) {
    unsafe { ((*self.0).line, (*self.0).column, (*self.0).file) }
  }

  pub fn properties(&self) -> Option<Table> {
    if unsafe { (*self.0).properties.is_some() } {
      let sh_table_ptr = unsafe { (*self.0).properties.unwrap_unchecked()(self.0) };
      unsafe { Some(Table::from_sh_table(*sh_table_ptr)) }
    } else {
      None
    }
  }
}
