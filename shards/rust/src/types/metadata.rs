/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2020 Fragcolor Pte. Ltd. */

//! Type metadata, type info utilities, and From implementations for Var conversions.

use super::*;
use crate::core::{cloneVar, Core};
use crate::shardsc::*;
use crate::SHStringWithLen;
use serde::ser::{SerializeMap, SerializeSeq, SerializeTuple, Serializer};
use serde::Serialize;
use std::ffi::{c_void, CStr, CString};
use std::os::raw::c_char;
use std::slice;

#[derive(PartialEq)]
pub struct String(pub SHString);
#[derive(Default, Clone, Copy)]
pub struct OptionalString(pub SHOptionalString);
pub struct DerivedType(pub Type);

#[macro_export]
macro_rules! shstr {
  ($text:expr) => {{
    use shards::types::RawString;
    const SHSTR: RawString = concat!($text, "\0").as_ptr() as *const std::os::raw::c_char;
    SHSTR
  }};
}

impl Drop for DerivedType {
  fn drop(&mut self) {
    let ti = &mut self.0;
    unsafe {
      (*Core).freeDerivedTypeInfo.unwrap_unchecked()(ti as *mut _);
    }
  }
}


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

fn internal_from_types(types: &[Type]) -> SHTypesInfo {
  let len = types.len();
  SHTypesInfo {
    elements: types.as_ptr() as *mut SHTypeInfo,
    len: len as u32,
    cap: 0,
  }
}

/*
SHExposedTypeInfo & co
*/

impl ExposedInfo {
  pub fn new(name: *const c_char, ctype: SHTypeInfo) -> Self {
    let chelp = core::ptr::null();
    SHExposedTypeInfo {
      exposedType: ctype,
      name,
      help: SHOptionalString {
        string: chelp, // TODO: Pull from parameter into
        crc: 0,
      },
      isMutable: false,
      isProtected: false,
      global: false,
      declared: false,
      trackingMask: 0,
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
      global: false,
      declared: false,
      trackingMask: 0,
    }
  }

  pub fn new_with_help_from_ptr(name: SHString, help: SHOptionalString, ctype: SHTypeInfo) -> Self {
    SHExposedTypeInfo {
      exposedType: ctype,
      name,
      help,
      isMutable: false,
      isProtected: false,
      global: false,
      declared: false,
      trackingMask: 0,
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
      global: false,
      declared: false,
      trackingMask: 0,
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
      global: false,
      declared: false,
      trackingMask: 0,
    }
  }
}

pub type ExposedTypes = Vec<ExposedInfo>;

impl From<SHExposedTypesInfo> for ExposedTypes {
  fn from(types: SHExposedTypesInfo) -> Self {
    let mut exposed_types = Vec::with_capacity(types.len as usize);
    for i in 0..types.len {
      let exposed_type = unsafe { &*types.elements.add(i as usize) };
      // copy is fine, we just care of the vector
      exposed_types.push(*exposed_type);
    }
    exposed_types
  }
}

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
  fn new(name: &'static str, types: &[Type]) -> Self {
    SHParameterInfo {
      name: name.as_ptr() as *mut std::os::raw::c_char,
      help: SHOptionalString {
        string: core::ptr::null(),
        crc: 0,
      },
      valueTypes: internal_from_types(types),
      variableSetter: false,
    }
  }

  fn new1(name: &'static str, help: &'static str, types: &[Type]) -> Self {
    SHParameterInfo {
      name: name.as_ptr() as *mut std::os::raw::c_char,
      help: SHOptionalString {
        string: help.as_ptr() as *mut std::os::raw::c_char,
        crc: 0,
      },
      valueTypes: internal_from_types(types),
      variableSetter: false,
    }
  }

  fn new2(name: &'static str, help: SHOptionalString, types: &[Type]) -> Self {
    SHParameterInfo {
      name: name.as_ptr() as *mut std::os::raw::c_char,
      help,
      valueTypes: internal_from_types(types),
      variableSetter: false,
    }
  }
}

impl From<SHOptionalString> for OptionalString {
  fn from(s: SHOptionalString) -> OptionalString {
    OptionalString(s)
  }
}

