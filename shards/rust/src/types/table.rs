/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2020 Fragcolor Pte. Ltd. */

//! Table types (TableVar, AutoTableVar, Table) and Display implementation for Var.

use super::*;
use crate::core::{cloneVar, destroyVar, Core};
use crate::shardsc::*;
use crate::fourCharacterCode;
use crate::types::common::SHType; // Import the enum to disambiguate from shardsc::SHType
use lazy_static::lazy_static;
use std::ffi::{c_void, CStr};
use std::fmt::{Debug, Formatter};
use std::mem::transmute;
use std::ops::{Index, IndexMut};
use std::slice;

pub const FRAG_CC: i32 = fourCharacterCode(*b"frag");

pub struct TableVar(pub Var);

impl TableVar {
  #[inline(always)]
  pub(crate) fn new() -> TableVar {
    TableVar(Var {
      valueType: SHType_Table,
      payload: SHVarPayload {
        __bindgen_anon_1: SHVarPayload__bindgen_ty_1 {
          tableValue: unsafe { (*Core).tableNew.unwrap_unchecked()() },
        },
      },
      ..Default::default()
    })
  }

  /// Creates a new `SeqVar` that IS NOT DROPPED when it goes out of scope.
  /// The resulting variable should be wrapped inside a `ClonedVar(v)` or `destroyVar` should be called.
  #[inline(always)]
  pub fn leaking_new() -> TableVar {
    Self::new()
  }

  #[inline(always)]
  pub fn insert(&mut self, k: Var, v: &Var) -> Option<Var> {
    unsafe {
      let t = self.0.payload.__bindgen_anon_1.tableValue;
      if (*t.api).tableContains.unwrap_unchecked()(t, k) {
        let p = (*t.api).tableAt.unwrap_unchecked()(t, k);
        let old = *p;
        cloneVar(&mut *p, &v);
        Some(old)
      } else {
        let p = (*t.api).tableAt.unwrap_unchecked()(t, k);
        cloneVar(&mut *p, &v);
        None
      }
    }
  }

  #[inline(always)]
  pub fn emplace(&mut self, k: Var, v: ClonedVar) {
    unsafe {
      let t = self.0.payload.__bindgen_anon_1.tableValue;
      let p = (*t.api).tableAt.unwrap_unchecked()(t, k);
      *p = v.0;
      std::mem::forget(v);
    }
  }

  #[inline(always)]
  pub fn emplace_table(&mut self, k: Var, v: AutoTableVar) {
    unsafe {
      let t = self.0.payload.__bindgen_anon_1.tableValue;
      let p = (*t.api).tableAt.unwrap_unchecked()(t, k);
      *p = v.0 .0;
      std::mem::forget(v);
    }
  }

  #[inline(always)]
  pub fn emplace_seq(&mut self, k: Var, v: AutoSeqVar) {
    unsafe {
      let t = self.0.payload.__bindgen_anon_1.tableValue;
      let p = (*t.api).tableAt.unwrap_unchecked()(t, k);
      *p = v.0 .0;
      std::mem::forget(v);
    }
  }

  #[inline(always)]
  pub fn insert_fast(&mut self, k: Var, v: &Var) {
    unsafe {
      let t = self.0.payload.__bindgen_anon_1.tableValue;
      let p = (*t.api).tableAt.unwrap_unchecked()(t, k);
      cloneVar(&mut *p, &v);
    }
  }

  #[inline(always)]
  pub fn insert_fast_static(&mut self, k: &str, v: &Var) {
    unsafe {
      let t = self.0.payload.__bindgen_anon_1.tableValue;
      let str_key = Var::ephemeral_string(k);
      let p = (*t.api).tableAt.unwrap_unchecked()(t, str_key);
      cloneVar(&mut *p, &v);
    }
  }

  #[inline(always)]
  pub fn get_mut(&self, k: Var) -> Option<&mut Var> {
    unsafe {
      let t = self.0.payload.__bindgen_anon_1.tableValue;
      if (*t.api).tableContains.unwrap_unchecked()(t, k) {
        let p = (*t.api).tableAt.unwrap_unchecked()(t, k);
        Some(&mut *p)
      } else {
        None
      }
    }
  }

  #[inline(always)]
  pub fn get_mut_fast(&mut self, k: Var) -> &mut Var {
    unsafe {
      let t = self.0.payload.__bindgen_anon_1.tableValue;
      &mut *(*t.api).tableAt.unwrap_unchecked()(t, k)
    }
  }

