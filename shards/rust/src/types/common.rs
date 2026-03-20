/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2020 Fragcolor Pte. Ltd. */

//! Common types, macros, and core type definitions for the Shards type system.

// Core Modules
use crate::core::{cloneVar, destroyVar, Core};
use crate::{
  fourCharacterCode, shlog, shlog_error, shlog_warn, SHAudio, SHExtendedObjectTypeInfo,
  SHType_Audio, SHVarPayload__bindgen_ty_1__bindgen_ty_3, SHVAR_FLAGS_WEAK_OBJECT,
};

// Shard Constants
use crate::shardsc::{
  SHBool, SHColor, SHComposeResult, SHContext, SHEnumInfo, SHEnumTypeInfo, SHExposedTypeInfo,
  SHExposedTypesInfo, SHImage, SHInstanceData, SHMeshRef, SHObjectTypeInfo, SHOptionalString,
  SHOptionalStrings, SHParameterInfo, SHParametersInfo, SHPointer, SHSeq, SHString, SHStrings,
  SHTable, SHTableIndices, SHTableIterator, SHTableTypeInfo, SHTraits, SHTypeInfo,
  SHTypeInfo_Details, SHType_Any, SHType_Bool, SHType_Bytes, SHType_Color, SHType_ContextVar,
  SHType_Enum, SHType_Float, SHType_Float2, SHType_Float3, SHType_Float4, SHType_Image, SHType_Int,
  SHType_Int16, SHType_Int2, SHType_Int3, SHType_Int4, SHType_Int8, SHType_None, SHType_Object,
  SHType_Path, SHType_Seq, SHType_ShardRef, SHType_String, SHType_Table, SHType_Wire, SHTypesInfo,
  SHVar, SHVarPayload, SHVarPayload__bindgen_ty_1, SHVarPayload__bindgen_ty_1__bindgen_ty_1,
  SHVarPayload__bindgen_ty_1__bindgen_ty_2, SHVarPayload__bindgen_ty_1__bindgen_ty_4, SHWire,
  SHWireInfo, SHWireRef, SHWireState, SHWireState_Continue, SHWireState_Rebase,
  SHWireState_Restart, SHWireState_Return, SHWireState_Stop, Shard, ShardPtr, Shards,
  SHIMAGE_FLAGS_16BITS_INT, SHIMAGE_FLAGS_32BITS_FLOAT, SHVAR_FLAGS_EXTERNAL,
  SHVAR_FLAGS_REF_COUNTED, SHVAR_FLAGS_USES_OBJINFO,
};

// Additional Shard Utilities
use crate::{SHObjectInfo, SHStringWithLen, SHType_Type, SHVar__bindgen_ty_1, SHWireState_Error};

// Core Conversions
use core::{
  convert::{TryFrom, TryInto},
  fmt::{Debug, Formatter},
  mem::transmute,
  ops::{Index, IndexMut},
  slice,
};
use std::ptr::NonNull;

use serde::ser::SerializeTuple;
// Serde for Serialization/Deserialization
use serde::{
  de::{MapAccess, SeqAccess, Visitor},
  ser::{SerializeMap, SerializeSeq},
  Deserialize, Deserializer, Serialize,
};

// Standard Libraries
use std::{
  borrow::Cow,
  ffi::{c_void, CStr, CString},
  hash::{Hash, Hasher},
  i32::MAX,
  os::raw::c_char,
  pin::Pin,
  rc::Rc,
  str::Utf8Error,
  sync::atomic::{AtomicU32, Ordering},
  sync::RwLock,
};

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum SHType {
  None,
  Any,
  Enum,
  Bool,
  Int,
  Int2,
  Int3,
  Int4,
  Int8,
  Int16,
  Float,
  Float2,
  Float3,
  Float4,
  Color,
  Bytes,
  String,
  Path,
  ContextVar,
  Image,
  Seq,
  Table,
  Wire,
  ShardRef,
  Object,
  Audio,
  Type,
  Trait,
}

