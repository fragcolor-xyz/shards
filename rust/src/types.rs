/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2020 Fragcolor Pte. Ltd. */

use crate::core::cloneVar;
use crate::core::Core;
use crate::shardsc::SHBool;
use crate::shardsc::SHComposeResult;
use crate::shardsc::SHContext;
use crate::shardsc::SHExposedTypeInfo;
use crate::shardsc::SHExposedTypesInfo;
use crate::shardsc::SHInstanceData;
use crate::shardsc::SHMeshRef;
use crate::shardsc::SHOptionalString;
use crate::shardsc::SHParameterInfo;
use crate::shardsc::SHParametersInfo;
use crate::shardsc::SHPointer;
use crate::shardsc::SHSeq;
use crate::shardsc::SHString;
use crate::shardsc::SHStrings;
use crate::shardsc::SHTable;
use crate::shardsc::SHTableIterator;
use crate::shardsc::SHTypeInfo;
use crate::shardsc::SHTypeInfo_Details;
use crate::shardsc::SHTypeInfo_Details_Enum;
use crate::shardsc::SHTypeInfo_Details_Object;
use crate::shardsc::SHTypeInfo_Details_Table;
use crate::shardsc::SHType_Any;
use crate::shardsc::SHType_Array;
use crate::shardsc::SHType_Bool;
use crate::shardsc::SHType_Bytes;
use crate::shardsc::SHType_Color;
use crate::shardsc::SHType_ContextVar;
use crate::shardsc::SHType_Enum;
use crate::shardsc::SHType_Float;
use crate::shardsc::SHType_Float2;
use crate::shardsc::SHType_Float3;
use crate::shardsc::SHType_Float4;
use crate::shardsc::SHType_Image;
use crate::shardsc::SHType_Int;
use crate::shardsc::SHType_Int16;
use crate::shardsc::SHType_Int2;
use crate::shardsc::SHType_Int3;
use crate::shardsc::SHType_Int4;
use crate::shardsc::SHType_Int8;
use crate::shardsc::SHType_None;
use crate::shardsc::SHType_Object;
use crate::shardsc::SHType_Path;
use crate::shardsc::SHType_Seq;
use crate::shardsc::SHType_Set;
use crate::shardsc::SHType_ShardRef;
use crate::shardsc::SHType_String;
use crate::shardsc::SHType_Table;
use crate::shardsc::SHType_Wire;
use crate::shardsc::SHTypesInfo;
use crate::shardsc::SHVar;
use crate::shardsc::SHVarPayload;
use crate::shardsc::SHVarPayload__bindgen_ty_1;
use crate::shardsc::SHVarPayload__bindgen_ty_1__bindgen_ty_1;
use crate::shardsc::SHVarPayload__bindgen_ty_1__bindgen_ty_2;
use crate::shardsc::SHVarPayload__bindgen_ty_1__bindgen_ty_4;
use crate::shardsc::SHWire;
use crate::shardsc::SHWireRef;
use crate::shardsc::SHWireState;
use crate::shardsc::SHWireState_Continue;
use crate::shardsc::SHWireState_Rebase;
use crate::shardsc::SHWireState_Restart;
use crate::shardsc::SHWireState_Return;
use crate::shardsc::SHWireState_Stop;
use crate::shardsc::Shard;
use crate::shardsc::ShardPtr;
use crate::shardsc::Shards;
use crate::shardsc::SHIMAGE_FLAGS_16BITS_INT;
use crate::shardsc::SHIMAGE_FLAGS_32BITS_FLOAT;
use crate::shardsc::SHVAR_FLAGS_REF_COUNTED;
use crate::SHVAR_FLAGS_EXTERNAL;
use core::convert::TryFrom;
use core::convert::TryInto;
use core::mem::transmute;
use core::ops::Index;
use core::ops::IndexMut;
use core::slice;
use serde::ser::{SerializeMap, SerializeSeq};
use serde::{Deserialize, Serialize};
use std::ffi::CStr;
use std::ffi::CString;
use std::rc::Rc;