  #[inline(always)]
  pub fn get_mut_fast_static(&mut self, k: &'static str) -> &mut Var {
    unsafe {
      let t = self.0.payload.__bindgen_anon_1.tableValue;
      let str_key = Var::ephemeral_string(k);
      &mut *(*t.api).tableAt.unwrap_unchecked()(t, str_key)
    }
  }

  #[inline(always)]
  pub fn get(&self, k: Var) -> Option<&Var> {
    unsafe {
      let t = self.0.payload.__bindgen_anon_1.tableValue;
      if (*t.api).tableContains.unwrap_unchecked()(t, k) {
        let p = (*t.api).tableAt.unwrap_unchecked()(t, k);
        Some(&*p)
      } else {
        None
      }
    }
  }

  #[inline(always)]
  pub fn get_static(&self, k: &'static str) -> Option<&Var> {
    unsafe {
      let t = self.0.payload.__bindgen_anon_1.tableValue;
      let key_str = Var::ephemeral_string(k);
      if (*t.api).tableContains.unwrap_unchecked()(t, key_str) {
        let p = (*t.api).tableAt.unwrap_unchecked()(t, key_str);
        Some(&*p)
      } else {
        None
      }
    }
  }

  #[inline(always)]
  pub fn get_fast_static(&self, k: &'static str) -> &Var {
    unsafe {
      let t = self.0.payload.__bindgen_anon_1.tableValue;
      let key_str = Var::ephemeral_string(k);
      &*(*t.api).tableAt.unwrap_unchecked()(t, key_str)
    }
  }

  #[inline(always)]
  pub fn len(&self) -> usize {
    unsafe {
      let t = self.0.payload.__bindgen_anon_1.tableValue;
      (*t.api).tableSize.unwrap_unchecked()(t) as usize
    }
  }

  #[inline(always)]
  pub fn iter(&self) -> TableIterator {
    unsafe {
      let t = self.0.payload.__bindgen_anon_1.tableValue;
      let it = TableIterator {
        table: t,
        citer: [0; 64],
      };
      (*t.api).tableGetIterator.unwrap_unchecked()(t, &it.citer as *const _ as *mut _);
      it
    }
  }

  pub fn as_table(&self) -> &Table {
    unsafe {
      let tab_ptr = self.0.payload.__bindgen_anon_1.tableValue.opaque as *const Table;
      &*tab_ptr
    }
  }

  pub fn clear(&mut self) {
    unsafe {
      let t = self.0.payload.__bindgen_anon_1.tableValue;
      (*t.api).tableClear.unwrap_unchecked()(t);
    }
    // Also increase version (used for caching!)
    unsafe { self.0.__bindgen_anon_1.version += 1 };
  }

  pub fn remove(&mut self, k: Var) {
    unsafe {
      let t = self.0.payload.__bindgen_anon_1.tableValue;
      (*t.api).tableRemove.unwrap_unchecked()(t, k);
    }
    // Also increase version (used for caching!)
    unsafe { self.0.__bindgen_anon_1.version += 1 };
  }

  pub fn remove_static(&mut self, k: &'static str) {
    let k = Var::ephemeral_string(k);
    unsafe {
      let t = self.0.payload.__bindgen_anon_1.tableValue;
      (*t.api).tableRemove.unwrap_unchecked()(t, k);
    }
    // Also increase version (used for caching!)
    unsafe { self.0.__bindgen_anon_1.version += 1 };
  }
}

/// A wrapper around `TableVar` that automatically destroys the variable when it goes out of scope.
#[repr(transparent)] // force it same size of original
pub struct AutoTableVar(pub TableVar);

impl Debug for AutoTableVar {
  fn fmt(&self, f: &mut Formatter<'_>) -> std::fmt::Result {
    write!(f, "AutoTableVar")
  }
}

impl Drop for AutoTableVar {
  fn drop(&mut self) {
    destroyVar(&mut self.0 .0);
  }
}

impl AutoTableVar {
  /// Creates a new `AutoTableVar`.
  pub fn new() -> AutoTableVar {
    AutoTableVar(TableVar::new())
  }

  /// Extracts the `Var` from the `AutoTableVar` and leaks it.
  ///
  /// This function destroys the `AutoTableVar` and returns the `Var` contained within it.
  /// The `Var` will not be destroyed when it goes out of scope.
  pub fn leak(&mut self) -> Var {
    std::mem::replace(&mut self.0 .0, Var::default())
  }

  pub fn to_cloned(self) -> ClonedVar {
    unsafe { std::mem::transmute(self) }
  }
}

// Table / SHTable