impl From<crate::shardsc::SHType> for SHType {
  fn from(t: crate::shardsc::SHType) -> Self {
    match t {
      crate::shardsc::SHType_None => SHType::None,
      crate::shardsc::SHType_Any => SHType::Any,
      crate::shardsc::SHType_Enum => SHType::Enum,
      crate::shardsc::SHType_Bool => SHType::Bool,
      crate::shardsc::SHType_Int => SHType::Int,
      crate::shardsc::SHType_Int2 => SHType::Int2,
      crate::shardsc::SHType_Int3 => SHType::Int3,
      crate::shardsc::SHType_Int4 => SHType::Int4,
      crate::shardsc::SHType_Int8 => SHType::Int8,
      crate::shardsc::SHType_Int16 => SHType::Int16,
      crate::shardsc::SHType_Float => SHType::Float,
      crate::shardsc::SHType_Float2 => SHType::Float2,
      crate::shardsc::SHType_Float3 => SHType::Float3,
      crate::shardsc::SHType_Float4 => SHType::Float4,
      crate::shardsc::SHType_Color => SHType::Color,
      crate::shardsc::SHType_Bytes => SHType::Bytes,
      crate::shardsc::SHType_String => SHType::String,
      crate::shardsc::SHType_Path => SHType::Path,
      crate::shardsc::SHType_ContextVar => SHType::ContextVar,
      crate::shardsc::SHType_Image => SHType::Image,
      crate::shardsc::SHType_Seq => SHType::Seq,
      crate::shardsc::SHType_Table => SHType::Table,
      crate::shardsc::SHType_Wire => SHType::Wire,
      crate::shardsc::SHType_ShardRef => SHType::ShardRef,
      crate::shardsc::SHType_Object => SHType::Object,
      crate::shardsc::SHType_Audio => SHType::Audio,
      crate::shardsc::SHType_Type => SHType::Type,
      crate::shardsc::SHType_Trait => SHType::Trait,
      _ => panic!("Unknown SHType: {:?}", t),
    }
  }
}

impl From<SHType> for crate::shardsc::SHType {
  fn from(t: SHType) -> Self {
    match t {
      SHType::None => crate::shardsc::SHType_None,
      SHType::Any => crate::shardsc::SHType_Any,
      SHType::Enum => crate::shardsc::SHType_Enum,
      SHType::Bool => crate::shardsc::SHType_Bool,
      SHType::Int => crate::shardsc::SHType_Int,
      SHType::Int2 => crate::shardsc::SHType_Int2,
      SHType::Int3 => crate::shardsc::SHType_Int3,
      SHType::Int4 => crate::shardsc::SHType_Int4,
      SHType::Int8 => crate::shardsc::SHType_Int8,
      SHType::Int16 => crate::shardsc::SHType_Int16,
      SHType::Float => crate::shardsc::SHType_Float,
      SHType::Float2 => crate::shardsc::SHType_Float2,
      SHType::Float3 => crate::shardsc::SHType_Float3,
      SHType::Float4 => crate::shardsc::SHType_Float4,
      SHType::Color => crate::shardsc::SHType_Color,
      SHType::Bytes => crate::shardsc::SHType_Bytes,
      SHType::String => crate::shardsc::SHType_String,
      SHType::Path => crate::shardsc::SHType_Path,
      SHType::ContextVar => crate::shardsc::SHType_ContextVar,
      SHType::Image => crate::shardsc::SHType_Image,
      SHType::Seq => crate::shardsc::SHType_Seq,
      SHType::Table => crate::shardsc::SHType_Table,
      SHType::Wire => crate::shardsc::SHType_Wire,
      SHType::ShardRef => crate::shardsc::SHType_ShardRef,
      SHType::Object => crate::shardsc::SHType_Object,
      SHType::Audio => crate::shardsc::SHType_Audio,
      SHType::Type => crate::shardsc::SHType_Type,
      SHType::Trait => crate::shardsc::SHType_Trait,
    }
  }
}