#[macro_export]
macro_rules! cstr {
  ($text:expr) => {
    concat!($text, "\0")
  };
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
#[derive(Default)]
pub struct ClonedVar(pub Var);

#[repr(transparent)] // force it same size of original
pub struct ExternalVar(pub Var);

#[repr(transparent)] // force it same size of original
#[derive(Default)]
pub struct WrappedVar(pub Var); // used in DSL macro, ignore it

#[derive(Clone)]
pub struct Mesh(pub SHMeshRef);

#[derive(Copy, Clone)]
pub struct WireRef(pub SHWireRef);

#[derive(Copy, Clone)]
pub struct ShardRef(pub ShardPtr);

impl Default for Mesh {
  fn default() -> Self {
    Mesh(unsafe { (*Core).createMesh.unwrap()() })
  }
}

impl Drop for Mesh {
  fn drop(&mut self) {
    unsafe { (*Core).destroyMesh.unwrap()(self.0) }
  }
}

impl Mesh {
  pub fn schedule(&self, wire: WireRef) {
    unsafe { (*Core).schedule.unwrap()(self.0, wire.0) }
  }

  pub fn tick(&self) -> bool {
    unsafe { (*Core).tick.unwrap()(self.0) }
  }
}

pub struct Wire(pub WireRef);

impl Default for Wire {
  fn default() -> Self {
    Wire(WireRef(unsafe { (*Core).createWire.unwrap()() }))
  }
}

impl Drop for Wire {
  fn drop(&mut self) {
    unsafe { (*Core).destroyWire.unwrap()(self.0 .0) }
  }
}

impl Wire {
  pub fn add_shard(&self, shard: ShardRef) {
    unsafe { (*Core).addShard.unwrap()(self.0 .0, shard.0) }
  }

  pub fn set_looped(&self, looped: bool) {
    unsafe { (*Core).setWireLooped.unwrap()(self.0 .0, looped) }
  }

  pub fn set_name(&self, name: &str) {
    let c_name = CString::new(name).unwrap();
    unsafe { (*Core).setWireName.unwrap()(self.0 .0, c_name.as_ptr()) }
  }
}

impl ShardRef {
  pub fn create(name: &str) -> Self {
    let c_name = CString::new(name).unwrap();
    ShardRef(unsafe { (*Core).createShard.unwrap()(c_name.as_ptr()) })
  }

  pub fn set_parameter(&self, index: i32, value: Var) {
    unsafe {
      (*self.0).setParam.unwrap()(self.0, index, &value);
    }
  }
}

#[derive(PartialEq)]
pub struct String(pub SHString);
#[derive(Default, Clone, Copy)]
pub struct OptionalString(pub SHOptionalString);
pub struct DerivedType(pub Type);

#[macro_export]
macro_rules! shstr {
  ($text:expr) => {{
    const shstr: RawString = concat!($text, "\0").as_ptr() as *const std::os::raw::c_char;
    shstr
  }};
}

impl Drop for DerivedType {
  fn drop(&mut self) {
    let ti = &mut self.0;
    unsafe {
      (*Core).freeDerivedTypeInfo.unwrap()(ti as *mut _);
    }
  }
}

#[derive(PartialEq)]
pub enum WireState {
  Continue,
  Return,
  Rebase,
  Restart,
  Stop,
}

impl From<SHWireState> for WireState {
  fn from(state: SHWireState) -> Self {
    match state {
      SHWireState_Continue => WireState::Continue,
      SHWireState_Return => WireState::Return,
      SHWireState_Rebase => WireState::Rebase,
      SHWireState_Restart => WireState::Restart,
      SHWireState_Stop => WireState::Stop,
      _ => unreachable!(),
    }
  }
}

unsafe impl Send for Var {}
unsafe impl Send for Context {}
unsafe impl Send for Shard {}
unsafe impl Send for Table {}
unsafe impl Sync for Table {}
unsafe impl Sync for OptionalString {}
unsafe impl Sync for WireRef {}
unsafe impl Sync for Mesh {}
unsafe impl Sync for ClonedVar {}
unsafe impl Sync for ExternalVar {}

/*
SHTypeInfo & co
*/
unsafe impl std::marker::Sync for SHTypeInfo {}
unsafe impl std::marker::Sync for SHExposedTypeInfo {}
unsafe impl std::marker::Sync for SHParameterInfo {}
unsafe impl std::marker::Sync for SHStrings {}

// Todo SHTypeInfo proper wrapper Type with helpers

pub type Types = Vec<Type>;

impl From<&Types> for SHTypesInfo {
  fn from(types: &Types) -> Self {
    SHTypesInfo {
      elements: types.as_ptr() as *mut SHTypeInfo,
      len: types.len() as u32,
      cap: 0,
    }
  }
}

impl From<&[Type]> for SHTypesInfo {
  fn from(types: &[Type]) -> Self {
    SHTypesInfo {
      elements: types.as_ptr() as *mut SHTypeInfo,
      len: types.len() as u32,
      cap: 0,
    }
  }
}

fn internal_from_types(types: Types) -> SHTypesInfo {
  let len = types.len();
  let boxed = types.into_boxed_slice();
  SHTypesInfo {
    elements: Box::into_raw(boxed) as *mut SHTypeInfo,
    len: len as u32,
    cap: 0,
  }
}

unsafe fn internal_drop_types(types: SHTypesInfo) {
  // use with care
  let elems = Box::from_raw(types.elements);
  drop(elems);
}

/*
SHExposedTypeInfo & co
*/

impl ExposedInfo {
  pub fn new(name: &CString, ctype: SHTypeInfo) -> Self {
    let chelp = core::ptr::null();
    SHExposedTypeInfo {
      exposedType: ctype,
      name: name.as_ptr(),
      help: SHOptionalString {
        string: chelp,
        crc: 0,
      },
      isMutable: false,
      isProtected: false,
      isTableEntry: false,
      global: false,
      scope: core::ptr::null_mut(),
    }
  }

  pub fn new_with_help(name: &CString, help: &CString, ctype: SHTypeInfo) -> Self {
    SHExposedTypeInfo {
      exposedType: ctype,
      name: name.as_ptr(),
      help: SHOptionalString {
        string: help.as_ptr(),
        crc: 0,
      },
      isMutable: false,
      isProtected: false,
      isTableEntry: false,
      global: false,
      scope: core::ptr::null_mut(),
    }
  }

  pub fn new_with_help_from_ptr(name: SHString, help: SHOptionalString, ctype: SHTypeInfo) -> Self {
    SHExposedTypeInfo {
      exposedType: ctype,
      name,
      help,
      isMutable: false,
      isProtected: false,
      isTableEntry: false,
      global: false,
      scope: core::ptr::null_mut(),
    }
  }

  pub const fn new_static(name: &'static str, ctype: SHTypeInfo) -> Self {
    let cname = name.as_ptr() as *const std::os::raw::c_char;
    let chelp = core::ptr::null();
    SHExposedTypeInfo {
      exposedType: ctype,
      name: cname,
      help: SHOptionalString {
        string: chelp,
        crc: 0,
      },
      isMutable: false,
      isProtected: false,
      isTableEntry: false,
      global: false,
      scope: core::ptr::null_mut(),
    }
  }

  pub const fn new_static_with_help(
    name: &'static str,
    help: SHOptionalString,
    ctype: SHTypeInfo,
  ) -> Self {
    let cname = name.as_ptr() as *const std::os::raw::c_char;
    SHExposedTypeInfo {
      exposedType: ctype,
      name: cname,
      help,
      isMutable: false,
      isProtected: false,
      isTableEntry: false,
      global: false,
      scope: core::ptr::null_mut(),
    }
  }
}

pub type ExposedTypes = Vec<ExposedInfo>;

impl From<&ExposedTypes> for SHExposedTypesInfo {
  fn from(vec: &ExposedTypes) -> Self {
    SHExposedTypesInfo {
      elements: vec.as_ptr() as *mut SHExposedTypeInfo,
      len: vec.len() as u32,
      cap: 0,
    }
  }
}

/*
SHParameterInfo & co
*/
impl ParameterInfo {
  fn new(name: &'static str, types: Types) -> Self {
    SHParameterInfo {
      name: name.as_ptr() as *mut std::os::raw::c_char,
      help: SHOptionalString {
        string: core::ptr::null(),
        crc: 0,
      },
      valueTypes: internal_from_types(types),
    }
  }

  fn new1(name: &'static str, help: &'static str, types: Types) -> Self {
    SHParameterInfo {
      name: name.as_ptr() as *mut std::os::raw::c_char,
      help: SHOptionalString {
        string: help.as_ptr() as *mut std::os::raw::c_char,
        crc: 0,
      },
      valueTypes: internal_from_types(types),
    }
  }

  fn new2(name: &'static str, help: SHOptionalString, types: Types) -> Self {
    SHParameterInfo {
      name: name.as_ptr() as *mut std::os::raw::c_char,
      help,
      valueTypes: internal_from_types(types),
    }
  }
}

impl From<&str> for OptionalString {
  fn from(s: &str) -> OptionalString {
    let cos = SHOptionalString {
      string: s.as_ptr() as *const std::os::raw::c_char,
      crc: 0, // TODO
    };
    OptionalString(cos)
  }
}

impl From<&str> for SHOptionalString {
  fn from(s: &str) -> SHOptionalString {
    SHOptionalString {
      string: s.as_ptr() as *const std::os::raw::c_char,
      crc: 0, // TODO
    }
  }
}

impl From<(&'static str, Types)> for ParameterInfo {
  fn from(v: (&'static str, Types)) -> ParameterInfo {
    ParameterInfo::new(v.0, v.1)
  }
}

impl From<(&'static str, &'static str, Types)> for ParameterInfo {
  fn from(v: (&'static str, &'static str, Types)) -> ParameterInfo {
    ParameterInfo::new1(v.0, v.1, v.2)
  }
}

impl From<(&'static str, SHOptionalString, Types)> for ParameterInfo {
  fn from(v: (&'static str, SHOptionalString, Types)) -> ParameterInfo {
    ParameterInfo::new2(v.0, v.1, v.2)
  }
}

pub type Parameters = Vec<ParameterInfo>;

impl From<&Parameters> for SHParametersInfo {
  fn from(vec: &Parameters) -> Self {
    SHParametersInfo {
      elements: vec.as_ptr() as *mut SHParameterInfo,
      len: vec.len() as u32,
      cap: 0,
    }
  }
}

impl From<SHParametersInfo> for &[SHParameterInfo] {
  fn from(_: SHParametersInfo) -> Self {
    unimplemented!()
  }
}

/*
Static common type infos utility
*/
pub mod common_type {
  use crate::shardsc::SHStrings;
  use crate::shardsc::SHTypeInfo;
  use crate::shardsc::SHTypeInfo_Details;
  use crate::shardsc::SHTypeInfo_Details_Table;
  use crate::shardsc::SHType_Any;
  use crate::shardsc::SHType_Bool;
  use crate::shardsc::SHType_Bytes;
  use crate::shardsc::SHType_ContextVar;
  use crate::shardsc::SHType_Enum;
  use crate::shardsc::SHType_Float;
  use crate::shardsc::SHType_Int;
  use crate::shardsc::SHType_None;
  use crate::shardsc::SHType_Path;
  use crate::shardsc::SHType_Seq;
  use crate::shardsc::SHType_ShardRef;
  use crate::shardsc::SHType_String;
  use crate::shardsc::SHType_Table;
  use crate::shardsc::SHType_Wire;
  use crate::shardsc::SHTypesInfo;
  use crate::types::SHType_Float2;
  use crate::types::SHType_Float3;
  use crate::types::SHType_Float4;
  use crate::types::SHType_Image;
  use crate::types::SHType_Int2;
  use crate::types::SHType_Object;

  const fn base_info() -> SHTypeInfo {
    SHTypeInfo {
      basicType: SHType_None,
      details: SHTypeInfo_Details {
        seqTypes: SHTypesInfo {
          elements: core::ptr::null_mut(),
          len: 0,
          cap: 0,
        },
      },
      fixedSize: 0,
      innerType: SHType_None,
      recursiveSelf: false,
    }
  }

  pub static none: SHTypeInfo = base_info();

  macro_rules! shtype {
    ($fname:ident, $type:expr, $name:ident, $names:ident, $name_var:ident, $names_var:ident, $name_table:ident, $name_table_var:ident) => {
      const fn $fname() -> SHTypeInfo {
        let mut res = base_info();
        res.basicType = $type;
        res
      }

      pub static $name: SHTypeInfo = $fname();

      pub static $names: SHTypeInfo = SHTypeInfo {
        basicType: SHType_Seq,
        details: SHTypeInfo_Details {
          seqTypes: SHTypesInfo {
            elements: &$name as *const SHTypeInfo as *mut SHTypeInfo,
            len: 1,
            cap: 0,
          },
        },
        fixedSize: 0,
        innerType: SHType_None,
        recursiveSelf: false,
      };

      pub static $name_table: SHTypeInfo = SHTypeInfo {
        basicType: SHType_Table,
        details: SHTypeInfo_Details {
          table: SHTypeInfo_Details_Table {
            keys: SHStrings {
              elements: core::ptr::null_mut(),
              len: 0,
              cap: 0,
            },
            types: SHTypesInfo {
              elements: &$name as *const SHTypeInfo as *mut SHTypeInfo,
              len: 1,
              cap: 0,
            },
          },
        },
        fixedSize: 0,
        innerType: SHType_None,
        recursiveSelf: false,
      };

      pub static $name_var: SHTypeInfo = SHTypeInfo {
        basicType: SHType_ContextVar,
        details: SHTypeInfo_Details {
          contextVarTypes: SHTypesInfo {
            elements: &$name as *const SHTypeInfo as *mut SHTypeInfo,
            len: 1,
            cap: 0,
          },
        },
        fixedSize: 0,
        innerType: SHType_None,
        recursiveSelf: false,
      };

      pub static $names_var: SHTypeInfo = SHTypeInfo {
        basicType: SHType_ContextVar,
        details: SHTypeInfo_Details {
          contextVarTypes: SHTypesInfo {
            elements: &$names as *const SHTypeInfo as *mut SHTypeInfo,
            len: 1,
            cap: 0,
          },
        },
        fixedSize: 0,
        innerType: SHType_None,
        recursiveSelf: false,
      };

      pub static $name_table_var: SHTypeInfo = SHTypeInfo {
        basicType: SHType_ContextVar,
        details: SHTypeInfo_Details {
          contextVarTypes: SHTypesInfo {
            elements: &$name_table as *const SHTypeInfo as *mut SHTypeInfo,
            len: 1,
            cap: 0,
          },
        },
        fixedSize: 0,
        innerType: SHType_None,
        recursiveSelf: false,
      };
    };
  }

  shtype!(
    make_any,
    SHType_Any,
    any,
    anys,
    any_var,
    anys_var,
    any_table,
    any_table_var
  );
  shtype!(
    make_object,
    SHType_Object,
    object,
    objects,
    object_var,
    objects_var,
    object_table,
    object_table_var
  );
  shtype!(
    make_enum,
    SHType_Enum,
    enumeration,
    enums,
    enum_var,
    enums_var,
    enum_table,
    enum_table_var
  );
  shtype!(
    make_string,
    SHType_String,
    string,
    strings,
    string_var,
    strings_var,
    string_table,
    string_table_var
  );
  shtype!(
    make_bytes,
    SHType_Bytes,
    bytes,
    bytezs,
    bytes_var,
    bytess_var,
    bytes_table,
    bytes_table_var
  );
  shtype!(
    make_image,
    SHType_Image,
    image,
    images,
    image_var,
    images_var,
    image_table,
    images_table_var
  );
  shtype!(
    make_int,
    SHType_Int,
    int,
    ints,
    int_var,
    ints_var,
    int_table,
    int_table_var
  );
  shtype!(
    make_int2,
    SHType_Int2,
    int2,
    int2s,
    int2_var,
    int2s_var,
    int2_table,
    int2_table_var
  );
  shtype!(
    make_float,
    SHType_Float,
    float,
    floats,
    float_var,
    floats_var,
    float_table,
    float_table_var
  );
  shtype!(
    make_float2,
    SHType_Float2,
    float2,
    float2s,
    float2_var,
    float2s_var,
    float2_table,
    float2_table_var
  );
  shtype!(
    make_float3,
    SHType_Float3,
    float3,
    float3s,
    float3_var,
    float3s_var,
    float3_table,
    float3_table_var
  );
  shtype!(
    make_float4,
    SHType_Float4,
    float4,
    float4s,
    float4_var,
    float4s_var,
    float4_table,
    float4_table_var
  );
  shtype!(
    make_bool,
    SHType_Bool,
    bool,
    bools,
    bool_var,
    bools_var,
    bool_table,
    bool_table_var
  );
  shtype!(
    make_shard,
    SHType_ShardRef,
    shard,
    shards,
    shard_var,
    shards_var,
    shard_table,
    shard_table_var
  );
  shtype!(
    make_wire,
    SHType_Wire,
    wire,
    wires,
    wire_var,
    wires_var,
    wire_table,
    wire_table_var
  );
  shtype!(
    make_path,
    SHType_Path,
    path,
    paths,
    path_var,
    paths_var,
    path_table,
    path_table_var
  );
}

impl Type {
  pub const fn context_variable(types: &[Type]) -> Type {
    Type {
      basicType: SHType_ContextVar,
      details: SHTypeInfo_Details {
        contextVarTypes: SHTypesInfo {
          elements: types.as_ptr() as *mut SHTypeInfo,
          len: types.len() as u32,
          cap: 0,
        },
      },
      fixedSize: 0,
      innerType: SHType_None,
      recursiveSelf: false,
    }
  }

  pub const fn object(vendorId: i32, typeId: i32) -> Type {
    Type {
      basicType: SHType_Object,
      details: SHTypeInfo_Details {
        object: SHTypeInfo_Details_Object { vendorId, typeId },
      },
      fixedSize: 0,
      innerType: SHType_None,
      recursiveSelf: false,
    }
  }

  pub const fn table(keys: &[SHString], types: &[Type]) -> Type {
    Type {
      basicType: SHType_Table,
      details: SHTypeInfo_Details {
        table: SHTypeInfo_Details_Table {
          keys: SHStrings {
            elements: keys.as_ptr() as *mut _,
            len: keys.len() as u32,
            cap: 0,
          },
          types: SHTypesInfo {
            elements: types.as_ptr() as *mut SHTypeInfo,
            len: types.len() as u32,
            cap: 0,
          },
        },
      },
      fixedSize: 0,
      innerType: SHType_None,
      recursiveSelf: false,
    }
  }

  pub const fn seq(types: &[Type]) -> Type {
    Type {
      basicType: SHType_Seq,
      details: SHTypeInfo_Details {
        seqTypes: SHTypesInfo {
          elements: types.as_ptr() as *mut SHTypeInfo,
          len: types.len() as u32,
          cap: 0,
        },
      },
      fixedSize: 0,
      innerType: SHType_None,
      recursiveSelf: false,
    }
  }
}

/*
SHVar utility
 */

impl Serialize for Var {
  fn serialize<S>(
    &self,
    se: S,
  ) -> Result<<S as serde::Serializer>::Ok, <S as serde::Serializer>::Error>
  where
    S: serde::Serializer,
  {
    match self.valueType {
      SHType_Int => {
        let value: i64 = unsafe { self.payload.__bindgen_anon_1.intValue };
        se.serialize_i64(value)
      }
      SHType_Float => {
        let value: f64 = unsafe { self.payload.__bindgen_anon_1.floatValue };
        se.serialize_f64(value)
      }
      SHType_Bool => {
        let value: bool = unsafe { self.payload.__bindgen_anon_1.boolValue };
        se.serialize_bool(value)
      }
      SHType_Table => {
        let table: Table = self.try_into().unwrap();
        let mut t = se.serialize_map(None)?;
        for (key, value) in table.iter() {
          unsafe {
            let key = CStr::from_ptr(key.0).to_str().unwrap();
            t.serialize_entry(&key, &value)?;
          }
        }
        t.end()
      }
      SHType_String => {
        let value: &str = self.try_into().unwrap();
        se.serialize_str(value)
      }
      SHType_Seq => {
        let seq: Seq = self.try_into().unwrap();
        let mut s = se.serialize_seq(Some(seq.len()))?;
        for value in seq {
          s.serialize_element(&value)?;
        }
        s.end()
      }
      _ => se.serialize_none(),
    }
  }
}

impl<T> From<T> for ClonedVar
where
  T: Into<Var>,
{
  fn from(v: T) -> Self {
    let vt: Var = v.into();
    let res = ClonedVar(Var::default());
    unsafe {
      let rv = &res.0 as *const SHVar as *mut SHVar;
      let sv = &vt as *const SHVar;
      (*Core).cloneVar.unwrap()(rv, sv);
    }
    res
  }
}

impl<T> From<T> for ExternalVar
where
  T: Into<Var>,
{
  fn from(v: T) -> Self {
    let vt: Var = v.into();
    let mut res = ExternalVar(Var::default());
    unsafe {
      let rv = &res.0 as *const SHVar as *mut SHVar;
      let sv = &vt as *const SHVar;
      (*Core).cloneVar.unwrap()(rv, sv);
    }
    // ensure this flag is set
    res.0.flags |= SHVAR_FLAGS_EXTERNAL as u16;
    res
  }
}

impl From<&Var> for ClonedVar {
  fn from(v: &Var) -> Self {
    let res = ClonedVar(Var::default());
    unsafe {
      let rv = &res.0 as *const SHVar as *mut SHVar;
      let sv = v as *const SHVar;
      (*Core).cloneVar.unwrap()(rv, sv);
    }
    res
  }
}

impl From<&Var> for ExternalVar {
  fn from(v: &Var) -> Self {
    let mut res = ExternalVar(Var::default());
    unsafe {
      let rv = &res.0 as *const SHVar as *mut SHVar;
      let sv = v as *const SHVar;
      (*Core).cloneVar.unwrap()(rv, sv);
    }
    // ensure this flag is set
    res.0.flags |= SHVAR_FLAGS_EXTERNAL as u16;
    res
  }
}

impl From<Vec<Var>> for ClonedVar {
  fn from(v: Vec<Var>) -> Self {
    let tmp = Var::from(&v);
    let res = ClonedVar(Var::default());
    unsafe {
      let rv = &res.0 as *const SHVar as *mut SHVar;
      let sv = &tmp as *const SHVar;
      (*Core).cloneVar.unwrap()(rv, sv);
    }
    res
  }
}

impl From<std::string::String> for ClonedVar {
  fn from(v: std::string::String) -> Self {
    let cstr = CString::new(v).unwrap();
    let tmp = Var::from(&cstr);
    let res = ClonedVar(Var::default());
    unsafe {
      let rv = &res.0 as *const SHVar as *mut SHVar;
      let sv = &tmp as *const SHVar;
      (*Core).cloneVar.unwrap()(rv, sv);
    }
    res
  }
}

impl From<&[ClonedVar]> for ClonedVar {
  fn from(vec: &[ClonedVar]) -> Self {
    let res = ClonedVar(Var::default());
    unsafe {
      let src: &[Var] = &*(vec as *const [ClonedVar] as *const [SHVar]);
      let vsrc: Var = src.into();
      let rv = &res.0 as *const SHVar as *mut SHVar;
      let sv = &vsrc as *const SHVar;
      (*Core).cloneVar.unwrap()(rv, sv);
    }
    res
  }
}

impl Drop for ClonedVar {
  #[inline(always)]
  fn drop(&mut self) {
    unsafe {
      let rv = &self.0 as *const SHVar as *mut SHVar;
      (*Core).destroyVar.unwrap()(rv);
    }
  }
}

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
      (*Core).cloneVar.unwrap()(rv, sv);
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
      (*Core).destroyVar.unwrap()(rv);
    }
  }
}