pub struct Table {
  pub t: SHTable, // note, don't derive clone... cos won't work
  owned: bool,
}

impl Drop for Table {
  fn drop(&mut self) {
    if self.owned {
      unsafe {
        (*self.t.api).tableFree.unwrap_unchecked()(self.t);
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
        t: (*Core).tableNew.unwrap_unchecked()(),
        owned: true,
      }
    }
  }

  pub fn from_sh_table(t: SHTable) -> Table {
    Table { t, owned: false }
  }

  pub fn insert(&mut self, k: Var, v: &Var) -> Option<Var> {
    unsafe {
      if (*self.t.api).tableContains.unwrap_unchecked()(self.t, k) {
        let p = (*self.t.api).tableAt.unwrap_unchecked()(self.t, k);
        let old = *p;
        cloneVar(&mut *p, &v);
        Some(old)
      } else {
        let p = (*self.t.api).tableAt.unwrap_unchecked()(self.t, k);
        cloneVar(&mut *p, &v);
        None
      }
    }
  }

  pub fn insert_fast(&mut self, k: Var, v: &Var) {
    unsafe {
      let p = (*self.t.api).tableAt.unwrap_unchecked()(self.t, k);
      cloneVar(&mut *p, &v);
    }
  }

  pub fn insert_fast_static(&mut self, k: &str, v: &Var) {
    unsafe {
      let k = Var::ephemeral_string(k);
      let p = (*self.t.api).tableAt.unwrap_unchecked()(self.t, k);
      cloneVar(&mut *p, &v);
    }
  }

  pub fn get_mut(&self, k: Var) -> Option<&mut Var> {
    unsafe {
      if (*self.t.api).tableContains.unwrap_unchecked()(self.t, k) {
        let p = (*self.t.api).tableAt.unwrap_unchecked()(self.t, k);
        Some(&mut *p)
      } else {
        None
      }
    }
  }

  pub fn get_mut_fast(&mut self, k: Var) -> &mut Var {
    unsafe { &mut *(*self.t.api).tableAt.unwrap_unchecked()(self.t, k) }
  }

  pub fn get_mut_fast_static(&mut self, k: &'static str) -> &mut Var {
    unsafe {
      let k = Var::ephemeral_string(k);
      &mut *(*self.t.api).tableAt.unwrap_unchecked()(self.t, k)
    }
  }

  pub fn get(&self, k: Var) -> Option<&Var> {
    unsafe {
      if (*self.t.api).tableContains.unwrap_unchecked()(self.t, k) {
        let p = (*self.t.api).tableAt.unwrap_unchecked()(self.t, k);
        Some(&*p)
      } else {
        None
      }
    }
  }

  pub fn get_static(&self, k: &'static str) -> Option<&Var> {
    let k = Var::ephemeral_string(k);
    unsafe {
      if (*self.t.api).tableContains.unwrap_unchecked()(self.t, k) {
        let p = (*self.t.api).tableAt.unwrap_unchecked()(self.t, k);
        Some(&*p)
      } else {
        None
      }
    }
  }

  pub fn get_fast_static(&self, k: &'static str) -> &Var {
    let k = Var::ephemeral_string(k);
    unsafe { &*(*self.t.api).tableAt.unwrap_unchecked()(self.t, k) }
  }

  pub fn len(&self) -> usize {
    unsafe { (*self.t.api).tableSize.unwrap_unchecked()(self.t) as usize }
  }

  pub fn iter(&self) -> TableIterator {
    unsafe {
      let it = TableIterator {
        table: self.t,
        citer: [0; 64],
      };
      (*self.t.api).tableGetIterator.unwrap_unchecked()(self.t, &it.citer as *const _ as *mut _);
      it
    }
  }

  pub fn remove(&mut self, k: Var) {
    unsafe {
      (*self.t.api).tableRemove.unwrap_unchecked()(self.t, k);
    }
  }

  pub fn remove_static(&mut self, k: &'static str) {
    let k = Var::ephemeral_string(k);
    unsafe {
      (*self.t.api).tableRemove.unwrap_unchecked()(self.t, k);
    }
  }
}

pub struct TableIterator {
  pub table: SHTable,
  pub citer: SHTableIterator,
}