pub fn type_to_string(t: SHType) -> &'static str {
  match t {
    SHType::None => "None",
    SHType::Any => "Any",
    SHType::Enum => "Enum",
    SHType::Bool => "Bool",
    SHType::Int => "Int",
    SHType::Int2 => "Int2",
    SHType::Int3 => "Int3",
    SHType::Int4 => "Int4",
    SHType::Int8 => "Int8",
    SHType::Int16 => "Int16",
    SHType::Float => "Float",
    SHType::Float2 => "Float2",
    SHType::Float3 => "Float3",
    SHType::Float4 => "Float4",
    SHType::Color => "Color",
    SHType::Bytes => "Bytes",
    SHType::String => "String",
    SHType::Path => "Path",
    SHType::ContextVar => "ContextVar",
    SHType::Image => "Image",
    SHType::Seq => "Seq",
    SHType::Table => "Table",
    SHType::Wire => "Wire",
    SHType::ShardRef => "ShardRef",
    SHType::Object => "Object",
    SHType::Audio => "Audio",
    SHType::Type => "Type",
    SHType::Trait => "Trait",
  }
}

pub type Context = SHContext;
pub type Var = SHVar;
pub type Type = SHTypeInfo;
pub type InstanceData = SHInstanceData;
pub type ComposeResult = SHComposeResult;
pub type ExposedInfo = SHExposedTypeInfo;
pub type ParameterInfo = SHParameterInfo;
pub type RawString = SHString;

#[repr(transparent)] // force it same size of original
#[derive(Default, Serialize, Hash, PartialEq, Eq, PartialOrd, Ord)]
pub struct ClonedVar(pub Var);

/// Typed wrapper for bytes output - avoids extra allocations
#[repr(transparent)]
#[derive(Default)]
pub struct BytesOut(pub ClonedVar);

/// Typed wrapper for string output - avoids extra allocations
#[repr(transparent)]
#[derive(Default)]
pub struct StringOut(pub ClonedVar);

impl BytesOut {
  pub fn new(data: &[u8]) -> Self {
    BytesOut(Var::from(data).into())
  }
}

impl From<&[u8]> for BytesOut {
  fn from(v: &[u8]) -> Self {
    BytesOut::new(v)
  }
}

impl std::ops::Deref for BytesOut {
  type Target = ClonedVar;
  fn deref(&self) -> &Self::Target {
    &self.0
  }
}

impl StringOut {
  pub fn new(s: &str) -> Self {
    StringOut(Var::ephemeral_string(s).into())
  }
}

impl From<&str> for StringOut {
  fn from(v: &str) -> Self {
    StringOut::new(v)
  }
}

impl std::ops::Deref for StringOut {
  type Target = ClonedVar;
  fn deref(&self) -> &Self::Target {
    &self.0
  }
}

impl Ord for Var {
  fn cmp(&self, other: &Self) -> std::cmp::Ordering {
    unsafe {
      // Use partialOrder to determine the full ordering directly
      match (*Core).compareVar.unwrap_unchecked()(
        self as *const SHVar as *mut SHVar,
        other as *const SHVar as *mut SHVar,
      ) {
        -1 => std::cmp::Ordering::Less,
        0 => std::cmp::Ordering::Equal,
        1 => std::cmp::Ordering::Greater,
        _ => unreachable!("compareVar returned an invalid value"),
      }
    }
  }
}

impl PartialOrd for Var {
  fn partial_cmp(&self, other: &Self) -> Option<std::cmp::Ordering> {
    Some(self.cmp(other))
  }
}

impl ClonedVar {
  pub fn new_deserializing(bytes_buffer_var: &Var) -> Self {
    ClonedVar(unsafe { (*Core).deserializeVar.unwrap_unchecked()(bytes_buffer_var) })
  }

  pub fn new_string(s: &str) -> Self {
    let string = Var::ephemeral_string(s);
    string.into()
  }

  pub fn new_path(s: &str) -> Self {
    let mut path = Var::ephemeral_string(s);
    path.valueType = SHType_Path;
    path.into()
  }

  pub fn new_context_var(s: &str) -> Self {
    let mut context_var = Var::ephemeral_string(s);
    context_var.valueType = SHType_ContextVar;
    context_var.into()
  }

  pub fn new_bytes(bytes: &[u8]) -> Self {
    let bytes = Var::ephemeral_slice(bytes);
    bytes.into()
  }

  pub fn new_image(
    width: u16,
    height: u16,
    channels: u8,
    flags: u8,
    data: &[u8],
  ) -> Result<Self, &'static str> {
    let dataLen = unsafe {
      let mut dummyImg = SHImage {
        width,
        height,
        channels,
        flags,
        ..Default::default()
      };
      (*Core).imageDeriveDataLength.unwrap_unchecked()(&mut dummyImg)
    };