macro_rules! var_from {
  ($type:ident, $varfield:ident, $shtype:expr) => {
    impl From<$type> for Var {
      #[inline(always)]
      fn from(v: $type) -> Self {
        SHVar {
          valueType: $shtype,
          payload: SHVarPayload {
            __bindgen_anon_1: SHVarPayload__bindgen_ty_1 { $varfield: v },
          },
          ..Default::default()
        }
      }
    }
  };
}

macro_rules! var_from_into {
  ($type:ident, $varfield:ident, $shtype:expr) => {
    impl From<$type> for Var {
      #[inline(always)]
      fn from(v: $type) -> Self {
        SHVar {
          valueType: $shtype,
          payload: SHVarPayload {
            __bindgen_anon_1: SHVarPayload__bindgen_ty_1 {
              $varfield: v.into(),
            },
          },
          ..Default::default()
        }
      }
    }
  };
}

macro_rules! var_try_from {
  ($type:ident, $varfield:ident, $shtype:expr) => {
    impl TryFrom<$type> for Var {
      type Error = &'static str;

      #[inline(always)]
      fn try_from(v: $type) -> Result<Self, Self::Error> {
        Ok(SHVar {
          valueType: $shtype,
          payload: SHVarPayload {
            __bindgen_anon_1: SHVarPayload__bindgen_ty_1 {
              $varfield: v
                .try_into()
                .map_err(|_| "Conversion failed, value out of range")?,
            },
          },
          ..Default::default()
        })
      }
    }
  };
}