impl From<(&'static str, &[Type])> for ParameterInfo {
  fn from(v: (&'static str, &[Type])) -> ParameterInfo {
    ParameterInfo::new(v.0, v.1)
  }
}

impl From<(&'static str, &'static str, &[Type])> for ParameterInfo {
  fn from(v: (&'static str, &'static str, &[Type])) -> ParameterInfo {
    ParameterInfo::new1(v.0, v.1, v.2)
  }
}

impl From<(&'static str, SHOptionalString, &[Type])> for ParameterInfo {
  fn from(v: (&'static str, SHOptionalString, &[Type])) -> ParameterInfo {
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

impl From<SHTypeInfo> for ClonedVar {
  fn from(t: SHTypeInfo) -> Self {
    let mut var = Var::default();
    var.valueType = SHType_Type;
    var.payload.__bindgen_anon_1.typeValue = &t as *const SHTypeInfo as *mut SHTypeInfo; // Directly assign the pointer
    let mut cloned = ClonedVar::default();
    cloneVar(&mut cloned.0, &var);
    cloned
  }
}

impl<'a> SHStringWithLen {
  pub fn str(&'a self) -> &'a str {
    unsafe { self.static_str() }
  }

  pub unsafe fn static_str(&self) -> &'static str {
    if self.len == 0 {
      return "";
    }
    let slice = slice::from_raw_parts(self.string as *const u8, self.len as usize);
    let s = std::str::from_utf8(slice);
    match s {
      Ok(s) => s,
      Err(e) => {
        let valid = e.valid_up_to();
        std::str::from_utf8(&slice[..valid]).unwrap()
      }
    }
  }
}

impl From<&str> for SHStringWithLen {
  fn from(s: &str) -> Self {
    SHStringWithLen {
      string: s.as_ptr() as *const c_char,
      len: s.len() as u64,
    }
  }
}

/*
Static common type infos utility
*/
pub mod common_type {
  use crate::shardsc::SHSeq;
  use crate::shardsc::SHStrings;
  use crate::shardsc::SHTableIndices;
  use crate::shardsc::SHTableTypeInfo;
  use crate::shardsc::SHType;
  use crate::shardsc::SHTypeInfo;
  use crate::shardsc::SHTypeInfo_Details;
  use crate::shardsc::SHType_Any;
  use crate::shardsc::SHType_Audio;
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
  use crate::shardsc::SHType_ShardRef;
  use crate::shardsc::SHType_String;
  use crate::shardsc::SHType_Table;
  use crate::shardsc::SHType_Wire;
  use crate::shardsc::SHTypesInfo;
  use std::ffi::CStr;

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
      recursiveSelf: false,
    }
  }

  pub static none: SHTypeInfo = base_info();

  pub fn type2name(value_type: SHType) -> &'static str {
    unsafe {
      let ptr = (*crate::core::Core).type2Name.unwrap_unchecked()(value_type);
      let s = CStr::from_ptr(ptr);
      s.to_str().unwrap()
    }
  }

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
        recursiveSelf: false,
      };

      pub static $name_table: SHTypeInfo = SHTypeInfo {
        basicType: SHType_Table,
        details: SHTypeInfo_Details {
          table: SHTableTypeInfo {
            keys: SHSeq {
              elements: core::ptr::null_mut(),
              len: 0,
              cap: 0,
            },
            types: SHTypesInfo {
              elements: &$name as *const SHTypeInfo as *mut SHTypeInfo,
              len: 1,
              cap: 0,
            },
            fixedStructTable: false,
            indices: SHTableIndices {
              elements: core::ptr::null_mut(),
              len: 0,
              cap: 0,
            },
          },
        },
        fixedSize: 0,
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
    make_int3,
    SHType_Int3,
    int3,
    int3s,
    int3_var,
    int3s_var,
    int3_table,
    int3_table_var
  );
  shtype!(
    make_int4,
    SHType_Int4,
    int4,
    int4s,
    int4_var,
    int4s_var,
    int4_table,
    int4_table_var
  );
  shtype!(
    make_int8,
    SHType_Int8,
    int8,
    int8s,
    int8_var,
    int8s_var,
    int8_table,
    int8_table_var
  );
  shtype!(
    make_int16,
    SHType_Int16,
    int16,
    int16s,
    int16_var,
    int16s_var,
    int16_table,
    int16_table_var
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
    make_color,
    SHType_Color,
    color,
    colors,
    color_var,
    colors_var,
    color_table,
    color_table_var
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
  shtype!(
    make_audio,
    SHType_Audio,
    audio,
    audios,
    audio_var,
    audios_var,
    audio_table,
    audio_table_var
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
      recursiveSelf: false,
    }
  }

  pub const fn enumeration(vendorId: i32, typeId: i32) -> Type {
    Type {
      basicType: SHType_Enum,
      details: SHTypeInfo_Details {
        enumeration: SHEnumTypeInfo { vendorId, typeId },
      },
      fixedSize: 0,
      recursiveSelf: false,
    }
  }

  pub const fn object(vendorId: i32, typeId: i32) -> Type {
    Type {
      basicType: SHType_Object,
      details: SHTypeInfo_Details {
        object: SHObjectTypeInfo {
          vendorId,
          typeId,
          extInfo: core::ptr::null(),
          extInfoData: core::ptr::null_mut(),
        },
      },
      fixedSize: 0,
      recursiveSelf: false,
    }
  }

  pub const fn table(keys: &[Var], types: &[Type]) -> Type {
    Type {
      basicType: SHType_Table,
      details: SHTypeInfo_Details {
        table: SHTableTypeInfo {
          keys: SHSeq {
            elements: keys.as_ptr() as *mut _,
            len: keys.len() as u32,
            cap: 0,
          },
          types: SHTypesInfo {
            elements: types.as_ptr() as *mut SHTypeInfo,
            len: types.len() as u32,
            cap: 0,
          },
          fixedStructTable: false,
          indices: SHTableIndices {
            elements: core::ptr::null_mut(),
            len: 0,
            cap: 0,
          },
        },
      },
      fixedSize: 0,
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
      recursiveSelf: false,
    }
  }
}

/*
SHVar utility
 */

impl Serialize for SeqVar {
  fn serialize<S>(
    &self,
    se: S,
  ) -> Result<<S as serde::Serializer>::Ok, <S as serde::Serializer>::Error>
  where
    S: serde::Serializer,
  {
    let mut s = se.serialize_seq(Some(self.len()))?;
    for ref value in self.iter() {
      s.serialize_element(value)?;
    }
    s.end()
  }
}

