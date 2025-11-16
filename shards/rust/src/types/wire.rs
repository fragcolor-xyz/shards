/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2020 Fragcolor Pte. Ltd. */

//! Wire types and related functionality.

use super::*;
use crate::core::Core;
use crate::shardsc::{
  SHBool, SHEnumInfo, SHSeq, SHWireInfo, SHWireRef, Shard, ShardPtr,
  SHWireState, SHWireState_Continue, SHWireState_Error, SHWireState_Rebase,
  SHWireState_Restart, SHWireState_Return, SHWireState_Stop,
};
use crate::{shlog_error, shlog_warn, SHObjectInfo, SHStringWithLen};
use std::ffi::{c_void, CStr};
use std::os::raw::c_char;
use std::slice;

#[repr(transparent)] // force it same size of original
#[derive(Copy, Clone)]
pub struct WireRef(pub SHWireRef);

pub struct Wire(pub WireRef);

impl Drop for Wire {
  fn drop(&mut self) {
    unsafe { (*Core).destroyWire.unwrap_unchecked()(self.0 .0) }
  }
}

pub enum EnumInfoId<'a> {
  Int(i64),
  VendorTypePair(i32, i32),
  String(&'a str),
}

pub fn get_enum_info(id: EnumInfoId) -> Option<&'static SHEnumInfo> {
  let id = match id {
    EnumInfoId::Int(id) => id,
    EnumInfoId::VendorTypePair(vendor, type_) => (vendor as i64) << 32 | type_ as i64,
    EnumInfoId::String(name) => {
      let name = SHStringWithLen {
        string: name.as_ptr() as *const c_char,
        len: name.len() as u64,
      };
      unsafe { (*Core).findEnumId.unwrap_unchecked()(name) }
    }
  };
  let vendor_id = ((id >> 32) & 0xFFFFFFFF) as i32;
  let enum_id = (id & 0xFFFFFFFF) as i32;
  let enum_info = unsafe { (*Core).findEnumInfo.unwrap_unchecked()(vendor_id, enum_id) };
  if enum_info.is_null() {
    None
  } else {
    Some(unsafe { &*enum_info })
  }
}

pub enum ObjectInfoId<'a> {
  Int(i64),
  VendorTypePair(i32, i32),
  String(&'a str),
}

pub fn find_object_type_id(name: &str) -> Option<i64> {
  let name = SHStringWithLen {
    string: name.as_ptr() as *const c_char,
    len: name.len() as u64,
  };
  let id = unsafe { (*Core).findObjectTypeId.unwrap_unchecked()(name) };
  if id == 0 {
    None
  } else {
    Some(id)
  }
}

pub fn find_object_type_vendor_type_pair(name: &str) -> Option<(i32, i32)> {
  let id = find_object_type_id(name)?;
  // Convert to unsigned first to ensure proper bit manipulation
  let unsigned_id = id as u64;
  Some((
    (unsigned_id >> 32) as i32,
    (unsigned_id & 0xFFFFFFFF) as i32,
  ))
}

pub fn get_object_info(id: ObjectInfoId) -> Option<&'static SHObjectInfo> {
  let id = match id {
    ObjectInfoId::Int(id) => id,
    ObjectInfoId::VendorTypePair(vendor, type_) => (vendor as i64) << 32 | type_ as i64,
    ObjectInfoId::String(name) => find_object_type_id(name).unwrap_or(0),
  };
  let vendor_id = ((id >> 32) & 0xFFFFFFFF) as i32;
  let type_id = (id & 0xFFFFFFFF) as i32;
  let object_info = unsafe { (*Core).findObjectInfo.unwrap_unchecked()(vendor_id, type_id) };
  if object_info.is_null() {
    None
  } else {
    Some(unsafe { &*object_info })
  }
}

impl Wire {
  pub fn new(name: &str) -> Self {
    let name = SHStringWithLen {
      string: name.as_ptr() as *const c_char,
      len: name.len() as u64,
    };
    Wire(WireRef(unsafe {
      (*Core).createWire.unwrap_unchecked()(name)
    }))
  }

  pub fn set_debug_id(self, id: u64) -> Self {
    unsafe { (*Core).setWireDebugId.unwrap_unchecked()(self.0 .0, id) };
    self
  }

  pub fn add_shard(&self, shard: ShardRef) {
    unsafe { (*Core).addShard.unwrap_unchecked()(self.0 .0, shard.0) }
  }

  pub fn set_looped(&self, looped: bool) {
    unsafe { (*Core).setWireLooped.unwrap_unchecked()(self.0 .0, looped) }
  }

  pub fn set_priority(&self, priority: i32) {
    unsafe { (*Core).setWirePriority.unwrap_unchecked()(self.0 .0, priority) }
  }

  pub fn set_traits(&self, traits: SHSeq) {
    unsafe { (*Core).setWireTraits.unwrap_unchecked()(self.0 .0, traits) }
  }

  pub fn set_unsafe(&self, unsafe_: bool) {
    unsafe { (*Core).setWireUnsafe.unwrap_unchecked()(self.0 .0, unsafe_) }
  }

  pub fn set_pure(&self, pure: bool) {
    unsafe { (*Core).setWirePure.unwrap_unchecked()(self.0 .0, pure) }
  }

  pub fn set_stack_size(&self, stack_size: usize) {
    unsafe { (*Core).setWireStackSize.unwrap_unchecked()(self.0 .0, stack_size as u64) }
  }

  pub fn stop(&self) -> ClonedVar {
    unsafe { ClonedVar((*Core).stopWire.unwrap_unchecked()(self.0 .0)) }
  }

  pub fn get_info(&self) -> SHWireInfo {
    unsafe { (*Core).getWireInfo.unwrap_unchecked()(self.0 .0) }
  }
}

unsafe extern "C" fn error_cb(
  errorShard: *const Shard,
  errorTxt: SHStringWithLen,
  nonfatalWarning: SHBool,
  userData: *mut c_void,
) {
  let shard_name = CStr::from_ptr((*errorShard).name.unwrap()(errorShard as *mut _));
  let msg = std::str::from_utf8(unsafe {
    if errorTxt.len == 0 {
      &[]
    } else {
      slice::from_raw_parts(errorTxt.string as *const u8, errorTxt.len as usize)
    }
  })
  .unwrap();
  if !nonfatalWarning {
    shlog_error!(
      "Fatal error: {} shard: {}",
      msg,
      shard_name.to_str().unwrap()
    );
    let failed = userData as *mut bool;
    *failed = true;
  } else {
    shlog_warn!("Warning: {} shard: {}", msg, shard_name.to_str().unwrap());
  }
}

#[derive(PartialEq, Eq)]
pub enum WireState {
  Continue,
  Return,
  Rebase,
  Restart,
  Stop,
  Error,
}

impl From<SHWireState> for WireState {
  fn from(state: SHWireState) -> Self {
    match state {
      SHWireState_Continue => WireState::Continue,
      SHWireState_Return => WireState::Return,
      SHWireState_Rebase => WireState::Rebase,
      SHWireState_Restart => WireState::Restart,
      SHWireState_Stop => WireState::Stop,
      SHWireState_Error => WireState::Error,
      _ => unreachable!(),
    }
  }
}