var_from!(bool, boolValue, SHType_Bool);
var_from!(i64, intValue, SHType_Int);
var_try_from!(u8, intValue, SHType_Int);
var_try_from!(u16, intValue, SHType_Int);
var_try_from!(u32, intValue, SHType_Int);
var_try_from!(u128, intValue, SHType_Int);
var_try_from!(i128, intValue, SHType_Int);
var_from_into!(i32, intValue, SHType_Int);
var_try_from!(usize, intValue, SHType_Int);
var_try_from!(u64, intValue, SHType_Int);
var_from!(f64, floatValue, SHType_Float);
var_from_into!(f32, floatValue, SHType_Float);

impl From<ShardPtr> for Var {
  #[inline(always)]
  fn from(v: ShardPtr) -> Self {
    SHVar {
      valueType: SHType_String,
      payload: SHVarPayload {
        __bindgen_anon_1: SHVarPayload__bindgen_ty_1 { shardValue: v },
      },
      ..Default::default()
    }
  }
}

impl From<SHString> for Var {
  #[inline(always)]
  fn from(v: SHString) -> Self {
    SHVar {
      valueType: SHType_String,
      payload: SHVarPayload {
        __bindgen_anon_1: SHVarPayload__bindgen_ty_1 {
          __bindgen_anon_2: SHVarPayload__bindgen_ty_1__bindgen_ty_2 {
            stringValue: v,
            stringLen: 0,
            stringCapacity: 0,
          },
        },
      },
      ..Default::default()
    }
  }
}

impl From<String> for &str {
  #[inline(always)]
  fn from(v: String) -> Self {
    unsafe {
      let cstr = CStr::from_ptr(v.0);
      cstr.to_str().unwrap()
    }
  }
}

impl<'a> From<&'a str> for Var {
  #[inline(always)]
  fn from(v: &'a str) -> Self {
    let len = v.len();
    let p = if len > 0 {
      v.as_ptr()
    } else {
      core::ptr::null()
    };
    SHVar {
      valueType: SHType_String,
      payload: SHVarPayload {
        __bindgen_anon_1: SHVarPayload__bindgen_ty_1 {
          __bindgen_anon_2: SHVarPayload__bindgen_ty_1__bindgen_ty_2 {
            stringValue: p as *const std::os::raw::c_char,
            stringLen: len as u32,
            stringCapacity: 0,
          },
        },
      },
      ..Default::default()
    }
  }
}

impl From<&CStr> for Var {
  #[inline(always)]
  fn from(v: &CStr) -> Self {
    SHVar {
      valueType: SHType_String,
      payload: SHVarPayload {
        __bindgen_anon_1: SHVarPayload__bindgen_ty_1 {
          __bindgen_anon_2: SHVarPayload__bindgen_ty_1__bindgen_ty_2 {
            stringValue: v.as_ptr(),
            stringLen: 0,
            stringCapacity: 0,
          },
        },
      },
      ..Default::default()
    }
  }
}

impl From<(i64, i64)> for Var {
  #[inline(always)]
  fn from(v: (i64, i64)) -> Self {
    let mut res = SHVar {
      valueType: SHType_Int2,
      ..Default::default()
    };
    unsafe {
      res.payload.__bindgen_anon_1.int2Value[0] = v.0;
      res.payload.__bindgen_anon_1.int2Value[1] = v.1;
    }
    res
  }
}

impl From<(f64, f64)> for Var {
  #[inline(always)]
  fn from(v: (f64, f64)) -> Self {
    let mut res = SHVar {
      valueType: SHType_Float2,
      ..Default::default()
    };
    unsafe {
      res.payload.__bindgen_anon_1.float2Value[0] = v.0;
      res.payload.__bindgen_anon_1.float2Value[1] = v.1;
    }
    res
  }
}

impl From<(f32, f32, f32)> for Var {
  #[inline(always)]
  fn from(v: (f32, f32, f32)) -> Self {
    let mut res = SHVar {
      valueType: SHType_Float3,
      ..Default::default()
    };
    unsafe {
      res.payload.__bindgen_anon_1.float3Value[0] = v.0;
      res.payload.__bindgen_anon_1.float3Value[1] = v.1;
      res.payload.__bindgen_anon_1.float3Value[2] = v.2;
    }
    res
  }
}

impl From<(f32, f32, f32, f32)> for Var {
  #[inline(always)]
  fn from(v: (f32, f32, f32, f32)) -> Self {
    let mut res = SHVar {
      valueType: SHType_Float4,
      ..Default::default()
    };
    unsafe {
      res.payload.__bindgen_anon_1.float4Value[0] = v.0;
      res.payload.__bindgen_anon_1.float4Value[1] = v.1;
      res.payload.__bindgen_anon_1.float4Value[2] = v.2;
      res.payload.__bindgen_anon_1.float4Value[3] = v.3;
    }
    res
  }
}

impl From<&CString> for Var {
  #[inline(always)]
  fn from(v: &CString) -> Self {
    SHVar {
      valueType: SHType_String,
      payload: SHVarPayload {
        __bindgen_anon_1: SHVarPayload__bindgen_ty_1 {
          __bindgen_anon_2: SHVarPayload__bindgen_ty_1__bindgen_ty_2 {
            stringValue: v.as_ptr(),
            stringLen: v.as_bytes().len() as u32,
            stringCapacity: 0,
          },
        },
      },
      ..Default::default()
    }
  }
}

impl From<Option<&CString>> for Var {
  #[inline(always)]
  fn from(v: Option<&CString>) -> Self {
    if let Some(v) = v {
      Var::from(v)
    } else {
      Var::default()
    }
  }
}

impl From<()> for Var {
  #[inline(always)]
  fn from(_: ()) -> Self {
    SHVar {
      valueType: SHType_None,
      ..Default::default()
    }
  }
}

impl From<&Vec<Var>> for Var {
  #[inline(always)]
  fn from(vec: &Vec<Var>) -> Self {
    SHVar {
      valueType: SHType_Seq,
      payload: SHVarPayload {
        __bindgen_anon_1: SHVarPayload__bindgen_ty_1 {
          seqValue: SHSeq {
            elements: vec.as_ptr() as *mut SHVar,
            len: vec.len() as u32,
            cap: 0,
          },
        },
      },
      ..Default::default()
    }
  }
}

impl From<&Vec<ClonedVar>> for Var {
  #[inline(always)]
  fn from(vec: &Vec<ClonedVar>) -> Self {
    SHVar {
      valueType: SHType_Seq,
      payload: SHVarPayload {
        __bindgen_anon_1: SHVarPayload__bindgen_ty_1 {
          seqValue: SHSeq {
            elements: vec.as_ptr() as *mut SHVar,
            len: vec.len() as u32,
            cap: 0,
          },
        },
      },
      ..Default::default()
    }
  }
}