impl Serialize for TableVar {
  fn serialize<S>(
    &self,
    se: S,
  ) -> Result<<S as serde::Serializer>::Ok, <S as serde::Serializer>::Error>
  where
    S: serde::Serializer,
  {
    let mut t = se.serialize_map(None)?;
    for (key, value) in self.iter() {
      let key: &str = key.as_ref().try_into().map_err(serde::ser::Error::custom)?;
      t.serialize_entry(&key, &value)?;
    }
    t.end()
  }
}

impl Serialize for Var {
  fn serialize<S>(
    &self,
    se: S,
  ) -> Result<<S as serde::Serializer>::Ok, <S as serde::Serializer>::Error>
  where
    S: serde::Serializer,
  {
    match self.valueType {
      SHType_None => se.serialize_none(),
      SHType_Any => {
        let mut s = se.serialize_tuple(1)?;
        s.serialize_element(&self.valueType)?;
        s.end()
      }
      SHType_Enum => {
        let value: i32 = unsafe { self.payload.__bindgen_anon_1.__bindgen_anon_3.enumValue };
        let vendor: i32 = unsafe { self.payload.__bindgen_anon_1.__bindgen_anon_3.enumVendorId };
        let type_: i32 = unsafe { self.payload.__bindgen_anon_1.__bindgen_anon_3.enumTypeId };
        let mut s = se.serialize_tuple(4)?;
        s.serialize_element(&self.valueType)?;
        s.serialize_element(&value)?;
        s.serialize_element(&vendor)?;
        s.serialize_element(&type_)?;
        s.end()
      }
      SHType_Bool => {
        let value: bool = unsafe { self.payload.__bindgen_anon_1.boolValue };
        se.serialize_bool(value)
      }
      SHType_Int => {
        let value: i64 = unsafe { self.payload.__bindgen_anon_1.intValue };
        se.serialize_i64(value)
      }
      SHType_Int2 => {
        let mut s = se.serialize_tuple(2)?;
        s.serialize_element(&self.valueType)?;
        unsafe {
          s.serialize_element(&self.payload.__bindgen_anon_1.int2Value)?;
        }
        s.end()
      }
      SHType_Int3 | SHType_Int4 => {
        let mut s = se.serialize_tuple(2)?;
        s.serialize_element(&self.valueType)?;
        unsafe {
          s.serialize_element(&self.payload.__bindgen_anon_1.int4Value)?;
        }
        s.end()
      }
      SHType_Int8 => {
        let mut s = se.serialize_tuple(2)?;
        s.serialize_element(&self.valueType)?;
        unsafe {
          s.serialize_element(&self.payload.__bindgen_anon_1.int8Value)?;
        }
        s.end()
      }
      SHType_Int16 => {
        let mut s = se.serialize_tuple(2)?;
        s.serialize_element(&self.valueType)?;
        unsafe {
          s.serialize_element(&self.payload.__bindgen_anon_1.int16Value)?;
        }
        s.end()
      }
      SHType_Float => {
        let value: f64 = unsafe { self.payload.__bindgen_anon_1.floatValue };
        se.serialize_f64(value)
      }
      SHType_Float2 => {
        let mut s = se.serialize_tuple(2)?;
        s.serialize_element(&self.valueType)?;
        unsafe {
          s.serialize_element(&self.payload.__bindgen_anon_1.float2Value)?;
        }
        s.end()
      }
      SHType_Float3 | SHType_Float4 => {
        let mut s = se.serialize_tuple(2)?;
        s.serialize_element(&self.valueType)?;
        unsafe {
          s.serialize_element(&self.payload.__bindgen_anon_1.float4Value)?;
        }
        s.end()
      }
      SHType_Color => {
        let value0: u8 = unsafe { self.payload.__bindgen_anon_1.colorValue.r };
        let value1: u8 = unsafe { self.payload.__bindgen_anon_1.colorValue.g };
        let value2: u8 = unsafe { self.payload.__bindgen_anon_1.colorValue.b };
        let value3: u8 = unsafe { self.payload.__bindgen_anon_1.colorValue.a };
        let arr = [value0, value1, value2, value3];
        let mut s = se.serialize_tuple(4)?;
        s.serialize_element(&self.valueType)?;
        s.serialize_element(&arr)?;
        s.end()
      }
      SHType_Bytes => {
        let value: &[u8] = self.try_into().unwrap();
        se.serialize_bytes(value)
      }
      SHType_String => {
        let value: &str = self.try_into().unwrap();
        se.serialize_str(value)
      }
      SHType_Path => {
        let value: &str = self.try_into().unwrap();
        let mut s = se.serialize_tuple(2)?;
        s.serialize_element(&self.valueType)?;
        s.serialize_element(&value)?;
        s.end()
      }
      SHType_ContextVar => {
        let value: &str = self.try_into().unwrap();
        let mut s = se.serialize_tuple(2)?;
        s.serialize_element(&self.valueType)?;
        s.serialize_element(&value)?;
        s.end()
      }
      SHType_Image => {
        let image = unsafe {
          self
            .payload
            .__bindgen_anon_1
            .imageValue
            .as_ref()
            .unwrap_unchecked()
        };
        let width = image.width;
        let height = image.height;
        let channels = image.channels;
        let flags = image.flags;
        let data = image.data;
        let pixsize = if (flags as u32 & SHIMAGE_FLAGS_16BITS_INT) == SHIMAGE_FLAGS_16BITS_INT {
          2
        } else if (flags as u32 & SHIMAGE_FLAGS_32BITS_FLOAT) == SHIMAGE_FLAGS_32BITS_FLOAT {
          4
        } else {
          1
        };
        let data = unsafe {
          if data.is_null() {
            &[]
          } else {
            std::slice::from_raw_parts(
              data,
              (width as usize * height as usize * channels as usize * pixsize as usize) as usize,
            )
          }
        };
        let mut s = se.serialize_tuple(6)?;
        s.serialize_element(&self.valueType)?;
        s.serialize_element(&width)?;
        s.serialize_element(&height)?;
        s.serialize_element(&channels)?;
        s.serialize_element(&flags)?;
        s.serialize_element(&data)?;
        s.end()
      }
      SHType_Seq => {
        let seq: SeqVar = self.try_into().unwrap();
        seq.serialize(se)
      }
      SHType_Table => {
        let table: TableVar = self.try_into().unwrap();
        table.serialize(se)
      }
      _ => Err(serde::ser::Error::custom("Unsupported Var type")),
    }
  }
}