    if data.len() != dataLen as usize {
      return Err("Invalid image data length");
    }

    return Ok(unsafe {
      let image = (*Core).imageNew.unwrap_unchecked()(dataLen);
      (*image).width = width;
      (*image).height = height;
      (*image).channels = channels;
      (*image).flags = flags;
      std::ptr::copy_nonoverlapping(data.as_ptr(), (*image).data, dataLen as usize);

      ClonedVar(Var {
        valueType: SHType_Image,
        payload: SHVarPayload {
          __bindgen_anon_1: SHVarPayload__bindgen_ty_1 { imageValue: image },
        },
        ..Default::default()
      })
    });
  }

  pub fn assign(&mut self, other: &Var) {
    cloneVar(&mut self.0, other);
  }

  pub fn set_param(&mut self, other: &Var) -> Result<(), &'static str> {
    self.assign(other);
    Ok(())
  }

  pub fn assign_string(&mut self, s: &str) {
    let cstr = CString::new(s).unwrap();
    let tmp = Var::from(&cstr);
    self.assign(&tmp);
  }
}

impl Clone for ClonedVar {
  #[inline(always)]
  fn clone(&self) -> Self {
    let mut ret = ClonedVar::default();
    ret.assign(&self.0);
    ret
  }
}

impl Drop for ClonedVar {
  #[inline(always)]
  fn drop(&mut self) {
    unsafe {
      let rv = &self.0 as *const SHVar as *mut SHVar;
      (*Core).destroyVar.unwrap_unchecked()(rv);
    }
  }
}

impl Debug for ClonedVar {
  fn fmt(&self, f: &mut Formatter<'_>) -> core::fmt::Result {
    write!(f, "{:?}", self.0)
  }
}

#[repr(transparent)] // force it same size of original
pub struct ExternalVar(pub Var);

impl Default for ExternalVar {
  #[inline(always)]
  fn default() -> Self {
    let mut res = ExternalVar(Var::default());
    res.0.flags |= SHVAR_FLAGS_EXTERNAL as u16;
    res
  }
}

impl ExternalVar {
  #[inline(always)]
  pub fn update<T>(&mut self, value: T)
  where
    T: Into<Var>,
  {
    let vt: Var = value.into();
    unsafe {
      let rv = &self.0 as *const SHVar as *mut SHVar;
      let sv = &vt as *const SHVar;
      (*Core).cloneVar.unwrap_unchecked()(rv, sv);
    }
    // ensure this flag is set
    self.0.flags |= SHVAR_FLAGS_EXTERNAL as u16;
  }
}

impl Drop for ExternalVar {
  #[inline(always)]
  fn drop(&mut self) {
    unsafe {
      let rv = &self.0 as *const SHVar as *mut SHVar;
      (*Core).destroyVar.unwrap_unchecked()(rv);
    }
  }
}

#[repr(transparent)] // force it same size of original
#[derive(Default)]
pub struct WrappedVar(pub Var); // used in DSL macro, ignore it

unsafe impl Send for Var {}
unsafe impl Send for Context {}
unsafe impl Send for Shard {}
unsafe impl Sync for Var {}
unsafe impl Send for SHOptionalString {}
unsafe impl Sync for SHOptionalString {}
unsafe impl Sync for ClonedVar {}
unsafe impl Sync for ExternalVar {}

/*
SHTypeInfo & co
*/
unsafe impl Send for SHTypeInfo {}
unsafe impl Send for SHExposedTypeInfo {}
unsafe impl Send for SHExtendedObjectTypeInfo {}
unsafe impl Send for SHParameterInfo {}
unsafe impl Send for SHStrings {}
unsafe impl Send for SHObjectInfo {}
unsafe impl Sync for SHTypeInfo {}
unsafe impl Sync for SHExposedTypeInfo {}
unsafe impl Sync for SHExtendedObjectTypeInfo {}
unsafe impl Sync for SHParameterInfo {}
unsafe impl Sync for SHStrings {}
unsafe impl Sync for SHObjectInfo {}