impl From<&[Var]> for Var {
  #[inline(always)]
  fn from(vec: &[Var]) -> Self {
    SHVar {
      valueType: SHType_Seq,
      payload: SHVarPayload {
        __bindgen_anon_1: SHVarPayload__bindgen_ty_1 {
          seqValue: SHSeq {
            elements: vec.as_ptr() as *mut SHVar,
            len: vec.len() as u32,
            cap: 0,
          },
        },
      },
      ..Default::default()
    }
  }
}

impl From<&[u8]> for Var {
  #[inline(always)]
  fn from(vec: &[u8]) -> Self {
    SHVar {
      valueType: SHType_Bytes,
      payload: SHVarPayload {
        __bindgen_anon_1: SHVarPayload__bindgen_ty_1 {
          __bindgen_anon_4: SHVarPayload__bindgen_ty_1__bindgen_ty_4 {
            bytesValue: vec.as_ptr() as *mut u8,
            bytesSize: vec.len() as u32,
            bytesCapacity: 0,
          },
        },
      },
      ..Default::default()
    }
  }
}

impl Var {
  pub fn context_variable(name: &'static str) -> Var {
    let mut v: Var = name.into();
    v.valueType = SHType_ContextVar;
    v
  }

  pub fn new_object<T>(obj: &Rc<T>, info: &Type) -> Var {
    unsafe {
      Var {
        valueType: SHType_Object,
        payload: SHVarPayload {
          __bindgen_anon_1: SHVarPayload__bindgen_ty_1 {
            __bindgen_anon_1: SHVarPayload__bindgen_ty_1__bindgen_ty_1 {
              objectValue: obj as *const Rc<T> as *mut Rc<T> as SHPointer,
              objectVendorId: info.details.object.vendorId,
              objectTypeId: info.details.object.typeId,
            },
          },
        },
        ..Default::default()
      }
    }
  }

  pub unsafe fn new_object_from_ptr<T>(obj: *const T, info: &Type) -> Var {
    Var {
      valueType: SHType_Object,
      payload: SHVarPayload {
        __bindgen_anon_1: SHVarPayload__bindgen_ty_1 {
          __bindgen_anon_1: SHVarPayload__bindgen_ty_1__bindgen_ty_1 {
            objectValue: obj as *mut T as SHPointer,
            objectVendorId: info.details.object.vendorId,
            objectTypeId: info.details.object.typeId,
          },
        },
      },
      ..Default::default()
    }
  }

  pub unsafe fn new_object_from_raw_ptr(obj: SHPointer, info: &Type) -> Var {
    Var {
      valueType: SHType_Object,
      payload: SHVarPayload {
        __bindgen_anon_1: SHVarPayload__bindgen_ty_1 {
          __bindgen_anon_1: SHVarPayload__bindgen_ty_1__bindgen_ty_1 {
            objectValue: obj,
            objectVendorId: info.details.object.vendorId,
            objectTypeId: info.details.object.typeId,
          },
        },
      },
      ..Default::default()
    }
  }

  pub fn from_object_as_clone<T>(var: Var, info: &Type) -> Result<Rc<T>, &str> {
    // use this to store the smart pointer in order to keep it alive
    // this will not allow mutable references btw
    unsafe {
      if var.valueType != SHType_Object
        || var.payload.__bindgen_anon_1.__bindgen_anon_1.objectVendorId
          != info.details.object.vendorId
        || var.payload.__bindgen_anon_1.__bindgen_anon_1.objectTypeId != info.details.object.typeId
      {
        Err("Failed to cast Var into custom Rc<T> object")
      } else {
        let aptr = var.payload.__bindgen_anon_1.__bindgen_anon_1.objectValue as *mut Rc<T>;
        let at = Rc::clone(&*aptr);
        Ok(at)
      }
    }
  }

  // This pattern is often used in shards storing Rcs of Vars
  pub fn get_mut_from_clone<'a, T>(c: &Option<Rc<Option<T>>>) -> Result<&'a mut T, &'a str> {
    let c = c.as_ref().ok_or("No Var reference found")?;
    let c = Rc::as_ptr(c) as *mut Option<T>;
    let c = unsafe { (*c).as_mut().ok_or("Failed to unwrap Rc-ed reference")? };
    Ok(c)
  }

  pub fn from_object_mut_ref<T>(var: Var, info: &Type) -> Result<&mut T, &str> {
    // used to use the object once, when it comes from a simple pointer
    unsafe {
      if var.valueType != SHType_Object
        || var.payload.__bindgen_anon_1.__bindgen_anon_1.objectVendorId
          != info.details.object.vendorId
        || var.payload.__bindgen_anon_1.__bindgen_anon_1.objectTypeId != info.details.object.typeId
      {
        Err("Failed to cast Var into custom &mut T object")
      } else {
        let aptr = var.payload.__bindgen_anon_1.__bindgen_anon_1.objectValue as *mut Rc<T>;
        let p = Rc::as_ptr(&*aptr);
        let mp = p as *mut T;
        Ok(&mut *mp)
      }
    }
  }

  pub fn from_object_ptr_mut_ref<T>(var: Var, info: &Type) -> Result<&mut T, &str> {
    // used to use the object once, when it comes from a Rc
    unsafe {
      if var.valueType != SHType_Object
        || var.payload.__bindgen_anon_1.__bindgen_anon_1.objectVendorId
          != info.details.object.vendorId
        || var.payload.__bindgen_anon_1.__bindgen_anon_1.objectTypeId != info.details.object.typeId
      {
        Err("Failed to cast Var into custom &mut T object")
      } else {
        let aptr = var.payload.__bindgen_anon_1.__bindgen_anon_1.objectValue as *mut T;
        Ok(&mut *aptr)
      }
    }
  }

  pub fn from_object_ptr_ref<T>(var: Var, info: &Type) -> Result<&T, &str> {
    // used to use the object once, when it comes from a Rc
    unsafe {
      if var.valueType != SHType_Object
        || var.payload.__bindgen_anon_1.__bindgen_anon_1.objectVendorId
          != info.details.object.vendorId
        || var.payload.__bindgen_anon_1.__bindgen_anon_1.objectTypeId != info.details.object.typeId
      {
        Err("Failed to cast Var into custom &mut T object")
      } else {
        let aptr = var.payload.__bindgen_anon_1.__bindgen_anon_1.objectValue as *mut T;
        Ok(&*aptr)
      }
    }
  }

  pub fn push<T: Into<Var>>(&mut self, _val: T) {
    unimplemented!();
  }

  pub fn try_push<T: TryInto<Var>>(&mut self, _val: T) {
    unimplemented!();
  }

  pub fn is_seq(&self) -> bool {
    self.valueType == SHType_Seq
  }

  pub fn is_none(&self) -> bool {
    self.valueType == SHType_None
  }

  pub fn is_string(&self) -> bool {
    self.valueType == SHType_String
  }

  pub fn is_path(&self) -> bool {
    self.valueType == SHType_Path
  }

  pub fn as_ref(&self) -> &Self {
    self
  }

  pub fn is_context_var(&self) -> bool {
    self.valueType == SHType_ContextVar
  }

  pub fn enum_value(&self) -> Result<i32, &'static str> {
    if self.valueType != SHType_Enum {
      Err("Variable is not an enum")
    } else {
      unsafe { Ok(self.payload.__bindgen_anon_1.__bindgen_anon_3.enumValue) }
    }
  }
}

impl TryFrom<&Var> for SHString {
  type Error = &'static str;

  #[inline(always)]
  fn try_from(var: &Var) -> Result<Self, Self::Error> {
    if var.valueType != SHType_String
      && var.valueType != SHType_Path
      && var.valueType != SHType_ContextVar
    {
      Err("Expected String, Path or ContextVar variable, but casting failed.")
    } else {
      unsafe { Ok(var.payload.__bindgen_anon_1.__bindgen_anon_2.stringValue) }
    }
  }
}

impl TryFrom<&Var> for std::string::String {
  type Error = &'static str;

  #[inline(always)]
  fn try_from(var: &Var) -> Result<Self, Self::Error> {
    if var.valueType != SHType_String
      && var.valueType != SHType_Path
      && var.valueType != SHType_ContextVar
    {
      Err("Expected String, Path or ContextVar variable, but casting failed.")
    } else {
      unsafe {
        let cstr = CStr::from_ptr(var.payload.__bindgen_anon_1.__bindgen_anon_2.stringValue);
        Ok(std::string::String::from(cstr.to_str().unwrap()))
      }
    }
  }
}

impl TryFrom<&Var> for CString {
  type Error = &'static str;

  #[inline(always)]
  fn try_from(var: &Var) -> Result<Self, Self::Error> {
    if var.valueType != SHType_String
      && var.valueType != SHType_Path
      && var.valueType != SHType_ContextVar
    {
      Err("Expected String, Path or ContextVar variable, but casting failed.")
    } else {
      unsafe {
        let cstr = CStr::from_ptr(var.payload.__bindgen_anon_1.__bindgen_anon_2.stringValue);
        // we need to do this to own the string, this is kinda a burden tho
        Ok(CString::new(cstr.to_str().unwrap()).unwrap())
      }
    }
  }
}

impl TryFrom<&Var> for &CStr {
  type Error = &'static str;