// TODO, Fix deserialization, for now never used!

// impl<'de> Deserialize<'de> for AutoSeqVar {
//   fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
//   where
//     D: Deserializer<'de>,
//   {
//     struct SeqVisitor;

//     impl<'de> Visitor<'de> for SeqVisitor {
//       type Value = AutoSeqVar;

//       fn expecting(&self, formatter: &mut std::fmt::Formatter) -> std::fmt::Result {
//         formatter.write_str("a supported Seq value")
//       }

//       fn visit_seq<A>(self, mut seq: A) -> Result<Self::Value, A::Error>
//       where
//         A: SeqAccess<'de>,
//       {
//         let mut dst = AutoSeqVar::new();
//         while let Some(var) = seq.next_element::<ClonedVar>()? {
//           dst.0.push(&var.0);
//         }
//         Ok(dst)
//       }
//     }

//     deserializer.deserialize_seq(SeqVisitor)
//   }
// }

// impl<'de> Deserialize<'de> for AutoTableVar {
//   fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
//   where
//     D: Deserializer<'de>,
//   {
//     struct TableVisitor;

//     impl<'de> Visitor<'de> for TableVisitor {
//       type Value = AutoTableVar;

//       fn expecting(&self, formatter: &mut std::fmt::Formatter) -> std::fmt::Result {
//         formatter.write_str("a supported Table value")
//       }

//       fn visit_map<A>(self, mut map: A) -> Result<Self::Value, A::Error>
//       where
//         A: MapAccess<'de>,
//       {
//         let mut table = AutoTableVar::new();
//         while let Some((key, value)) = map.next_entry::<&str, ClonedVar>()? {
//           let key = Var::ephemeral_string(key);
//           table.0.insert_fast(key, &value.0);
//         }
//         Ok(table)
//       }
//     }

//     deserializer.deserialize_map(TableVisitor)
//   }
// }

// impl<'de> Deserialize<'de> for ClonedVar {
//   fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
//   where
//     D: Deserializer<'de>,
//   {
//     struct VarVisitor;

//     impl<'de> Visitor<'de> for VarVisitor {
//       type Value = ClonedVar;

//       fn expecting(&self, formatter: &mut std::fmt::Formatter) -> std::fmt::Result {
//         formatter.write_str("a supported Var value")
//       }