impl Iterator for TableIterator {
  type Item = (Var, Var);
  fn next(&mut self) -> Option<Self::Item> {
    unsafe {
      let k: Var = Var::default();
      let v: Var = Var::default();
      let hasValue = (*(self.table.api)).tableNext.unwrap_unchecked()(
        self.table,
        &self.citer as *const _ as *mut _,
        &k as *const _ as *mut _,
        &v as *const _ as *mut _,
      );
      if hasValue {
        Some((k, v))
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

impl TryFrom<&mut Var> for Table {
  type Error = &'static str;

  #[inline(always)]
  fn try_from(var: &mut Var) -> Result<Self, Self::Error> {
    // in this case allow None type, we might be a new variable from a Table or Seq
    if var.valueType == SHType_None {
      var.valueType = SHType_Table;
    }

    if var.valueType != SHType_Table {
      Err("Expected Table variable, but casting failed.")
    } else {
      unsafe { Ok(var.payload.__bindgen_anon_1.tableValue.into()) }
    }
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
      unsafe { (*Core).isEqualVar.unwrap_unchecked()(self as *const _, other as *const _) }
    }
  }
}

impl Eq for Var {}

impl PartialEq for Type {
  fn eq(&self, other: &Type) -> bool {
    if self.basicType != other.basicType {
      false
    } else {
      unsafe { (*Core).isEqualType.unwrap_unchecked()(self as *const _, other as *const _) }
    }
  }
}


pub static INT_TYPES_SLICE: &[Type] = &[common_type::int];
pub static INT_OR_NONE_TYPES_SLICE: &[Type] = &[common_type::int, common_type::none];
pub static INT2_TYPES_SLICE: &[Type] = &[common_type::int2];
pub static FLOAT_TYPES_SLICE: &[Type] = &[common_type::float];
pub static FLOAT_OR_NONE_TYPES_SLICE: &[Type] = &[common_type::float, common_type::none];
pub static FLOAT2_TYPES_SLICE: &[Type] = &[common_type::float2];
pub static FLOAT3_TYPES_SLICE: &[Type] = &[common_type::float3];
pub static BOOL_TYPES_SLICE: &[Type] = &[common_type::bool];
pub static BOOL_OR_NONE_SLICE: &[Type] = &[common_type::bool, common_type::none];
pub static BOOL_OR_VAR_SLICE: &[Type] = &[common_type::bool, common_type::bool_var];
pub static BOOL_VAR_OR_NONE_SLICE: &[Type] =
  &[common_type::bool, common_type::bool_var, common_type::none];
pub static STRING_TYPES_SLICE: &[Type] = &[common_type::string];
pub static STRING_OR_NONE_SLICE: &[Type] = &[common_type::string, common_type::none];
pub static STRINGS_OR_NONE_SLICE: &[Type] = &[common_type::strings, common_type::none];
pub static STRING_VAR_OR_NONE_SLICE: &[Type] = &[
  common_type::string,
  common_type::string_var,
  common_type::none,
];

pub static ANY_TABLE_VAR_NONE_SLICE: &[Type] = &[
  common_type::any_table,
  common_type::any_table_var,
  common_type::none,
];

// TODO share those from C++ ones to reduce binary size
lazy_static! {
  pub static ref ANY_TYPES: Vec<Type> = vec![common_type::any];
  pub static ref WIRE_TYPES: Vec<Type> = vec![common_type::wire];
  pub static ref ANYS_TYPES: Vec<Type> = vec![common_type::anys];
  pub static ref ANY_TABLE_VAR_TYPES: Vec<Type> =
    vec![common_type::any_table, common_type::any_table_var];
  pub static ref ANY_TABLE_TYPES: Vec<Type> = vec![common_type::any_table];
  pub static ref SEQ_OF_ANY_TABLE: Type = Type::seq(&ANY_TABLE_TYPES);
  pub static ref SEQ_OF_ANY_TABLE_TYPES: Vec<Type> = vec![*SEQ_OF_ANY_TABLE, common_type::none];
  pub static ref NONE_TYPES: Vec<Type> = vec![common_type::none];
  pub static ref STRING_TYPES: Vec<Type> = vec![common_type::string];
  pub static ref STRINGS_TYPES: Vec<Type> = vec![common_type::strings];
  pub static ref SEQ_OF_STRINGS: Type = Type::seq(&STRINGS_TYPES);
  pub static ref SEQ_OF_STRINGS_TYPES: Vec<Type> = vec![*SEQ_OF_STRINGS];
  pub static ref SEQ_OF_STRINGS_OR_SEQ_OF_BYTES_TYPES: Vec<Type> =
    vec![*SEQ_OF_STRINGS, *SEQ_OF_BYTES];
  pub static ref SEQ_OF_STRING_OR_BYTE_TYPES: Vec<Type> =
    vec![common_type::string, common_type::bytes];
  pub static ref COLOR_TYPES: Vec<Type> = vec![common_type::color];
  pub static ref INT_TYPES: Vec<Type> = vec![common_type::int];
  pub static ref INT2_TYPES: Vec<Type> = vec![common_type::int2];
  pub static ref INT3_TYPES: Vec<Type> = vec![common_type::int3];
  pub static ref INT4_TYPES: Vec<Type> = vec![common_type::int4];
  pub static ref INT16_TYPES: Vec<Type> = vec![common_type::int16];
  pub static ref FLOAT_TYPES: Vec<Type> = vec![common_type::float];
  pub static ref SEQ_OF_INT: Type = Type::seq(&INT_TYPES);
  pub static ref SEQ_OF_INT_TYPES: Vec<Type> = vec![*SEQ_OF_INT];
  pub static ref SEQ_OF_SEQ_OF_INT: Type = Type::seq(&SEQ_OF_INT_TYPES);
  pub static ref SEQ_OF_SEQ_OF_INT_TYPES: Vec<Type> = vec![*SEQ_OF_SEQ_OF_INT];
  pub static ref SEQ_OF_FLOAT: Type = Type::seq(&FLOAT_TYPES);
  pub static ref SEQ_OF_FLOAT_TYPES: Vec<Type> = vec![*SEQ_OF_FLOAT];
  pub static ref SEQ_OF_SEQ_OF_FLOAT: Type = Type::seq(&SEQ_OF_FLOAT_TYPES);
  pub static ref SEQ_OF_SEQ_OF_FLOAT_TYPES: Vec<Type> = vec![*SEQ_OF_SEQ_OF_FLOAT];
  pub static ref FLOAT2_TYPES: Vec<Type> = vec![common_type::float2];
  pub static ref FLOAT3_TYPES: Vec<Type> = vec![common_type::float3];
  pub static ref FLOAT4_TYPES: Vec<Type> = vec![common_type::float4];
  pub static ref BOOL_TYPES: Vec<Type> = vec![common_type::bool];
  pub static ref BYTES_TYPES: Vec<Type> = vec![common_type::bytes];
  pub static ref SEQ_OF_BYTES: Type = Type::seq(&BYTES_TYPES);
  pub static ref AUDIO_TYPES: Vec<Type> = vec![common_type::audio];
  pub static ref BYTES_OR_STRING_TYPES: Vec<Type> = vec![common_type::bytes, common_type::string];
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
    t.details.enumeration = SHEnumTypeInfo {
      vendorId: FRAG_CC,
      typeId: fourCharacterCode(*b"type"),
    };
    t
  };
  pub static ref ENUM_TYPES: Vec<Type> = vec![*ENUM_TYPE];
  pub static ref ENUMS_TYPE: Type = Type::seq(&ENUM_TYPES);
  pub static ref ENUMS_TYPES: Vec<Type> = vec![*ENUMS_TYPE];
  pub static ref IMAGE_TYPES: Vec<Type> = vec![common_type::image];
  pub static ref SHARDS_OR_NONE_TYPES: Vec<Type> =
    vec![common_type::none, common_type::shard, common_type::shards];
  pub static ref SEQ_OF_SHARDS: Type = Type::seq(&SHARDS_OR_NONE_TYPES);
  pub static ref SEQ_OF_SHARDS_TYPES: Vec<Type> = vec![*SEQ_OF_SHARDS];
  pub static ref SEQ_OF_SEQ_OF_ANY: Type = Type::seq(&ANYS_TYPES);
  pub static ref SEQ_OF_SEQ_OF_ANY_TYPES: Vec<Type> = vec![*SEQ_OF_SEQ_OF_ANY];
  pub static ref SEQ_OF_INT_OR_FLOAT_TYPES: Vec<Type> = vec![*SEQ_OF_INT, *SEQ_OF_FLOAT];
}

macro_rules! test_to_from_vec1 {
  ($type:ty, $value:expr, $msg:literal) => {
    let fromNum: $type = $value;
    let asVar: Var = fromNum.try_into().unwrap();
    let intoNum: $type = <$type>::try_from(&asVar).unwrap();
    assert_eq!(fromNum, intoNum, $msg);
  };
}

macro_rules! test_to_from_vec2 {
  ($type:ty, $value:expr, $msg:literal) => {
    let fromNum: ($type, $type) = ($value, $value);
    let asVar: Var = fromNum.try_into().unwrap();
    let intoNum: ($type, $type) = <($type, $type)>::try_from(&asVar).unwrap();
    assert_eq!(fromNum, intoNum, $msg);
  };
}

macro_rules! test_to_from_vec3 {
  ($type:ty, $value:expr, $msg:literal) => {
    let fromNum: ($type, $type, $type) = ($value, $value, $value);
    let asVar: Var = fromNum.try_into().unwrap();
    let intoNum: ($type, $type, $type) = <($type, $type, $type)>::try_from(&asVar).unwrap();
    assert_eq!(fromNum, intoNum, $msg);
  };
}

macro_rules! test_to_from_vec4 {
  ($type:ty, $value:expr, $msg:literal) => {
    let fromNum: ($type, $type, $type, $type) = ($value, $value, $value, $value);
    let asVar: Var = fromNum.try_into().unwrap();
    let intoNum: ($type, $type, $type, $type) =
      <($type, $type, $type, $type)>::try_from(&asVar).unwrap();
    assert_eq!(fromNum, intoNum, $msg);
  };
}

#[test]
fn precision_conversion() {
  test_to_from_vec1!(i64, i64::MAX, "i64 conversion failed");
  test_to_from_vec1!(i64, i64::MIN, "i64 conversion failed");
  test_to_from_vec1!(u64, u64::MAX, "u64 conversion failed"); // Don't panic!
  test_to_from_vec1!(u64, u64::MIN, "u64 conversion failed");
  test_to_from_vec1!(f64, f64::MAX, "f64 conversion failed");
  test_to_from_vec1!(f64, f64::MIN, "f64 conversion failed");
  test_to_from_vec1!(f64, f64::MIN_POSITIVE, "f64 conversion failed");
  test_to_from_vec1!(f64, f64::EPSILON, "f64 conversion failed");
  test_to_from_vec1!(f64, f64::INFINITY, "f64 conversion failed");

  test_to_from_vec2!(i64, i64::MAX, "[i64,2] conversion failed");
  test_to_from_vec2!(i64, i64::MIN, "[i64,2] conversion failed");
  test_to_from_vec2!(u64, u64::MAX, "[u64,2] conversion failed"); // Don't panic!
  test_to_from_vec2!(u64, u64::MIN, "[u64,2] conversion failed");
  test_to_from_vec2!(f64, f64::MAX, "[f64,2] conversion failed");
  test_to_from_vec2!(f64, f64::MIN, "[f64,2] conversion failed");
  test_to_from_vec2!(f64, f64::MIN_POSITIVE, "[f64,2] conversion failed");
  test_to_from_vec2!(f64, f64::EPSILON, "[f64,2] conversion failed");
  test_to_from_vec2!(f64, f64::INFINITY, "[f64,2] conversion failed");

  test_to_from_vec3!(i32, i32::MAX, "[i32,3] conversion failed");
  test_to_from_vec3!(i32, i32::MIN, "[i32,3] conversion failed");
  test_to_from_vec3!(u32, u32::MAX, "[u32,3] conversion failed"); // Don't panic!
  test_to_from_vec3!(u32, u32::MIN, "[u32,3] conversion failed");
  test_to_from_vec3!(f32, f32::MAX, "[f32,3] conversion failed");
  test_to_from_vec3!(f32, f32::MIN, "[f32,3] conversion failed");
  test_to_from_vec3!(f32, f32::MIN_POSITIVE, "[f32,3] conversion failed");
  test_to_from_vec3!(f32, f32::EPSILON, "[f32,3] conversion failed");
  test_to_from_vec3!(f32, f32::INFINITY, "[f32,3] conversion failed");

  test_to_from_vec4!(i32, i32::MAX, "[i32,4] conversion failed");
  test_to_from_vec4!(i32, i32::MIN, "[i32,4] conversion failed");
  test_to_from_vec4!(u32, u32::MAX, "[u32,4] conversion failed"); // Don't panic!
  test_to_from_vec4!(u32, u32::MIN, "[u32,4] conversion failed");
  test_to_from_vec4!(f32, f32::MAX, "[f32,4] conversion failed");
  test_to_from_vec4!(f32, f32::MIN, "[f32,4] conversion failed");
  test_to_from_vec4!(f32, f32::MIN_POSITIVE, "[f32,4] conversion failed");
  test_to_from_vec4!(f32, f32::EPSILON, "[f32,4] conversion failed");
  test_to_from_vec4!(f32, f32::INFINITY, "[f32,4] conversion failed");

  test_to_from_vec4!(i16, i16::MAX, "[i16,4] conversion failed");
  test_to_from_vec4!(i16, i16::MIN, "[i16,4] conversion failed");
  test_to_from_vec4!(u16, u16::MAX, "[u16,4] conversion failed");
  test_to_from_vec4!(u16, u16::MIN, "[u16,4] conversion failed");
}

impl std::fmt::Display for Var {
  fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
    match self.get_type() {
      SHType::None => write!(f, "none"),
      SHType::Any => write!(f, "Any"),
      SHType::Type => {
        write!(f, "@type(")?;
        // Note: The C++ code references typeValue which would need to be implemented
        // This is just a placeholder based on the pattern
        // format(os, *var.payload.typeValue);
        write!(f, ")")?;
        Ok(())
      }
      SHType::Bool => {
        let value = unsafe { self.payload.__bindgen_anon_1.boolValue };
        write!(f, "{}", if value { "true" } else { "false" })
      }
      SHType::Int => {
        let value = unsafe { self.payload.__bindgen_anon_1.intValue };
        write!(f, "{}", value)
      }
      SHType::Int2 => {
        let values = unsafe { self.payload.__bindgen_anon_1.int2Value };
        write!(f, "@i2({} {})", values[0], values[1])
      }
      SHType::Int3 => {
        let values = unsafe { self.payload.__bindgen_anon_1.int3Value };
        write!(f, "@i3({} {} {})", values[0], values[1], values[2])
      }
      SHType::Int4 => {
        let values = unsafe { self.payload.__bindgen_anon_1.int4Value };
        write!(
          f,
          "@i4({} {} {} {})",
          values[0], values[1], values[2], values[3]
        )
      }
      SHType::Int8 => {
        let values = unsafe { self.payload.__bindgen_anon_1.int8Value };
        write!(f, "@i8(")?;
        for (i, val) in values.iter().enumerate() {
          if i > 0 {
            write!(f, " ")?;
          }
          write!(f, "{}", val)?;
        }
        write!(f, ")")
      }
      SHType::Int16 => {
        let values = unsafe { self.payload.__bindgen_anon_1.int16Value };
        write!(f, "@i16(")?;
        for val in values.iter() {
          write!(f, "{:02x}", *val as u8 & 0xFF)?;
        }
        write!(f, ")")
      }
      SHType::Float => {
        let value = unsafe { self.payload.__bindgen_anon_1.floatValue };
        write!(f, "{}", value)
      }
      SHType::Float2 => {
        let values = unsafe { self.payload.__bindgen_anon_1.float2Value };
        write!(f, "@f2({} {})", values[0], values[1])
      }
      SHType::Float3 => {
        let values = unsafe { self.payload.__bindgen_anon_1.float3Value };
        write!(f, "@f3({} {} {})", values[0], values[1], values[2])
      }
      SHType::Float4 => {
        let values = unsafe { self.payload.__bindgen_anon_1.float4Value };
        write!(
          f,
          "@f4({} {} {} {})",
          values[0], values[1], values[2], values[3]
        )
      }
      SHType::Color => {
        let color = unsafe { &self.payload.__bindgen_anon_1.colorValue };
        write!(
          f,
          "@color({} {} {} {})",
          color.r as i32, color.g as i32, color.b as i32, color.a as i32
        )
      }
      SHType::String => {
        unsafe {
          let string_value = self.payload.__bindgen_anon_1.__bindgen_anon_2.stringValue;
          let string_len = self.payload.__bindgen_anon_1.__bindgen_anon_2.stringLen;

          if !string_value.is_null() && string_len == 0 {
            // Edge case: treat as null-terminated C string
            let cstr = CStr::from_ptr(string_value);
            write!(f, "{}", cstr.to_str().unwrap_or("<invalid UTF-8 string>"))
          } else {
            // Normal case: use as_str() which uses string_len
            match self.as_str() {
              Ok(s) => write!(f, "{}", s),
              Err(_) => write!(f, "<invalid string>"),
            }
          }
        }
      }
      SHType::Path => {
        unsafe {
          let string_value = self.payload.__bindgen_anon_1.__bindgen_anon_2.stringValue;
          let string_len = self.payload.__bindgen_anon_1.__bindgen_anon_2.stringLen;

          if !string_value.is_null() && string_len == 0 {
            // Edge case: treat as null-terminated C string
            let cstr = CStr::from_ptr(string_value);
            write!(
              f,
              "Path: {}",
              cstr.to_str().unwrap_or("<invalid UTF-8 string>")
            )
          } else {
            // Normal case: use as_str() which uses string_len
            match self.as_str() {
              Ok(s) => write!(f, "Path: {}", s),
              Err(_) => write!(f, "<invalid path>"),
            }
          }
        }
      }
      SHType::ContextVar => {
        unsafe {
          let string_value = self.payload.__bindgen_anon_1.__bindgen_anon_2.stringValue;
          let string_len = self.payload.__bindgen_anon_1.__bindgen_anon_2.stringLen;

          if !string_value.is_null() && string_len == 0 {
            // Edge case: treat as null-terminated C string
            let cstr = CStr::from_ptr(string_value);
            write!(
              f,
              "Var: {}",
              cstr.to_str().unwrap_or("<invalid UTF-8 string>")
            )
          } else {
            // Normal case: use as_str() which uses string_len
            match self.as_str() {
              Ok(s) => write!(f, "Var: {}", s),
              Err(_) => write!(f, "<invalid context var>"),
            }
          }
        }
      }
      SHType::Seq => match self.as_seq() {
        Ok(seq) => {
          write!(f, "[")?;
          for i in 0..seq.len() {
            if i > 0 {
              write!(f, " ")?;
            }
            write!(f, "{}", &seq[i])?;
          }
          write!(f, "]")
        }
        Err(_) => write!(f, "<invalid sequence>"),
      },
      SHType::Table => match self.as_table() {
        Ok(table) => {
          write!(f, "{{")?;
          let mut first = true;
          for (k, v) in table.iter() {
            if !first {
              write!(f, " ")?;
            }
            write!(f, "{}: {}", k, v)?;
            first = false;
          }
          write!(f, "}}")
        }
        Err(_) => write!(f, "<invalid table>"),
      },
      SHType::ShardRef => {
        // Adapted from the C++ code which uses var.payload.shardValue->name
        unsafe {
          let shard_ptr = self.payload.__bindgen_anon_1.shardValue;
          if !shard_ptr.is_null() {
            let name_fn = (*shard_ptr).name;
            if let Some(name_fn) = name_fn {
              let name = std::ffi::CStr::from_ptr(name_fn(shard_ptr)).to_string_lossy();
              write!(f, "Shard: {}", name)
            } else {
              write!(f, "Shard: <unnamed>")
            }
          } else {
            write!(f, "Shard: <null>")
          }
        }
      }
      SHType::Image => unsafe {
        let image = self.payload.__bindgen_anon_1.imageValue;
        if !image.is_null() {
          write!(
            f,
            "Image({:x}) Width: {} Height: {} Channels: {}",
            image as usize,
            (*image).width,
            (*image).height,
            (*image).channels
          )
        } else {
          write!(f, "Image(null)")
        }
      },
      SHType::Wire => {
        unsafe {
          let wire_ref = self.payload.__bindgen_anon_1.wireValue;
          if !wire_ref.is_null() {
            // Need to implement sharedFromRef equivalent
            write!(f, "<Wire>")
          } else {
            write!(f, "<Wire: None>")
          }
        }
      }
      SHType::Object => {
        unsafe {
          // This needs access to objectInfo and findObjectInfo
          write!(
            f,
            "Object: 0x{:x} vendor: 0x{:x} type: 0x{:x}",
            self.payload.__bindgen_anon_1.__bindgen_anon_1.objectValue as usize,
            self
              .payload
              .__bindgen_anon_1
              .__bindgen_anon_1
              .objectVendorId,
            self.payload.__bindgen_anon_1.__bindgen_anon_1.objectTypeId
          )
        }
      }
      SHType::Enum => {
        unsafe {
          // This would need access to findEnumInfo
          write!(
            f,
            "Enum: {} vendor: 0x{:x} type: 0x{:x}",
            self.payload.__bindgen_anon_1.__bindgen_anon_3.enumValue,
            self.payload.__bindgen_anon_1.__bindgen_anon_3.enumVendorId,
            self.payload.__bindgen_anon_1.__bindgen_anon_3.enumTypeId
          )
        }
      }
      SHType::Audio => unsafe {
        let audio = self.payload.__bindgen_anon_1.audioValue;
        write!(
          f,
          "Audio SampleRate: {} Samples: {} Channels: {}",
          audio.sampleRate, audio.nsamples, audio.channels
        )
      },
      SHType::Bytes => unsafe {
        write!(
          f,
          "<{} SHType::Bytes>",
          self.payload.__bindgen_anon_1.__bindgen_anon_4.bytesSize
        )
      },
      SHType::Trait => {
        // Would need to implement trait formatting
        write!(f, "<Trait>")
      }
    }
  }
}

// Safety: Table can be safely sent between threads
unsafe impl Send for Table {}
unsafe impl Sync for Table {}