  #[inline(always)]
  fn try_from(var: &Var) -> Result<Self, Self::Error> {
    if var.valueType != SHType_String
      && var.valueType != SHType_Path
      && var.valueType != SHType_ContextVar
    {
      Err("Expected String, Path or ContextVar variable, but casting failed.")
    } else {
      unsafe {
        Ok(CStr::from_ptr(
          var.payload.__bindgen_anon_1.__bindgen_anon_2.stringValue as *mut std::os::raw::c_char,
        ))
      }
    }
  }
}

impl TryFrom<&Var> for Option<CString> {
  type Error = &'static str;

  #[inline(always)]
  fn try_from(var: &Var) -> Result<Self, Self::Error> {
    if var.valueType != SHType_String
      && var.valueType != SHType_Path
      && var.valueType != SHType_ContextVar
      && var.valueType != SHType_None
    {
      Err("Expected None, String, Path or ContextVar variable, but casting failed.")
    } else if var.is_none() {
      Ok(None)
    } else {
      Ok(Some(
        var.try_into().unwrap_or_else(|_| CString::new("").unwrap()),
      ))
    }
  }
}

impl TryFrom<&Var> for &[u8] {
  type Error = &'static str;

  #[inline(always)]
  fn try_from(var: &Var) -> Result<Self, Self::Error> {
    if var.valueType != SHType_Bytes {
      Err("Expected Bytes, but casting failed.")
    } else {
      unsafe {
        Ok(core::slice::from_raw_parts_mut(
          var.payload.__bindgen_anon_1.__bindgen_anon_4.bytesValue,
          var.payload.__bindgen_anon_1.__bindgen_anon_4.bytesSize as usize,
        ))
      }
    }
  }
}

impl TryFrom<&Var> for &str {
  type Error = &'static str;

  #[inline(always)]
  fn try_from(var: &Var) -> Result<Self, Self::Error> {
    if var.valueType != SHType_String
      && var.valueType != SHType_Path
      && var.valueType != SHType_ContextVar
    {
      Err("Expected None, String, Path or ContextVar variable, but casting failed.")
    } else {
      unsafe {
        let cstr = CStr::from_ptr(
          var.payload.__bindgen_anon_1.__bindgen_anon_2.stringValue as *mut std::os::raw::c_char,
        );
        cstr.to_str().map_err(|_| "UTF8 string conversion failed!")
      }
    }
  }
}

impl TryFrom<&Var> for i64 {
  type Error = &'static str;

  #[inline(always)]
  fn try_from(var: &Var) -> Result<Self, Self::Error> {
    if var.valueType != SHType_Int {
      Err("Expected Int variable, but casting failed.")
    } else {
      unsafe { Ok(var.payload.__bindgen_anon_1.intValue) }
    }
  }
}

impl TryFrom<&Var> for i32 {
  type Error = &'static str;

  #[inline(always)]
  fn try_from(var: &Var) -> Result<Self, Self::Error> {
    if var.valueType != SHType_Int {
      Err("Expected Int variable, but casting failed.")
    } else {
      unsafe { Ok(var.payload.__bindgen_anon_1.intValue as i32) }
    }
  }
}

impl TryFrom<&Var> for u64 {
  type Error = &'static str;

  #[inline(always)]
  fn try_from(var: &Var) -> Result<Self, Self::Error> {
    if var.valueType != SHType_Int {
      Err("Expected Int variable, but casting failed.")
    } else {
      unsafe {
        let u = var
          .payload
          .__bindgen_anon_1
          .intValue
          .try_into()
          .map_err(|_| "i64 -> u64 conversion failed, possible overflow.")?;
        Ok(u)
      }
    }
  }
}

impl TryFrom<&Var> for (i64, i64) {
  type Error = &'static str;

  #[inline(always)]
  fn try_from(var: &Var) -> Result<Self, Self::Error> {
    if var.valueType != SHType_Int2 {
      Err("Expected Int2 variable, but casting failed.")
    } else {
      unsafe {
        Ok((
          var.payload.__bindgen_anon_1.int2Value[0],
          var.payload.__bindgen_anon_1.int2Value[1],
        ))
      }
    }
  }
}

impl TryFrom<&Var> for usize {
  type Error = &'static str;

  #[inline(always)]
  fn try_from(var: &Var) -> Result<Self, Self::Error> {
    if var.valueType != SHType_Int {
      Err("Expected Int variable, but casting failed.")
    } else {
      unsafe {
        var
          .payload
          .__bindgen_anon_1
          .intValue
          .try_into()
          .map_err(|_| "Int conversion failed, likely out of range (usize)")
      }
    }
  }
}

impl TryFrom<&Var> for f64 {
  type Error = &'static str;

  #[inline(always)]
  fn try_from(var: &Var) -> Result<Self, Self::Error> {
    if var.valueType != SHType_Float {
      Err("Expected Float variable, but casting failed.")
    } else {
      unsafe { Ok(var.payload.__bindgen_anon_1.floatValue) }
    }
  }
}

impl TryFrom<&Var> for f32 {
  type Error = &'static str;

  #[inline(always)]
  fn try_from(var: &Var) -> Result<Self, Self::Error> {
    if var.valueType != SHType_Float {
      Err("Expected Float variable, but casting failed.")
    } else {
      unsafe { Ok(var.payload.__bindgen_anon_1.floatValue as f32) }
    }
  }
}

impl TryFrom<&Var> for (f32, f32, f32) {
  type Error = &'static str;

  #[inline(always)]
  fn try_from(var: &Var) -> Result<Self, Self::Error> {
    if var.valueType != SHType_Float3 {
      Err("Expected Float3 variable, but casting failed.")
    } else {
      unsafe {
        Ok((
          var.payload.__bindgen_anon_1.float3Value[0],
          var.payload.__bindgen_anon_1.float3Value[1],
          var.payload.__bindgen_anon_1.float3Value[2],
        ))
      }
    }
  }
}

impl TryFrom<&Var> for (f32, f32, f32, f32) {
  type Error = &'static str;

  #[inline(always)]
  fn try_from(var: &Var) -> Result<Self, Self::Error> {
    if var.valueType != SHType_Float4 {
      Err("Expected Float4 variable, but casting failed.")
    } else {
      unsafe {
        Ok((
          var.payload.__bindgen_anon_1.float4Value[0],
          var.payload.__bindgen_anon_1.float4Value[1],
          var.payload.__bindgen_anon_1.float4Value[2],
          var.payload.__bindgen_anon_1.float4Value[3],
        ))
      }
    }
  }
}

impl TryFrom<&Var> for bool {
  type Error = &'static str;

  #[inline(always)]
  fn try_from(var: &Var) -> Result<Self, Self::Error> {
    if var.valueType != SHType_Bool {
      Err("Expected Float variable, but casting failed.")
    } else {
      unsafe { Ok(var.payload.__bindgen_anon_1.boolValue) }
    }
  }
}

impl TryFrom<&Var> for &[Var] {
  type Error = &'static str;

  #[inline(always)]
  fn try_from(var: &Var) -> Result<Self, Self::Error> {
    if var.valueType != SHType_Seq {
      Err("Expected Float variable, but casting failed.")
    } else {
      unsafe {
        let elems = var.payload.__bindgen_anon_1.seqValue.elements;
        let len = var.payload.__bindgen_anon_1.seqValue.len;
        let res = std::slice::from_raw_parts(elems, len as usize);
        Ok(res)
      }
    }
  }
}

impl TryFrom<Var> for &[Var] {
  type Error = &'static str;

  #[inline(always)]
  fn try_from(var: Var) -> Result<Self, Self::Error> {
    if var.valueType != SHType_Seq {
      Err("Expected Float variable, but casting failed.")
    } else {
      unsafe {
        let elems = var.payload.__bindgen_anon_1.seqValue.elements;
        let len = var.payload.__bindgen_anon_1.seqValue.len;
        let res = std::slice::from_raw_parts(elems, len as usize);
        Ok(res)
      }
    }
  }
}

impl TryFrom<Var> for WireRef {
  type Error = &'static str;

  #[inline(always)]
  fn try_from(var: Var) -> Result<Self, Self::Error> {
    if var.valueType != SHType_Wire {
      Err("Expected Wire variable, but casting failed.")
    } else {
      unsafe { Ok(WireRef(var.payload.__bindgen_anon_1.wireValue)) }
    }
  }
}

impl From<WireRef> for Var {
  #[inline(always)]
  fn from(wire: WireRef) -> Self {
    Var {
      valueType: SHType_Wire,
      payload: SHVarPayload {
        __bindgen_anon_1: SHVarPayload__bindgen_ty_1 { wireValue: wire.0 },
      },
      ..Default::default()
    }
  }
}

impl TryFrom<Var> for ShardRef {
  type Error = &'static str;

  #[inline(always)]
  fn try_from(var: Var) -> Result<Self, Self::Error> {
    if var.valueType != SHType_ShardRef {
      Err("Expected Shard variable, but casting failed.")
    } else {
      unsafe { Ok(ShardRef(var.payload.__bindgen_anon_1.shardValue)) }
    }
  }
}

impl TryFrom<&Var> for ShardRef {
  type Error = &'static str;