//       fn visit_seq<A>(self, mut seq: A) -> Result<Self::Value, A::Error>
//       where
//         A: SeqAccess<'de>,
//       {
//         let type_: u8 = seq.next_element()?.unwrap();
//         let mut v = Var::default();
//         v.valueType = type_;
//         match type_ {
//           SHType_None | SHType_Any => {}
//           SHType_Enum => {
//             let value: i32 = seq.next_element()?.unwrap();
//             let vendor: i32 = seq.next_element()?.unwrap();
//             let enum_type: i32 = seq.next_element()?.unwrap();
//             v.payload.__bindgen_anon_1.__bindgen_anon_3.enumValue = value;
//             v.payload.__bindgen_anon_1.__bindgen_anon_3.enumVendorId = vendor;
//             v.payload.__bindgen_anon_1.__bindgen_anon_3.enumTypeId = enum_type;
//           }
//           SHType_Bool => {
//             let value: bool = seq.next_element()?.unwrap();
//             v.payload.__bindgen_anon_1.boolValue = value;
//           }
//           SHType_Int => {
//             let value: i64 = seq.next_element()?.unwrap();
//             v.payload.__bindgen_anon_1.intValue = value;
//           }
//           SHType_Int2 => {
//             let value: [i64; 2] = seq.next_element()?.unwrap();
//             v.payload.__bindgen_anon_1.int2Value = value;
//           }
//           SHType_Int3 | SHType_Int4 => {
//             let value: [i32; 4] = seq.next_element()?.unwrap();
//             v.payload.__bindgen_anon_1.int4Value = value;
//           }
//           SHType_Int8 => {
//             let value: [i16; 8] = seq.next_element()?.unwrap();
//             v.payload.__bindgen_anon_1.int8Value = value;
//           }
//           SHType_Int16 => {
//             let value: [i8; 16] = seq.next_element()?.unwrap();
//             v.payload.__bindgen_anon_1.int16Value = value;
//           }
//           SHType_Float => {
//             let value: f64 = seq.next_element()?.unwrap();
//             v.payload.__bindgen_anon_1.floatValue = value;
//           }
//           SHType_Float2 => {
//             let value: [f64; 2] = seq.next_element()?.unwrap();
//             v.payload.__bindgen_anon_1.float2Value = value;
//           }
//           SHType_Float3 | SHType_Float4 => {
//             let value: [f32; 4] = seq.next_element()?.unwrap();
//             v.payload.__bindgen_anon_1.float4Value = value;
//           }
//           SHType_Color => {
//             let value: [u8; 4] = seq.next_element()?.unwrap();
//             v.payload.__bindgen_anon_1.colorValue.r = value[0];
//             v.payload.__bindgen_anon_1.colorValue.g = value[1];
//             v.payload.__bindgen_anon_1.colorValue.b = value[2];
//             v.payload.__bindgen_anon_1.colorValue.a = value[3];
//           }
//           SHType_Bytes => {
//             let value: &[u8] = seq.next_element()?.unwrap();
//             let len = value.len();
//             let ptr = value.as_ptr();
//             v.payload.__bindgen_anon_1.__bindgen_anon_4.bytesValue = ptr as *mut u8;
//             v.payload.__bindgen_anon_1.__bindgen_anon_4.bytesSize = len as u32;
//           }
//           SHType_String | SHType_Path | SHType_ContextVar => {
//             let value: &str = seq.next_element()?.unwrap();
//             let value = Var::ephemeral_string(value);
//             v = value.into();
//           }
//           SHType_Image => {
//             return ClonedVar::new_image(
//               seq.next_element()?.unwrap(),
//               seq.next_element()?.unwrap(),
//               seq.next_element()?.unwrap(),
//               seq.next_element()?.unwrap(),
//               seq.next_element()?.unwrap(),
//             )
//             .map_err(|s| serde::de::Error::custom(s));
//           }
//           SHType_Seq => {
//             let seq: AutoSeqVar = seq.next_element()?.unwrap();
//             // just reinterpret the sequence as a ClonedVar! (this is safe)
//             return Ok(unsafe { std::mem::transmute(seq) });
//           }
//           SHType_Table => {
//             let table: AutoTableVar = seq.next_element()?.unwrap();
//             // just reinterpret the sequence as a ClonedVar! (this is safe)
//             return Ok(unsafe { std::mem::transmute(table) });
//           }
//           _ => return Err(serde::de::Error::custom("Unsupported Var type")),
//         }
//         Ok(v.into())
//       }
//     }

//     deserializer.deserialize_seq(VarVisitor)
//   }
// }

impl<T> From<T> for ClonedVar
where
  T: Into<Var>,
{
  #[inline]
  fn from(v: T) -> Self {
    let vt: Var = v.into();
    let res = ClonedVar(Var::default());
    unsafe {
      let rv = &res.0 as *const SHVar as *mut SHVar;
      let sv = &vt as *const SHVar;
      (*Core).cloneVar.unwrap_unchecked()(rv, sv);
    }
    res
  }
}