  #[inline(always)]
  fn try_from(var: &Var) -> Result<Self, Self::Error> {
    if var.valueType != SHType_ShardRef {
      Err("Expected Shard variable, but casting failed.")
    } else {
      unsafe { Ok(ShardRef(var.payload.__bindgen_anon_1.shardValue)) }
    }
  }
}

impl AsRef<Var> for Var {
  #[inline(always)]
  fn as_ref(&self) -> &Var {
    self
  }
}

pub struct ParamVar {
  pub parameter: ClonedVar,
  pub pointee: *mut Var,
}

impl ParamVar {
  pub fn new(initial_value: Var) -> ParamVar {
    ParamVar {
      parameter: initial_value.into(),
      pointee: std::ptr::null_mut(),
    }
  }

  pub fn cleanup(&mut self) {
    unsafe {
      if self.parameter.0.valueType == SHType_ContextVar {
        (*Core).releaseVariable.unwrap()(self.pointee);
      }
      self.pointee = std::ptr::null_mut();
    }
  }

  pub fn warmup(&mut self, context: &Context) {
    if self.parameter.0.valueType == SHType_ContextVar {
      assert_eq!(self.pointee, std::ptr::null_mut());
      unsafe {
        let ctx = context as *const SHContext as *mut SHContext;
        self.pointee = (*Core).referenceVariable.unwrap()(
          ctx,
          self
            .parameter
            .0
            .payload
            .__bindgen_anon_1
            .__bindgen_anon_2
            .stringValue,
        );
      }
    } else {
      self.pointee = &mut self.parameter.0 as *mut Var;
    }
    assert_ne!(self.pointee, std::ptr::null_mut());
  }

  pub fn set(&mut self, value: Var) {
    // avoid overwrite refcount
    assert_ne!(self.pointee, std::ptr::null_mut());
    unsafe {
      let rc = (*self.pointee).refcount;
      (*self.pointee) = value;
      (*self.pointee).flags = value.flags | (SHVAR_FLAGS_REF_COUNTED as u16);
      (*self.pointee).refcount = rc;
    }
  }

  pub fn get(&self) -> Var {
    assert_ne!(self.pointee, std::ptr::null_mut());
    unsafe { *self.pointee }
  }

  pub fn try_get(&self) -> Option<Var> {
    if self.pointee.is_null() {
      None
    } else {
      let v = unsafe { *self.pointee };
      Some(v)
    }
  }

  pub fn set_param(&mut self, value: &Var) {
    self.parameter = value.into();
  }

  pub fn get_param(&self) -> Var {
    self.parameter.0
  }

  pub fn is_variable(&self) -> bool {
    self.parameter.0.valueType == SHType_ContextVar
  }

  pub fn set_name(&mut self, name: &str) {
    self.parameter = name.into();
    self.parameter.0.valueType = SHType_ContextVar;
  }

  pub fn get_name(&self) -> *const std::os::raw::c_char {
    (&self.parameter.0).try_into().unwrap()
  }
}

impl Default for ParamVar {
  fn default() -> Self {
    ParamVar::new(().into())
  }
}

impl Drop for ParamVar {
  fn drop(&mut self) {
    self.cleanup();
  }
}

// Seq / SHSeq

#[derive(Clone)]
pub struct Seq {
  s: SHSeq,
  owned: bool,
}

impl Drop for Seq {
  fn drop(&mut self) {
    if self.owned {
      unsafe {
        (*Core).seqFree.unwrap()(&self.s as *const SHSeq as *mut SHSeq);
      }
    }
  }
}

pub struct SeqIterator {
  s: Seq,
  i: u32,
}

impl Iterator for SeqIterator {
  fn next(&mut self) -> Option<Self::Item> {
    let res = if self.i < self.s.s.len {
      unsafe { Some(*self.s.s.elements.offset(self.i.try_into().unwrap())) }
    } else {
      None
    };
    self.i += 1;
    res
  }
  type Item = Var;
}

impl IntoIterator for Seq {
  fn into_iter(self) -> Self::IntoIter {
    SeqIterator { s: self, i: 0 }
  }
  type Item = Var;
  type IntoIter = SeqIterator;
}

impl Index<usize> for SHSeq {
  #[inline(always)]
  fn index(&self, idx: usize) -> &Self::Output {
    let idx_u32: u32 = idx.try_into().unwrap();
    if idx_u32 < self.len {
      unsafe { &*self.elements.offset(idx.try_into().unwrap()) }
    } else {
      panic!("Index out of range");
    }
  }
  type Output = Var;
}

impl Index<usize> for Seq {
  #[inline(always)]
  fn index(&self, idx: usize) -> &Self::Output {
    &self.s[idx]
  }
  type Output = Var;
}

impl IndexMut<usize> for SHSeq {
  #[inline(always)]
  fn index_mut(&mut self, idx: usize) -> &mut Self::Output {
    let idx_u32: u32 = idx.try_into().unwrap();
    if idx_u32 < self.len {
      unsafe { &mut *self.elements.offset(idx.try_into().unwrap()) }
    } else {
      panic!("Index out of range");
    }
  }
}

impl IndexMut<usize> for Seq {
  #[inline(always)]
  fn index_mut(&mut self, idx: usize) -> &mut Self::Output {
    &mut self.s[idx]
  }
}

impl Seq {
  pub const fn new() -> Seq {
    Seq {
      s: SHSeq {
        elements: core::ptr::null_mut(),
        len: 0,
        cap: 0,
      },
      owned: true,
    }
  }

  pub fn set_len(&mut self, len: usize) {
    unsafe {
      (*Core).seqResize.unwrap()(
        &self.s as *const SHSeq as *mut SHSeq,
        len.try_into().unwrap(),
      );
    }
  }

  pub fn push(&mut self, value: Var) {
    // we need to clone to own the memory shards side
    let mut tmp = SHVar::default();
    cloneVar(&mut tmp, &value);
    unsafe {
      (*Core).seqPush.unwrap()(&self.s as *const SHSeq as *mut SHSeq, &tmp);
    }
  }

  pub fn insert(&mut self, index: usize, value: Var) {
    // we need to clone to own the memory shards side
    let mut tmp = SHVar::default();
    cloneVar(&mut tmp, &value);
    unsafe {
      (*Core).seqInsert.unwrap()(
        &self.s as *const SHSeq as *mut SHSeq,
        index.try_into().unwrap(),
        &tmp,
      );
    }
  }

  pub fn len(&self) -> usize {
    self.s.len.try_into().unwrap()
  }

  pub fn is_empty(&self) -> bool {
    self.s.len == 0
  }

  pub fn pop(&mut self) -> Option<ClonedVar> {
    unsafe {
      if !self.is_empty() {
        let v = (*Core).seqPop.unwrap()(&self.s as *const SHSeq as *mut SHSeq);
        Some(transmute(v))
      } else {
        None
      }
    }
  }

  pub fn clear(&mut self) {
    unsafe {
      (*Core).seqResize.unwrap()(&self.s as *const SHSeq as *mut SHSeq, 0);
    }
  }

  pub fn iter(&self) -> SeqIterator {
    SeqIterator {
      s: self.clone(),
      i: 0,
    }
  }
}

impl Default for Seq {
  fn default() -> Self {
    Seq::new()
  }
}

impl AsRef<Seq> for Seq {
  #[inline(always)]
  fn as_ref(&self) -> &Seq {
    &self
  }
}

impl From<&Seq> for Var {
  #[inline(always)]
  fn from(s: &Seq) -> Self {
    SHVar {
      valueType: SHType_Seq,
      payload: SHVarPayload {
        __bindgen_anon_1: SHVarPayload__bindgen_ty_1 { seqValue: s.s },
      },
      ..Default::default()
    }
  }
}

impl TryFrom<&mut Var> for Seq {
  type Error = &'static str;

  #[inline(always)]
  fn try_from(v: &mut Var) -> Result<Self, Self::Error> {
    // in this case allow None type, we might be a new variable from a Table or Seq
    if v.valueType == SHType_None {
      v.valueType = SHType_Seq;
    }

    if v.valueType != SHType_Seq {
      Err("Expected Seq variable, but casting failed.")
    } else {
      unsafe {
        Ok(Seq {
          s: v.payload.__bindgen_anon_1.seqValue,
          owned: false,
        })
      }
    }
  }
}

impl TryFrom<&Var> for Seq {
  type Error = &'static str;

  #[inline(always)]
  fn try_from(v: &Var) -> Result<Self, Self::Error> {
    if v.valueType != SHType_Seq {
      Err("Expected Seq variable, but casting failed.")
    } else {
      unsafe {
        Ok(Seq {
          s: v.payload.__bindgen_anon_1.seqValue,
          owned: false,
        })
      }
    }
  }
}

impl TryFrom<Var> for Seq {
  type Error = &'static str;

  #[inline(always)]
  fn try_from(v: Var) -> Result<Self, Self::Error> {
    if v.valueType != SHType_Seq {
      Err("Expected Seq variable, but casting failed.")
    } else {
      unsafe {
        Ok(Seq {
          s: v.payload.__bindgen_anon_1.seqValue,
          owned: false,
        })
      }
    }
  }
}

// Table / SHTable

#[derive(Clone)]
pub struct Table {
  pub t: SHTable,
  owned: bool,
}

impl Drop for Table {
  fn drop(&mut self) {
    if self.owned {
      unsafe {
        (*self.t.api).tableFree.unwrap()(self.t);
      }
    }
  }
}

unsafe extern "C" fn table_foreach_callback(
  key: *const ::std::os::raw::c_char,
  value: *mut SHVar,
  userData: *mut ::std::os::raw::c_void,
) -> SHBool {
  let ptrs = userData as *mut (&mut Vec<&str>, &mut Vec<Var>);
  let cstr = CStr::from_ptr(key);
  (*ptrs).0.push(cstr.to_str().unwrap());
  (*ptrs).1.push(*value);
  true // false aborts iteration
}

impl Default for Table {
  fn default() -> Self {
    Self::new()
  }
}

impl AsRef<Table> for Table {
  fn as_ref(&self) -> &Table {
    self
  }
}

impl Table {
  pub fn new() -> Table {
    unsafe {
      Table {
        t: (*Core).tableNew.unwrap()(),
        owned: true,
      }
    }
  }

  pub fn insert(&mut self, k: &CString, v: Var) -> Option<Var> {
    unsafe {
      let cstr = k.as_bytes_with_nul().as_ptr() as *const std::os::raw::c_char;
      if (*self.t.api).tableContains.unwrap()(self.t, cstr) {
        let p = (*self.t.api).tableAt.unwrap()(self.t, cstr);
        let old = *p;
        cloneVar(&mut *p, &v);
        Some(old)
      } else {
        let p = (*self.t.api).tableAt.unwrap()(self.t, cstr);
        *p = v;
        None
      }
    }
  }

  pub fn insert_fast(&mut self, k: &CString, v: Var) {
    unsafe {
      let cstr = k.as_bytes_with_nul().as_ptr() as *const std::os::raw::c_char;
      let p = (*self.t.api).tableAt.unwrap()(self.t, cstr);
      cloneVar(&mut *p, &v);
    }
  }

  pub fn insert_fast_static(&mut self, k: &'static str, v: Var) {
    unsafe {
      let cstr = k.as_ptr() as *const std::os::raw::c_char;
      let p = (*self.t.api).tableAt.unwrap()(self.t, cstr);
      cloneVar(&mut *p, &v);
    }
  }

  pub fn get_mut(&self, k: &CString) -> Option<&mut Var> {
    unsafe {
      let cstr = k.as_bytes_with_nul().as_ptr() as *const std::os::raw::c_char;
      if (*self.t.api).tableContains.unwrap()(self.t, cstr) {
        let p = (*self.t.api).tableAt.unwrap()(self.t, cstr);
        Some(&mut *p)
      } else {
        None
      }
    }
  }

  pub fn get_mut_fast(&mut self, k: &CString) -> &mut Var {
    unsafe {
      let cstr = k.as_bytes_with_nul().as_ptr() as *const std::os::raw::c_char;
      &mut *(*self.t.api).tableAt.unwrap()(self.t, cstr)
    }
  }

  pub fn get_mut_fast_static(&mut self, k: &'static str) -> &mut Var {
    unsafe {
      let cstr = k.as_ptr() as *const std::os::raw::c_char;
      &mut *(*self.t.api).tableAt.unwrap()(self.t, cstr)
    }
  }

  pub fn get(&self, k: &CString) -> Option<&Var> {
    unsafe {
      let cstr = k.as_bytes_with_nul().as_ptr() as *const std::os::raw::c_char;
      if (*self.t.api).tableContains.unwrap()(self.t, cstr) {
        let p = (*self.t.api).tableAt.unwrap()(self.t, cstr);
        Some(&*p)
      } else {
        None
      }
    }
  }

  pub fn get_fast_static(&self, k: &'static str) -> &Var {
    unsafe {
      let cstr = k.as_ptr() as *const std::os::raw::c_char;
      &*(*self.t.api).tableAt.unwrap()(self.t, cstr)
    }
  }

  pub fn iter(&self) -> TableIterator {
    unsafe {
      let it = TableIterator {
        table: self.t,
        citer: [0; 64],
      };
      (*self.t.api).tableGetIterator.unwrap()(self.t, &it.citer as *const _ as *mut _);
      it
    }
  }
}

pub struct TableIterator {
  table: SHTable,
  citer: SHTableIterator,
}

impl Iterator for TableIterator {
  type Item = (String, Var);
  fn next(&mut self) -> Option<Self::Item> {
    unsafe {
      let k: SHString = core::ptr::null();
      let v: SHVar = SHVar::default();
      let hasValue = (*(self.table.api)).tableNext.unwrap()(
        self.table,
        &self.citer as *const _ as *mut _,
        &k as *const _ as *mut _,
        &v as *const _ as *mut _,
      );
      if hasValue {
        Some((String(k), v))
      } else {
        None
      }
    }
  }
}

impl From<SHTable> for Table {
  fn from(t: SHTable) -> Self {
    Table { t, owned: false }
  }
}

impl TryFrom<&Var> for Table {
  type Error = &'static str;

  #[inline(always)]
  fn try_from(var: &Var) -> Result<Self, Self::Error> {
    if var.valueType != SHType_Table {
      Err("Expected Table, but casting failed.")
    } else {
      unsafe { Ok(var.payload.__bindgen_anon_1.tableValue.into()) }
    }
  }
}

impl From<&Table> for Var {
  fn from(t: &Table) -> Self {
    SHVar {
      valueType: SHType_Table,
      payload: SHVarPayload {
        __bindgen_anon_1: SHVarPayload__bindgen_ty_1 { tableValue: t.t },
      },
      ..Default::default()
    }
  }
}

impl PartialEq for Var {
  fn eq(&self, other: &Self) -> bool {
    if self.valueType != other.valueType {
      false
    } else {
      unsafe { (*Core).isEqualVar.unwrap()(self as *const _, other as *const _) }
    }
  }
}

impl PartialEq for Type {
  fn eq(&self, other: &Type) -> bool {
    if self.basicType != other.basicType {
      false
    } else {
      unsafe { (*Core).isEqualType.unwrap()(self as *const _, other as *const _) }
    }
  }
}

pub const FRAG_CC: i32 = 0x66726167; // 'frag'

// TODO share those from C++ ones to reduce binary size
lazy_static! {
  pub static ref ANY_TYPES: Vec<Type> = vec![common_type::any];
  pub static ref ANYS_TYPES: Vec<Type> = vec![common_type::anys];
  pub static ref NONE_TYPES: Vec<Type> = vec![common_type::none];
  pub static ref STRING_TYPES: Vec<Type> = vec![common_type::string];
  pub static ref STRINGS_TYPES: Vec<Type> = vec![common_type::strings];
  pub static ref SEQ_OF_STRINGS: Type = Type::seq(&STRINGS_TYPES);
  pub static ref SEQ_OF_STRINGS_TYPES: Vec<Type> = vec![*SEQ_OF_STRINGS];
  pub static ref INT_TYPES: Vec<Type> = vec![common_type::int];
  pub static ref BYTES_TYPES: Vec<Type> = vec![common_type::bytes];
  pub static ref FLOAT3_TYPES: Vec<Type> = vec![common_type::float3];
  pub static ref FLOAT4_TYPES: Vec<Type> = vec![common_type::float4];
  pub static ref FLOAT4X4_TYPE: Type = {
    let mut t = common_type::float4s;
    t.fixedSize = 4;
    t
  };
  pub static ref FLOAT4X4_TYPES: Vec<Type> = vec![*FLOAT4X4_TYPE];
  pub static ref FLOAT4X4S_TYPE: Type = Type::seq(&FLOAT4X4_TYPES);
  pub static ref FLOAT4X4orS_TYPES: Vec<Type> = vec![*FLOAT4X4_TYPE, *FLOAT4X4S_TYPE];
  pub static ref FLOAT3X3_TYPE: Type = {
    let mut t = common_type::float3s;
    t.fixedSize = 3;
    t
  };
  pub static ref FLOAT3X3_TYPES: Vec<Type> = vec![*FLOAT3X3_TYPE];
  pub static ref FLOAT3X3S_TYPE: Type = Type::seq(&FLOAT3X3_TYPES);
  pub static ref FLOAT4X2_TYPE: Type = {
    let mut t = common_type::float4s;
    t.fixedSize = 2;
    t
  };
  pub static ref FLOAT4X2_TYPES: Vec<Type> = vec![*FLOAT4X2_TYPE];
  pub static ref FLOAT4X2S_TYPE: Type = Type::seq(&FLOAT4X2_TYPES);
  pub static ref ENUM_TYPE: Type = {
    let mut t = common_type::enumeration;
    t.details.enumeration = SHTypeInfo_Details_Enum {
      vendorId: FRAG_CC, // 'frag'
      typeId: 0x74797065, // 'type'
    };
    t
  };
  pub static ref ENUM_TYPES: Vec<Type> = vec![*ENUM_TYPE];
  pub static ref ENUMS_TYPE: Type = Type::seq(&ENUM_TYPES);
}