impl<T> From<T> for ExternalVar
where
  T: Into<Var>,
{
  #[inline]
  fn from(v: T) -> Self {
    let vt: Var = v.into();
    let mut res = ExternalVar(Var::default());
    unsafe {
      let rv = &res.0 as *const SHVar as *mut SHVar;
      let sv = &vt as *const SHVar;
      (*Core).cloneVar.unwrap_unchecked()(rv, sv);
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
      (*Core).cloneVar.unwrap_unchecked()(rv, sv);
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
      (*Core).cloneVar.unwrap_unchecked()(rv, sv);
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
      (*Core).cloneVar.unwrap_unchecked()(rv, sv);
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
      (*Core).cloneVar.unwrap_unchecked()(rv, sv);
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
      (*Core).cloneVar.unwrap_unchecked()(rv, sv);
    }
    res
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

macro_rules! var_from_as_i64 {
  ($type:ident, $varfield:ident, $shtype:expr) => {
    impl From<$type> for Var {
      #[inline(always)]
      fn from(v: $type) -> Self {
        SHVar {
          valueType: $shtype,
          payload: SHVarPayload {
            __bindgen_anon_1: SHVarPayload__bindgen_ty_1 {
              $varfield: v as i64,
            },
          },
          ..Default::default()
        }
      }
    }
  };
}

macro_rules! var_from_as_f64 {
  ($type:ident, $varfield:ident, $shtype:expr) => {
    impl From<$type> for Var {
      #[inline(always)]
      fn from(v: $type) -> Self {
        SHVar {
          valueType: $shtype,
          payload: SHVarPayload {
            __bindgen_anon_1: SHVarPayload__bindgen_ty_1 {
              $varfield: v as f64,
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
var_from!(SHColor, colorValue, SHType_Color);
var_from_as_i64!(u8, intValue, SHType_Int);
var_from_as_i64!(u16, intValue, SHType_Int);
var_from_as_i64!(u32, intValue, SHType_Int);
var_from_as_i64!(i32, intValue, SHType_Int);
var_try_from!(u128, intValue, SHType_Int);
var_try_from!(i128, intValue, SHType_Int);
var_try_from!(usize, intValue, SHType_Int);
// var_try_from!(u64, intValue, SHType_Int); // Don't panic!
var_from!(f64, floatValue, SHType_Float);
var_from_as_f64!(f32, floatValue, SHType_Float);

impl From<ShardPtr> for Var {
  #[inline(always)]
  fn from(v: ShardPtr) -> Self {
    SHVar {
      valueType: SHType_ShardRef,
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

impl From<&CStr> for Var {
  #[inline(always)]
  fn from(v: &CStr) -> Self {
    SHVar {
      valueType: SHType_String,
      payload: SHVarPayload {
        __bindgen_anon_1: SHVarPayload__bindgen_ty_1 {
          __bindgen_anon_2: SHVarPayload__bindgen_ty_1__bindgen_ty_2 {
            stringValue: v.as_ptr(),
            stringLen: v.to_bytes().len() as u32,
            stringCapacity: 0,
          },
        },
      },
      ..Default::default()
    }
  }
}

// 64-bit precision :[i64;2]
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

impl From<&[i64; 2]> for Var {
  #[inline(always)]
  fn from(v: &[i64; 2]) -> Self {
    let mut res = Var {
      valueType: SHType_Int2,
      ..Default::default()
    };
    unsafe {
      res.payload.__bindgen_anon_1.int2Value[0] = v[0];
      res.payload.__bindgen_anon_1.int2Value[1] = v[1];
    }
    res
  }
}

// 64-bit precision :u64
impl From<u64> for Var {
  #[inline(always)]
  fn from(v: u64) -> Self {
    let mut res = SHVar {
      valueType: SHType_Int,
      ..Default::default()
    };
    res.payload.__bindgen_anon_1.intValue = v as i64;
    res
  }
}

// 64-bit precision :[u64;2]
impl From<(u64, u64)> for Var {
  #[inline(always)]
  fn from(v: (u64, u64)) -> Self {
    let mut res = SHVar {
      valueType: SHType_Int2,
      ..Default::default()
    };
    unsafe {
      res.payload.__bindgen_anon_1.int2Value[0] = v.0 as i64;
      res.payload.__bindgen_anon_1.int2Value[1] = v.1 as i64;
    }
    res
  }
}

// 64-bit precision :[f64;2]
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

impl From<&[f64; 2]> for Var {
  #[inline(always)]
  fn from(v: &[f64; 2]) -> Self {
    let mut res = Var {
      valueType: crate::shardsc::SHType_Float2,
      ..Default::default()
    };
    unsafe {
      res.payload.__bindgen_anon_1.float2Value[0] = v[0];
      res.payload.__bindgen_anon_1.float2Value[1] = v[1];
    }
    res
  }
}

// 32-bit precision :[i32;2]
impl From<(i32, i32)> for Var {
  #[inline(always)]
  fn from(v: (i32, i32)) -> Self {
    let mut res = SHVar {
      valueType: SHType_Int2,
      ..Default::default()
    };
    unsafe {
      res.payload.__bindgen_anon_1.int2Value[0] = v.0 as i64;
      res.payload.__bindgen_anon_1.int2Value[1] = v.1 as i64;
    }
    res
  }
}

// 32-bit precision :[i32;3]
impl From<(i32, i32, i32)> for Var {
  #[inline(always)]
  fn from(v: (i32, i32, i32)) -> Self {
    let mut res = SHVar {
      valueType: SHType_Int3,
      ..Default::default()
    };
    unsafe {
      res.payload.__bindgen_anon_1.int3Value[0] = v.0;
      res.payload.__bindgen_anon_1.int3Value[1] = v.1;
      res.payload.__bindgen_anon_1.int3Value[2] = v.2;
    }
    res
  }
}

impl From<&[i32; 3]> for Var {
  #[inline(always)]
  fn from(v: &[i32; 3]) -> Self {
    let mut res = Var {
      valueType: SHType_Int3,
      ..Default::default()
    };
    unsafe {
      res.payload.__bindgen_anon_1.int3Value[0] = v[0];
      res.payload.__bindgen_anon_1.int3Value[1] = v[1];
      res.payload.__bindgen_anon_1.int3Value[2] = v[2];
    }
    res
  }
}

// 32-bit precision :[i32;4]
impl From<(i32, i32, i32, i32)> for Var {
  #[inline(always)]
  fn from(v: (i32, i32, i32, i32)) -> Self {
    Var {
      valueType: SHType_Int4,
      payload: SHVarPayload {
        __bindgen_anon_1: SHVarPayload__bindgen_ty_1 {
          int4Value: [v.0, v.1, v.2, v.3],
        },
      },
      ..Default::default()
    }
  }
}

impl From<&[i32; 4]> for Var {
  #[inline(always)]
  fn from(v: &[i32; 4]) -> Self {
    Var {
      valueType: SHType_Int4,
      payload: SHVarPayload {
        __bindgen_anon_1: SHVarPayload__bindgen_ty_1 { int4Value: *v },
      },
      ..Default::default()
    }
  }
}

impl From<&[i16; 8]> for Var {
  #[inline(always)]
  fn from(v: &[i16; 8]) -> Self {
    Var {
      valueType: SHType_Int8,
      payload: SHVarPayload {
        __bindgen_anon_1: SHVarPayload__bindgen_ty_1 { int8Value: *v },
      },
      ..Default::default()
    }
  }
}

impl From<&[i8; 16]> for Var {
  #[inline(always)]
  fn from(v: &[i8; 16]) -> Self {
    Var {
      valueType: SHType_Int16,
      payload: SHVarPayload {
        __bindgen_anon_1: SHVarPayload__bindgen_ty_1 { int16Value: *v },
      },
      ..Default::default()
    }
  }
}

impl From<&[u8; 16]> for Var {
  #[inline(always)]
  fn from(v: &[u8; 16]) -> Self {
    let val = v as *const [u8; 16] as *const [i8; 16];
    let val = unsafe { *val };
    Var {
      valueType: SHType_Int16,
      payload: SHVarPayload {
        __bindgen_anon_1: SHVarPayload__bindgen_ty_1 { int16Value: val },
      },
      ..Default::default()
    }
  }
}

impl TryFrom<&Var> for SHAudio {
  type Error = &'static str;

  fn try_from(v: &Var) -> Result<Self, Self::Error> {
    if v.valueType != SHType_Audio {
      return Err("Invalid type");
    }
    let audio = unsafe { v.payload.__bindgen_anon_1.audioValue };
    Ok(audio)
  }
}

impl TryFrom<&Var> for [i8; 16] {
  type Error = &'static str;

  // convert int16Value into
  #[inline(always)]
  fn try_from(v: &Var) -> Result<Self, Self::Error> {
    if v.valueType != SHType_Int16 {
      return Err("Invalid type");
    }
    let val = unsafe { v.payload.__bindgen_anon_1.int16Value };
    Ok(val)
  }
}

impl TryFrom<&Var> for [u8; 16] {
  type Error = &'static str;

  // convert int16Value into
  #[inline(always)]
  fn try_from(v: &Var) -> Result<Self, Self::Error> {
    if v.valueType != SHType_Int16 {
      return Err("Invalid type");
    }
    let val = unsafe { v.payload.__bindgen_anon_1.int16Value };
    let val = &val as *const [i8; 16] as *const [u8; 16];
    let val = unsafe { *val };
    Ok(val)
  }
}

// 32-bit precision :[u32;2]
impl From<(u32, u32)> for Var {
  #[inline(always)]
  fn from(v: (u32, u32)) -> Self {
    let mut res = SHVar {
      valueType: SHType_Int2,
      ..Default::default()
    };
    unsafe {
      res.payload.__bindgen_anon_1.int2Value[0] = v.0 as i64;
      res.payload.__bindgen_anon_1.int2Value[1] = v.1 as i64;
    }
    res
  }
}

// 32-bit precision :[u32;3]
impl From<(u32, u32, u32)> for Var {
  #[inline(always)]
  fn from(v: (u32, u32, u32)) -> Self {
    let mut res = SHVar {
      valueType: SHType_Int3,
      ..Default::default()
    };
    unsafe {
      res.payload.__bindgen_anon_1.int3Value[0] = v.0 as i32;
      res.payload.__bindgen_anon_1.int3Value[1] = v.1 as i32;
      res.payload.__bindgen_anon_1.int3Value[2] = v.2 as i32;
    }
    res
  }
}

// 32-bit precision :[u32;4]
impl From<(u32, u32, u32, u32)> for Var {
  #[inline(always)]
  fn from(v: (u32, u32, u32, u32)) -> Self {
    let mut res = SHVar {
      valueType: SHType_Int4,
      ..Default::default()
    };
    unsafe {
      res.payload.__bindgen_anon_1.int4Value[0] = v.0 as i32;
      res.payload.__bindgen_anon_1.int4Value[1] = v.1 as i32;
      res.payload.__bindgen_anon_1.int4Value[2] = v.2 as i32;
      res.payload.__bindgen_anon_1.int4Value[3] = v.3 as i32;
    }
    res
  }
}

// 32-bit precision :[f32;2]
impl From<(f32, f32)> for Var {
  #[inline(always)]
  fn from(v: (f32, f32)) -> Self {
    let mut res = SHVar {
      valueType: SHType_Float2,
      ..Default::default()
    };
    unsafe {
      res.payload.__bindgen_anon_1.float2Value[0] = v.0 as f64;
      res.payload.__bindgen_anon_1.float2Value[1] = v.1 as f64;
    }
    res
  }
}

// 32-bit precision :[f32;3]
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

impl From<&[f32; 3]> for Var {
  #[inline(always)]
  fn from(v: &[f32; 3]) -> Self {
    let mut res = Var {
      valueType: crate::shardsc::SHType_Float3,
      ..Default::default()
    };
    unsafe {
      res.payload.__bindgen_anon_1.float3Value[0] = v[0];
      res.payload.__bindgen_anon_1.float3Value[1] = v[1];
      res.payload.__bindgen_anon_1.float3Value[2] = v[2];
    }
    res
  }
}

// 32-bit precision :[f32;4]
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

impl From<&[f32; 4]> for Var {
  #[inline(always)]
  fn from(v: &[f32; 4]) -> Self {
    let mut res = Var {
      valueType: crate::shardsc::SHType_Float4,
      ..Default::default()
    };
    unsafe {
      res.payload.__bindgen_anon_1.float4Value[0] = v[0];
      res.payload.__bindgen_anon_1.float4Value[1] = v[1];
      res.payload.__bindgen_anon_1.float4Value[2] = v[2];
      res.payload.__bindgen_anon_1.float4Value[3] = v[3];
    }
    res
  }
}

// 16-bit precision :[i16;2]
impl From<(i16, i16)> for Var {
  #[inline(always)]
  fn from(v: (i16, i16)) -> Self {
    let mut res = SHVar {
      valueType: SHType_Int2,
      ..Default::default()
    };
    unsafe {
      res.payload.__bindgen_anon_1.int2Value[0] = v.0 as i64;
      res.payload.__bindgen_anon_1.int2Value[1] = v.1 as i64;
    }
    res
  }
}

// 16-bit precision :[i16;3]
impl From<(i16, i16, i16)> for Var {
  #[inline(always)]
  fn from(v: (i16, i16, i16)) -> Self {
    let mut res = SHVar {
      valueType: SHType_Int3,
      ..Default::default()
    };
    unsafe {
      res.payload.__bindgen_anon_1.int3Value[0] = v.0 as i32;
      res.payload.__bindgen_anon_1.int3Value[1] = v.1 as i32;
      res.payload.__bindgen_anon_1.int3Value[2] = v.2 as i32;
    }
    res
  }
}

// 16-bit precision :[i16;4]
impl From<(i16, i16, i16, i16)> for Var {
  #[inline(always)]
  fn from(v: (i16, i16, i16, i16)) -> Self {
    let mut res = SHVar {
      valueType: SHType_Int4,
      ..Default::default()
    };
    unsafe {
      res.payload.__bindgen_anon_1.int4Value[0] = v.0 as i32;
      res.payload.__bindgen_anon_1.int4Value[1] = v.1 as i32;
      res.payload.__bindgen_anon_1.int4Value[2] = v.2 as i32;
      res.payload.__bindgen_anon_1.int4Value[3] = v.3 as i32;
    }
    res
  }
}

// 16-bit precision :[u16;2]
impl From<(u16, u16)> for Var {
  #[inline(always)]
  fn from(v: (u16, u16)) -> Self {
    let mut res = SHVar {
      valueType: SHType_Int2,
      ..Default::default()
    };
    unsafe {
      res.payload.__bindgen_anon_1.int2Value[0] = v.0 as i64;
      res.payload.__bindgen_anon_1.int2Value[1] = v.1 as i64;
    }
    res
  }
}

// 16-bit precision :[u16;3]
impl From<(u16, u16, u16)> for Var {
  #[inline(always)]
  fn from(v: (u16, u16, u16)) -> Self {
    let mut res = SHVar {
      valueType: SHType_Int3,
      ..Default::default()
    };
    unsafe {
      res.payload.__bindgen_anon_1.int3Value[0] = v.0 as i32;
      res.payload.__bindgen_anon_1.int3Value[1] = v.1 as i32;
      res.payload.__bindgen_anon_1.int3Value[2] = v.2 as i32;
    }
    res
  }
}

// 16-bit precision :[u16;4]
impl From<(u16, u16, u16, u16)> for Var {
  #[inline(always)]
  fn from(v: (u16, u16, u16, u16)) -> Self {
    let mut res = SHVar {
      valueType: SHType_Int4,
      ..Default::default()
    };
    unsafe {
      res.payload.__bindgen_anon_1.int4Value[0] = v.0 as i32;
      res.payload.__bindgen_anon_1.int4Value[1] = v.1 as i32;
      res.payload.__bindgen_anon_1.int4Value[2] = v.2 as i32;
      res.payload.__bindgen_anon_1.int4Value[3] = v.3 as i32;
    }
    res
  }
}

// 16-bit precision :[f16;2]
impl From<(half::f16, half::f16)> for Var {
  #[inline(always)]
  fn from(v: (half::f16, half::f16)) -> Self {
    let mut res = SHVar {
      valueType: SHType_Float2,
      ..Default::default()
    };
    unsafe {
      res.payload.__bindgen_anon_1.float2Value[0] = f64::from(v.0);
      res.payload.__bindgen_anon_1.float2Value[1] = f64::from(v.1);
    }
    res
  }
}

// 16-bit precision :[f16;3]
impl From<(half::f16, half::f16, half::f16)> for Var {
  #[inline(always)]
  fn from(v: (half::f16, half::f16, half::f16)) -> Self {
    let mut res = SHVar {
      valueType: SHType_Float3,
      ..Default::default()
    };
    unsafe {
      res.payload.__bindgen_anon_1.float3Value[0] = f32::from(v.0);
      res.payload.__bindgen_anon_1.float3Value[1] = f32::from(v.1);
      res.payload.__bindgen_anon_1.float3Value[2] = f32::from(v.2);
    }
    res
  }
}

// 16-bit precision :[f16;4]
impl From<(half::f16, half::f16, half::f16, half::f16)> for Var {
  #[inline(always)]
  fn from(v: (half::f16, half::f16, half::f16, half::f16)) -> Self {
    let mut res = SHVar {
      valueType: SHType_Float4,
      ..Default::default()
    };
    unsafe {
      res.payload.__bindgen_anon_1.float4Value[0] = f32::from(v.0);
      res.payload.__bindgen_anon_1.float4Value[1] = f32::from(v.1);
      res.payload.__bindgen_anon_1.float4Value[2] = f32::from(v.2);
      res.payload.__bindgen_anon_1.float4Value[3] = f32::from(v.3);
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
