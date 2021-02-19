use crate::chainblocksc::CBBool;
use crate::chainblocksc::CBChain;
use crate::chainblocksc::CBChainState;
use crate::chainblocksc::CBChainState_Continue;
use crate::chainblocksc::CBChainState_Rebase;
use crate::chainblocksc::CBChainState_Restart;
use crate::chainblocksc::CBChainState_Return;
use crate::chainblocksc::CBChainState_Stop;
use crate::chainblocksc::CBComposeResult;
use crate::chainblocksc::CBContext;
use crate::chainblocksc::CBExposedTypeInfo;
use crate::chainblocksc::CBExposedTypesInfo;
use crate::chainblocksc::CBInstanceData;
use crate::chainblocksc::CBOptionalString;
use crate::chainblocksc::CBParameterInfo;
use crate::chainblocksc::CBParametersInfo;
use crate::chainblocksc::CBPointer;
use crate::chainblocksc::CBSeq;
use crate::chainblocksc::CBString;
use crate::chainblocksc::CBStrings;
use crate::chainblocksc::CBTable;
use crate::chainblocksc::CBTableIterator;
use crate::chainblocksc::CBTypeInfo;
use crate::chainblocksc::CBTypeInfo_Details;
use crate::chainblocksc::CBTypeInfo_Details_Object;
use crate::chainblocksc::CBTypeInfo_Details_Table;
use crate::chainblocksc::CBType_Any;
use crate::chainblocksc::CBType_Array;
use crate::chainblocksc::CBType_Block;
use crate::chainblocksc::CBType_Bool;
use crate::chainblocksc::CBType_Bytes;
use crate::chainblocksc::CBType_Chain;
use crate::chainblocksc::CBType_Color;
use crate::chainblocksc::CBType_ContextVar;
use crate::chainblocksc::CBType_Enum;
use crate::chainblocksc::CBType_Float;
use crate::chainblocksc::CBType_Float2;
use crate::chainblocksc::CBType_Float3;
use crate::chainblocksc::CBType_Float4;
use crate::chainblocksc::CBType_Image;
use crate::chainblocksc::CBType_Int;
use crate::chainblocksc::CBType_Int16;
use crate::chainblocksc::CBType_Int2;
use crate::chainblocksc::CBType_Int3;
use crate::chainblocksc::CBType_Int4;
use crate::chainblocksc::CBType_Int8;
use crate::chainblocksc::CBType_None;
use crate::chainblocksc::CBType_Object;
use crate::chainblocksc::CBType_Path;
use crate::chainblocksc::CBType_Seq;
use crate::chainblocksc::CBType_String;
use crate::chainblocksc::CBType_Table;
use crate::chainblocksc::CBTypesInfo;
use crate::chainblocksc::CBVar;
use crate::chainblocksc::CBVarPayload;
use crate::chainblocksc::CBVarPayload__bindgen_ty_1;
use crate::chainblocksc::CBVarPayload__bindgen_ty_1__bindgen_ty_1;
use crate::chainblocksc::CBVarPayload__bindgen_ty_1__bindgen_ty_2;
use crate::chainblocksc::CBVarPayload__bindgen_ty_1__bindgen_ty_4;
use crate::chainblocksc::CBlock;
use crate::chainblocksc::CBlockPtr;
use crate::chainblocksc::CBlocks;
use crate::chainblocksc::CBIMAGE_FLAGS_16BITS_INT;
use crate::chainblocksc::CBIMAGE_FLAGS_32BITS_FLOAT;
use crate::chainblocksc::CBVAR_FLAGS_REF_COUNTED;
use crate::core::cloneVar;
use crate::core::Core;
use core::convert::TryFrom;
use core::convert::TryInto;
use core::mem::transmute;
use core::ops::Index;
use core::ops::IndexMut;
use core::slice;
use std::ffi::CStr;
use std::ffi::CString;
use std::rc::Rc;

#[macro_export]
macro_rules! cstr {
  ($text:expr) => {
    concat!($text, "\0")
  };
}

pub type Context = CBContext;
pub type Var = CBVar;
pub type Type = CBTypeInfo;
pub type InstanceData = CBInstanceData;
pub type Chain = CBChain;
pub type ComposeResult = CBComposeResult;
pub type Block = CBlock;
pub type ExposedInfo = CBExposedTypeInfo;
pub type ParameterInfo = CBParameterInfo;

#[derive(PartialEq)]
pub struct String(pub CBString);
pub struct OptionalString(pub CBOptionalString);

#[derive(PartialEq)]
pub enum ChainState {
  Continue,
  Return,
  Rebase,
  Restart,
  Stop,
}

impl From<CBChainState> for ChainState {
  fn from(state: CBChainState) -> Self {
    match state {
      CBChainState_Continue => ChainState::Continue,
      CBChainState_Return => ChainState::Return,
      CBChainState_Rebase => ChainState::Rebase,
      CBChainState_Restart => ChainState::Restart,
      CBChainState_Stop => ChainState::Stop,
      _ => unreachable!(),
    }
  }
}

unsafe impl Send for Var {}
unsafe impl Send for Context {}
unsafe impl Send for Block {}

/*
CBTypeInfo & co
*/
unsafe impl std::marker::Sync for CBTypeInfo {}
unsafe impl std::marker::Sync for CBExposedTypeInfo {}
unsafe impl std::marker::Sync for CBParameterInfo {}
unsafe impl std::marker::Sync for CBStrings {}

// Todo CBTypeInfo proper wrapper Type with helpers

pub type Types = Vec<Type>;

impl From<&Types> for CBTypesInfo {
  fn from(types: &Types) -> Self {
    CBTypesInfo {
      elements: types.as_ptr() as *mut CBTypeInfo,
      len: types.len() as u32,
      cap: 0,
    }
  }
}

impl From<&[Type]> for CBTypesInfo {
  fn from(types: &[Type]) -> Self {
    CBTypesInfo {
      elements: types.as_ptr() as *mut CBTypeInfo,
      len: types.len() as u32,
      cap: 0,
    }
  }
}

fn internal_from_types(types: Types) -> CBTypesInfo {
  let len = types.len();
  let boxed = types.into_boxed_slice();
  CBTypesInfo {
    elements: Box::into_raw(boxed) as *mut CBTypeInfo,
    len: len as u32,
    cap: 0,
  }
}

unsafe fn internal_drop_types(types: CBTypesInfo) {
  // use with care
  let elems = Box::from_raw(types.elements);
  drop(elems);
}

/*
CBExposedTypeInfo & co
*/

impl ExposedInfo {
  pub fn new(name: &CString, ctype: CBTypeInfo) -> Self {
    let chelp = core::ptr::null();
    CBExposedTypeInfo {
      exposedType: ctype,
      name: name.as_ptr(),
      help: CBOptionalString {
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

  pub fn new_with_help(name: &CString, help: &CString, ctype: CBTypeInfo) -> Self {
    CBExposedTypeInfo {
      exposedType: ctype,
      name: name.as_ptr(),
      help: CBOptionalString {
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

  pub const fn new_static(name: &'static str, ctype: CBTypeInfo) -> Self {
    let cname = name.as_ptr() as *const i8;
    let chelp = core::ptr::null();
    CBExposedTypeInfo {
      exposedType: ctype,
      name: cname,
      help: CBOptionalString {
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
    help: CBOptionalString,
    ctype: CBTypeInfo,
  ) -> Self {
    let cname = name.as_ptr() as *const i8;
    CBExposedTypeInfo {
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

impl From<&ExposedTypes> for CBExposedTypesInfo {
  fn from(vec: &ExposedTypes) -> Self {
    CBExposedTypesInfo {
      elements: vec.as_ptr() as *mut CBExposedTypeInfo,
      len: vec.len() as u32,
      cap: 0,
    }
  }
}

/*
CBParameterInfo & co
*/
impl ParameterInfo {
  fn new(name: &'static str, types: Types) -> Self {
    CBParameterInfo {
      name: name.as_ptr() as *mut i8,
      help: CBOptionalString {
        string: core::ptr::null(),
        crc: 0,
      },
      valueTypes: internal_from_types(types),
    }
  }

  fn new1(name: &'static str, help: &'static str, types: Types) -> Self {
    CBParameterInfo {
      name: name.as_ptr() as *mut i8,
      help: CBOptionalString {
        string: help.as_ptr() as *mut i8,
        crc: 0,
      },
      valueTypes: internal_from_types(types),
    }
  }

  fn new2(name: &'static str, help: CBOptionalString, types: Types) -> Self {
    CBParameterInfo {
      name: name.as_ptr() as *mut i8,
      help,
      valueTypes: internal_from_types(types),
    }
  }
}

impl From<&str> for OptionalString {
  fn from(s: &str) -> OptionalString {
    let cos = CBOptionalString {
      string: s.as_ptr() as *const i8,
      crc: 0, // TODO
    };
    OptionalString(cos)
  }
}

impl From<&str> for CBOptionalString {
  fn from(s: &str) -> CBOptionalString {
    CBOptionalString {
      string: s.as_ptr() as *const i8,
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

impl From<(&'static str, CBOptionalString, Types)> for ParameterInfo {
  fn from(v: (&'static str, CBOptionalString, Types)) -> ParameterInfo {
    ParameterInfo::new2(v.0, v.1, v.2)
  }
}

pub type Parameters = Vec<ParameterInfo>;

impl From<&Parameters> for CBParametersInfo {
  fn from(vec: &Parameters) -> Self {
    CBParametersInfo {
      elements: vec.as_ptr() as *mut CBParameterInfo,
      len: vec.len() as u32,
      cap: 0,
    }
  }
}

impl From<CBParametersInfo> for &[CBParameterInfo] {
  fn from(_: CBParametersInfo) -> Self {
    unimplemented!()
  }
}

/*
Static common type infos utility
*/
pub mod common_type {
  use crate::chainblocksc::CBStrings;
  use crate::chainblocksc::CBTypeInfo;
  use crate::chainblocksc::CBTypeInfo_Details;
  use crate::chainblocksc::CBTypeInfo_Details_Table;
  use crate::chainblocksc::CBType_Any;
  use crate::chainblocksc::CBType_Block;
  use crate::chainblocksc::CBType_Bool;
  use crate::chainblocksc::CBType_Bytes;
  use crate::chainblocksc::CBType_Chain;
  use crate::chainblocksc::CBType_ContextVar;
  use crate::chainblocksc::CBType_Float;
  use crate::chainblocksc::CBType_Int;
  use crate::chainblocksc::CBType_None;
  use crate::chainblocksc::CBType_Path;
  use crate::chainblocksc::CBType_Seq;
  use crate::chainblocksc::CBType_String;
  use crate::chainblocksc::CBType_Table;
  use crate::chainblocksc::CBTypesInfo;
  use crate::types::CBType_Float2;
  use crate::types::CBType_Float3;
  use crate::types::CBType_Float4;
  use crate::types::CBType_Object;

  const fn base_info() -> CBTypeInfo {
    CBTypeInfo {
      basicType: CBType_None,
      details: CBTypeInfo_Details {
        seqTypes: CBTypesInfo {
          elements: core::ptr::null_mut(),
          len: 0,
          cap: 0,
        },
      },
      fixedSize: 0,
      innerType: CBType_None,
      recursiveSelf: false,
    }
  }

  pub static none: CBTypeInfo = base_info();

  macro_rules! cbtype {
    ($fname:ident, $type:expr, $name:ident, $names:ident, $name_var:ident, $name_table:ident, $name_table_var:ident) => {
      const fn $fname() -> CBTypeInfo {
        let mut res = base_info();
        res.basicType = $type;
        res
      }

      pub static $name: CBTypeInfo = $fname();

      pub static $names: CBTypeInfo = CBTypeInfo {
        basicType: CBType_Seq,
        details: CBTypeInfo_Details {
          seqTypes: CBTypesInfo {
            elements: &$name as *const CBTypeInfo as *mut CBTypeInfo,
            len: 1,
            cap: 0,
          },
        },
        fixedSize: 0,
        innerType: CBType_None,
        recursiveSelf: false,
      };

      pub static $name_table: CBTypeInfo = CBTypeInfo {
        basicType: CBType_Table,
        details: CBTypeInfo_Details {
          table: CBTypeInfo_Details_Table {
            keys: CBStrings {
              elements: core::ptr::null_mut(),
              len: 0,
              cap: 0,
            },
            types: CBTypesInfo {
              elements: &$name as *const CBTypeInfo as *mut CBTypeInfo,
              len: 1,
              cap: 0,
            },
          },
        },
        fixedSize: 0,
        innerType: CBType_None,
        recursiveSelf: false,
      };

      pub static $name_var: CBTypeInfo = CBTypeInfo {
        basicType: CBType_ContextVar,
        details: CBTypeInfo_Details {
          contextVarTypes: CBTypesInfo {
            elements: &$name as *const CBTypeInfo as *mut CBTypeInfo,
            len: 1,
            cap: 0,
          },
        },
        fixedSize: 0,
        innerType: CBType_None,
        recursiveSelf: false,
      };

      pub static $name_table_var: CBTypeInfo = CBTypeInfo {
        basicType: CBType_ContextVar,
        details: CBTypeInfo_Details {
          contextVarTypes: CBTypesInfo {
            elements: &$name_table as *const CBTypeInfo as *mut CBTypeInfo,
            len: 1,
            cap: 0,
          },
        },
        fixedSize: 0,
        innerType: CBType_None,
        recursiveSelf: false,
      };
    };
  }

  cbtype!(
    make_any,
    CBType_Any,
    any,
    anys,
    any_var,
    any_table,
    any_table_var
  );
  cbtype!(
    make_object,
    CBType_Object,
    object,
    objects,
    object_var,
    object_table,
    object_table_var
  );
  cbtype!(
    make_string,
    CBType_String,
    string,
    strings,
    string_var,
    string_table,
    string_table_var
  );
  cbtype!(
    make_bytes,
    CBType_Bytes,
    bytes,
    bytezs,
    bytes_var,
    bytes_table,
    bytes_table_var
  );
  cbtype!(
    make_int,
    CBType_Int,
    int,
    ints,
    int_var,
    int_table,
    int_table_var
  );
  cbtype!(
    make_float,
    CBType_Float,
    float,
    floats,
    float_var,
    float_table,
    float_table_var
  );
  cbtype!(
    make_float2,
    CBType_Float2,
    float2,
    float2s,
    float2_var,
    float2_table,
    float2_table_var
  );
  cbtype!(
    make_float3,
    CBType_Float3,
    float3,
    float3s,
    float3_var,
    float3_table,
    float3_table_var
  );
  cbtype!(
    make_float4,
    CBType_Float4,
    float4,
    float4s,
    float4_var,
    float4_table,
    float4_table_var
  );
  cbtype!(
    make_bool,
    CBType_Bool,
    bool,
    bools,
    bool_var,
    bool_table,
    bool_table_var
  );
  cbtype!(
    make_block,
    CBType_Block,
    block,
    blocks,
    block_var,
    block_table,
    block_table_var
  );
  cbtype!(
    make_chain,
    CBType_Chain,
    chain,
    chains,
    chain_var,
    chain_table,
    chain_table_var
  );
  cbtype!(
    make_path,
    CBType_Path,
    path,
    paths,
    path_var,
    path_table,
    path_table_var
  );
}

impl Type {
  pub const fn context_variable(types: &[Type]) -> Type {
    Type {
      basicType: CBType_ContextVar,
      details: CBTypeInfo_Details {
        contextVarTypes: CBTypesInfo {
          elements: types.as_ptr() as *mut CBTypeInfo,
          len: types.len() as u32,
          cap: 0,
        },
      },
      fixedSize: 0,
      innerType: CBType_None,
      recursiveSelf: false,
    }
  }

  pub const fn object(vendorId: i32, typeId: i32) -> Type {
    Type {
      basicType: CBType_Object,
      details: CBTypeInfo_Details {
        object: CBTypeInfo_Details_Object { vendorId, typeId },
      },
      fixedSize: 0,
      innerType: CBType_None,
      recursiveSelf: false,
    }
  }

  pub const fn table(keys: &[&'static str], types: &[Type]) -> Type {
    Type {
      basicType: CBType_Table,
      details: CBTypeInfo_Details {
        table: CBTypeInfo_Details_Table {
          keys: CBStrings {
            elements: keys.as_ptr() as *mut *const i8,
            len: keys.len() as u32,
            cap: 0,
          },
          types: CBTypesInfo {
            elements: types.as_ptr() as *mut CBTypeInfo,
            len: types.len() as u32,
            cap: 0,
          },
        },
      },
      fixedSize: 0,
      innerType: CBType_None,
      recursiveSelf: false,
    }
  }

  pub const fn seq(types: &[Type]) -> Type {
    Type {
      basicType: CBType_Seq,
      details: CBTypeInfo_Details {
        seqTypes: CBTypesInfo {
          elements: types.as_ptr() as *mut CBTypeInfo,
          len: types.len() as u32,
          cap: 0,
        },
      },
      fixedSize: 0,
      innerType: CBType_None,
      recursiveSelf: false,
    }
  }
}

/*
CBVar utility
 */

#[repr(transparent)] // force it same size of original
pub struct ClonedVar(pub Var);

#[repr(transparent)] // force it same size of original
pub struct WrappedVar(pub Var); // used in DSL macro, ignore it

impl<T> From<T> for ClonedVar
where
  T: Into<Var>,
{
  fn from(v: T) -> Self {
    let vt: Var = v.into();
    let res = ClonedVar(Var::default());
    unsafe {
      let rv = &res.0 as *const CBVar as *mut CBVar;
      let sv = &vt as *const CBVar;
      (*Core).cloneVar.unwrap()(rv, sv);
    }
    res
  }
}

impl From<&Var> for ClonedVar {
  fn from(v: &Var) -> Self {
    let res = ClonedVar(Var::default());
    unsafe {
      let rv = &res.0 as *const CBVar as *mut CBVar;
      let sv = v as *const CBVar;
      (*Core).cloneVar.unwrap()(rv, sv);
    }
    res
  }
}

impl From<Vec<Var>> for ClonedVar {
  fn from(v: Vec<Var>) -> Self {
    let tmp = Var::from(&v);
    let res = ClonedVar(Var::default());
    unsafe {
      let rv = &res.0 as *const CBVar as *mut CBVar;
      let sv = &tmp as *const CBVar;
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
      let rv = &res.0 as *const CBVar as *mut CBVar;
      let sv = &tmp as *const CBVar;
      (*Core).cloneVar.unwrap()(rv, sv);
    }
    res
  }
}

impl From<&[ClonedVar]> for ClonedVar {
  fn from(vec: &[ClonedVar]) -> Self {
    let res = ClonedVar(Var::default());
    unsafe {
      let src: &[Var] = &*(vec as *const [ClonedVar] as *const [CBVar]);
      let vsrc: Var = src.into();
      let rv = &res.0 as *const CBVar as *mut CBVar;
      let sv = &vsrc as *const CBVar;
      (*Core).cloneVar.unwrap()(rv, sv);
    }
    res
  }
}

impl Drop for ClonedVar {
  #[inline(always)]
  fn drop(&mut self) {
    unsafe {
      let rv = &self.0 as *const CBVar as *mut CBVar;
      (*Core).destroyVar.unwrap()(rv);
    }
  }
}

macro_rules! var_from {
  ($type:ident, $varfield:ident, $cbtype:expr) => {
    impl From<$type> for Var {
      #[inline(always)]
      fn from(v: $type) -> Self {
        CBVar {
          valueType: $cbtype,
          payload: CBVarPayload {
            __bindgen_anon_1: CBVarPayload__bindgen_ty_1 { $varfield: v },
          },
          ..Default::default()
        }
      }
    }
  };
}

macro_rules! var_from_into {
  ($type:ident, $varfield:ident, $cbtype:expr) => {
    impl From<$type> for Var {
      #[inline(always)]
      fn from(v: $type) -> Self {
        CBVar {
          valueType: $cbtype,
          payload: CBVarPayload {
            __bindgen_anon_1: CBVarPayload__bindgen_ty_1 {
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
  ($type:ident, $varfield:ident, $cbtype:expr) => {
    impl TryFrom<$type> for Var {
      type Error = &'static str;

      #[inline(always)]
      fn try_from(v: $type) -> Result<Self, Self::Error> {
        Ok(CBVar {
          valueType: $cbtype,
          payload: CBVarPayload {
            __bindgen_anon_1: CBVarPayload__bindgen_ty_1 {
              $varfield: v
                .try_into()
                .or_else(|_| Err("Conversion failed, value out of range"))?,
            },
          },
          ..Default::default()
        })
      }
    }
  };
}

var_from!(bool, boolValue, CBType_Bool);
var_from!(i64, intValue, CBType_Int);
var_from_into!(i32, intValue, CBType_Int);
var_try_from!(usize, intValue, CBType_Int);
var_try_from!(u64, intValue, CBType_Int);
var_from!(f64, floatValue, CBType_Float);
var_from_into!(f32, floatValue, CBType_Float);

impl From<CBlockPtr> for Var {
  #[inline(always)]
  fn from(v: CBlockPtr) -> Self {
    CBVar {
      valueType: CBType_String,
      payload: CBVarPayload {
        __bindgen_anon_1: CBVarPayload__bindgen_ty_1 { blockValue: v },
      },
      ..Default::default()
    }
  }
}

impl From<CBString> for Var {
  #[inline(always)]
  fn from(v: CBString) -> Self {
    CBVar {
      valueType: CBType_String,
      payload: CBVarPayload {
        __bindgen_anon_1: CBVarPayload__bindgen_ty_1 {
          __bindgen_anon_2: CBVarPayload__bindgen_ty_1__bindgen_ty_2 {
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
    CBVar {
      valueType: CBType_String,
      payload: CBVarPayload {
        __bindgen_anon_1: CBVarPayload__bindgen_ty_1 {
          __bindgen_anon_2: CBVarPayload__bindgen_ty_1__bindgen_ty_2 {
            stringValue: v.as_ptr() as *const i8,
            stringLen: v.len() as u32,
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
    CBVar {
      valueType: CBType_String,
      payload: CBVarPayload {
        __bindgen_anon_1: CBVarPayload__bindgen_ty_1 {
          __bindgen_anon_2: CBVarPayload__bindgen_ty_1__bindgen_ty_2 {
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

impl From<(f32, f32, f32)> for Var {
  #[inline(always)]
  fn from(v: (f32, f32, f32)) -> Self {
    let mut res = CBVar {
      valueType: CBType_Float3,
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
    let mut res = CBVar {
      valueType: CBType_Float4,
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
    CBVar {
      valueType: CBType_String,
      payload: CBVarPayload {
        __bindgen_anon_1: CBVarPayload__bindgen_ty_1 {
          __bindgen_anon_2: CBVarPayload__bindgen_ty_1__bindgen_ty_2 {
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
    CBVar {
      valueType: CBType_None,
      ..Default::default()
    }
  }
}

impl From<&Vec<Var>> for Var {
  #[inline(always)]
  fn from(vec: &Vec<Var>) -> Self {
    CBVar {
      valueType: CBType_Seq,
      payload: CBVarPayload {
        __bindgen_anon_1: CBVarPayload__bindgen_ty_1 {
          seqValue: CBSeq {
            elements: vec.as_ptr() as *mut CBVar,
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
    CBVar {
      valueType: CBType_Seq,
      payload: CBVarPayload {
        __bindgen_anon_1: CBVarPayload__bindgen_ty_1 {
          seqValue: CBSeq {
            elements: vec.as_ptr() as *mut CBVar,
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
    CBVar {
      valueType: CBType_Seq,
      payload: CBVarPayload {
        __bindgen_anon_1: CBVarPayload__bindgen_ty_1 {
          seqValue: CBSeq {
            elements: vec.as_ptr() as *mut CBVar,
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
    CBVar {
      valueType: CBType_Bytes,
      payload: CBVarPayload {
        __bindgen_anon_1: CBVarPayload__bindgen_ty_1 {
          __bindgen_anon_4: CBVarPayload__bindgen_ty_1__bindgen_ty_4 {
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
    v.valueType = CBType_ContextVar;
    v
  }

  pub fn new_object<T>(obj: &Rc<T>, info: &Type) -> Var {
    unsafe {
      Var {
        valueType: CBType_Object,
        payload: CBVarPayload {
          __bindgen_anon_1: CBVarPayload__bindgen_ty_1 {
            __bindgen_anon_1: CBVarPayload__bindgen_ty_1__bindgen_ty_1 {
              objectValue: obj as *const Rc<T> as *mut Rc<T> as CBPointer,
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
      valueType: CBType_Object,
      payload: CBVarPayload {
        __bindgen_anon_1: CBVarPayload__bindgen_ty_1 {
          __bindgen_anon_1: CBVarPayload__bindgen_ty_1__bindgen_ty_1 {
            objectValue: obj as *const Rc<T> as *mut Rc<T> as CBPointer,
            objectVendorId: info.details.object.vendorId,
            objectTypeId: info.details.object.typeId,
          },
        },
      },
      ..Default::default()
    }
  }

  pub fn into_object<T>(var: Var, info: &Type) -> Result<Rc<T>, &str> {
    unsafe {
      if var.valueType != CBType_Object
        || var.payload.__bindgen_anon_1.__bindgen_anon_1.objectVendorId
          != info.details.object.vendorId
        || var.payload.__bindgen_anon_1.__bindgen_anon_1.objectTypeId != info.details.object.typeId
      {
        Err("Failed to cast Var into custom Rc<T> object")
      } else {
        let aptr = var.payload.__bindgen_anon_1.__bindgen_anon_1.objectValue as *mut Rc<T>;
        let at = (*aptr).clone();
        Ok(at)
      }
    }
  }

  pub fn into_object_mut_ref<T>(var: Var, info: &Type) -> Result<&mut T, &str> {
    unsafe {
      if var.valueType != CBType_Object
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

  pub fn unsafe_mut<'a, T>(obj: &Rc<T>) -> &mut T {
    // this looks unsafe but CB gives some garantees within chains!
    let p = Rc::as_ptr(obj);
    let mp = p as *mut T;
    unsafe { &mut *mp }
  }

  pub fn push<T: Into<Var>>(&mut self, _val: T) {
    unimplemented!();
  }

  pub fn try_push<T: TryInto<Var>>(&mut self, _val: T) {
    unimplemented!();
  }

  pub fn is_seq(&self) -> bool {
    self.valueType == CBType_Seq
  }

  pub fn is_none(&self) -> bool {
    self.valueType == CBType_None
  }

  pub fn is_path(&self) -> bool {
    self.valueType == CBType_Path
  }

  pub fn as_ref(&self) -> &Self {
    &self
  }
}

impl TryFrom<&Var> for CBString {
  type Error = &'static str;

  #[inline(always)]
  fn try_from(var: &Var) -> Result<Self, Self::Error> {
    if var.valueType != CBType_String
      && var.valueType != CBType_Path
      && var.valueType != CBType_ContextVar
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
    if var.valueType != CBType_String
      && var.valueType != CBType_Path
      && var.valueType != CBType_ContextVar
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
    if var.valueType != CBType_String
      && var.valueType != CBType_Path
      && var.valueType != CBType_ContextVar
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
    if var.valueType != CBType_String
      && var.valueType != CBType_Path
      && var.valueType != CBType_ContextVar
    {
      Err("Expected String, Path or ContextVar variable, but casting failed.")
    } else {
      unsafe {
        Ok(CStr::from_ptr(
          var.payload.__bindgen_anon_1.__bindgen_anon_2.stringValue as *mut i8,
        ))
      }
    }
  }
}

impl TryFrom<&Var> for Option<CString> {
  type Error = &'static str;

  #[inline(always)]
  fn try_from(var: &Var) -> Result<Self, Self::Error> {
    if var.valueType != CBType_String
      && var.valueType != CBType_Path
      && var.valueType != CBType_ContextVar
      && var.valueType != CBType_None
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
    if var.valueType != CBType_Bytes {
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
    if var.valueType != CBType_String {
      Err("Expected String, but casting failed.")
    } else {
      unsafe {
        let cstr =
          CStr::from_ptr(var.payload.__bindgen_anon_1.__bindgen_anon_2.stringValue as *mut i8);
        cstr.to_str().map_err(|_| "UTF8 string conversion failed!")
      }
    }
  }
}

impl TryFrom<&Var> for i64 {
  type Error = &'static str;

  #[inline(always)]
  fn try_from(var: &Var) -> Result<Self, Self::Error> {
    if var.valueType != CBType_Int {
      Err("Expected Int variable, but casting failed.")
    } else {
      unsafe { Ok(var.payload.__bindgen_anon_1.intValue) }
    }
  }
}

impl TryFrom<&Var> for u64 {
  type Error = &'static str;

  #[inline(always)]
  fn try_from(var: &Var) -> Result<Self, Self::Error> {
    if var.valueType != CBType_Int {
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

impl TryFrom<&Var> for usize {
  type Error = &'static str;

  #[inline(always)]
  fn try_from(var: &Var) -> Result<Self, Self::Error> {
    if var.valueType != CBType_Int {
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
    if var.valueType != CBType_Float {
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
    if var.valueType != CBType_Float {
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
    if var.valueType != CBType_Float3 {
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
    if var.valueType != CBType_Float4 {
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
    if var.valueType != CBType_Bool {
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
    if var.valueType != CBType_Seq {
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
    if var.valueType != CBType_Seq {
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

impl AsRef<Var> for Var {
  #[inline(always)]
  fn as_ref(&self) -> &Var {
    &self
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
      if self.parameter.0.valueType == CBType_ContextVar {
        (*Core).releaseVariable.unwrap()(self.pointee);
      }
      self.pointee = std::ptr::null_mut();
    }
  }

  pub fn warmup(&mut self, context: &Context) {
    if self.parameter.0.valueType == CBType_ContextVar {
      assert_eq!(self.pointee, std::ptr::null_mut());
      unsafe {
        let ctx = context as *const CBContext as *mut CBContext;
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
      (*self.pointee).flags = value.flags | (CBVAR_FLAGS_REF_COUNTED as u8);
      (*self.pointee).refcount = rc;
    }
  }

  pub fn get(&mut self) -> Var {
    // avoid reading refcount
    assert_ne!(self.pointee, std::ptr::null_mut());
    unsafe { *self.pointee }
  }

  pub fn setParam(&mut self, value: &Var) {
    self.parameter = value.into();
  }

  pub fn getParam(&mut self) -> Var {
    self.parameter.0
  }

  pub fn isVariable(&mut self) -> bool {
    self.parameter.0.valueType == CBType_ContextVar
  }

  pub fn setName(&mut self, name: &str) {
    self.parameter = name.into();
    self.parameter.0.valueType = CBType_ContextVar;
  }

  pub fn getName(&mut self) -> *const i8 {
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

// Seq / CBSeq

#[derive(Clone)]
pub struct Seq {
  s: CBSeq,
  owned: bool,
}

impl Drop for Seq {
  fn drop(&mut self) {
    if self.owned {
      unsafe {
        (*Core).seqFree.unwrap()(&self.s as *const CBSeq as *mut CBSeq);
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

impl Index<usize> for CBSeq {
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

impl IndexMut<usize> for CBSeq {
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
      s: CBSeq {
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
        &self.s as *const CBSeq as *mut CBSeq,
        len.try_into().unwrap(),
      );
    }
  }

  pub fn push(&mut self, value: Var) {
    // we need to clone to own the memory chainblocks side
    let mut tmp = CBVar::default();
    cloneVar(&mut tmp, &value);
    unsafe {
      (*Core).seqPush.unwrap()(&self.s as *const CBSeq as *mut CBSeq, &tmp);
    }
  }

  pub fn insert(&mut self, index: usize, value: Var) {
    // we need to clone to own the memory chainblocks side
    let mut tmp = CBVar::default();
    cloneVar(&mut tmp, &value);
    unsafe {
      (*Core).seqInsert.unwrap()(
        &self.s as *const CBSeq as *mut CBSeq,
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
        let v = (*Core).seqPop.unwrap()(&self.s as *const CBSeq as *mut CBSeq);
        Some(transmute(v))
      } else {
        None
      }
    }
  }

  pub fn clear(&mut self) {
    unsafe {
      (*Core).seqResize.unwrap()(&self.s as *const CBSeq as *mut CBSeq, 0);
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
    CBVar {
      valueType: CBType_Seq,
      payload: CBVarPayload {
        __bindgen_anon_1: CBVarPayload__bindgen_ty_1 { seqValue: s.s },
      },
      ..Default::default()
    }
  }
}

impl TryFrom<&Var> for Seq {
  type Error = &'static str;

  #[inline(always)]
  fn try_from(v: &Var) -> Result<Self, Self::Error> {
    if v.valueType != CBType_Seq {
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
    if v.valueType != CBType_Seq {
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

// Table / CBTable

#[derive(Clone)]
pub struct Table {
  t: CBTable,
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
  value: *mut CBVar,
  userData: *mut ::std::os::raw::c_void,
) -> CBBool {
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
      let cstr = k.as_bytes_with_nul().as_ptr() as *const i8;
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
      let cstr = k.as_bytes_with_nul().as_ptr() as *const i8;
      let p = (*self.t.api).tableAt.unwrap()(self.t, cstr);
      cloneVar(&mut *p, &v);
    }
  }

  pub fn insert_fast_static(&mut self, k: &'static str, v: Var) {
    unsafe {
      let cstr = k.as_ptr() as *const i8;
      let p = (*self.t.api).tableAt.unwrap()(self.t, cstr);
      cloneVar(&mut *p, &v);
    }
  }

  pub fn get_mut(&self, k: &CString) -> Option<&mut Var> {
    unsafe {
      let cstr = k.as_bytes_with_nul().as_ptr() as *const i8;
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
      let cstr = k.as_bytes_with_nul().as_ptr() as *const i8;
      &mut *(*self.t.api).tableAt.unwrap()(self.t, cstr)
    }
  }

  pub fn get(&self, k: &CString) -> Option<&Var> {
    unsafe {
      let cstr = k.as_bytes_with_nul().as_ptr() as *const i8;
      if (*self.t.api).tableContains.unwrap()(self.t, cstr) {
        let p = (*self.t.api).tableAt.unwrap()(self.t, cstr);
        Some(&*p)
      } else {
        None
      }
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
  table: CBTable,
  citer: CBTableIterator,
}

impl Iterator for TableIterator {
  type Item = (String, Var);
  fn next(&mut self) -> Option<Self::Item> {
    unsafe {
      let k: CBString = core::ptr::null();
      let v: CBVar = CBVar::default();
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

impl From<CBTable> for Table {
  fn from(t: CBTable) -> Self {
    Table { t, owned: false }
  }
}

impl TryFrom<&Var> for Table {
  type Error = &'static str;

  #[inline(always)]
  fn try_from(var: &Var) -> Result<Self, Self::Error> {
    if var.valueType != CBType_Table {
      Err("Expected Table, but casting failed.")
    } else {
      unsafe { Ok(var.payload.__bindgen_anon_1.tableValue.into()) }
    }
  }
}

impl From<&Table> for Var {
  fn from(t: &Table) -> Self {
    CBVar {
      valueType: CBType_Table,
      payload: CBVarPayload {
        __bindgen_anon_1: CBVarPayload__bindgen_ty_1 { tableValue: t.t },
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
      unsafe {
        match self.valueType {
          CBType_Enum => {
            self.payload.__bindgen_anon_1.__bindgen_anon_3.enumVendorId
              == other.payload.__bindgen_anon_1.__bindgen_anon_3.enumVendorId
              && self.payload.__bindgen_anon_1.__bindgen_anon_3.enumTypeId
                == other.payload.__bindgen_anon_1.__bindgen_anon_3.enumTypeId
              && self.payload.__bindgen_anon_1.__bindgen_anon_3.enumValue
                == other.payload.__bindgen_anon_1.__bindgen_anon_3.enumValue
          }
          CBType_Bool => {
            self.payload.__bindgen_anon_1.boolValue == other.payload.__bindgen_anon_1.boolValue
          }
          CBType_Int => {
            self.payload.__bindgen_anon_1.intValue == other.payload.__bindgen_anon_1.intValue
          }
          CBType_Int2 => {
            self.payload.__bindgen_anon_1.int2Value == other.payload.__bindgen_anon_1.int2Value
          }
          CBType_Int3 => {
            self.payload.__bindgen_anon_1.int3Value == other.payload.__bindgen_anon_1.int3Value
          }
          CBType_Int4 => {
            self.payload.__bindgen_anon_1.int4Value == other.payload.__bindgen_anon_1.int4Value
          }
          CBType_Int8 => {
            self.payload.__bindgen_anon_1.int8Value == other.payload.__bindgen_anon_1.int8Value
          }
          CBType_Int16 => {
            self.payload.__bindgen_anon_1.int16Value == other.payload.__bindgen_anon_1.int16Value
          }
          CBType_Float => abs_diff_eq!(
            self.payload.__bindgen_anon_1.floatValue,
            other.payload.__bindgen_anon_1.floatValue
          ),
          CBType_Float2 => {
            abs_diff_eq!(
              self.payload.__bindgen_anon_1.float2Value[0],
              other.payload.__bindgen_anon_1.float2Value[0]
            ) && abs_diff_eq!(
              self.payload.__bindgen_anon_1.float2Value[1],
              other.payload.__bindgen_anon_1.float2Value[1]
            )
          }
          CBType_Float3 => {
            abs_diff_eq!(
              self.payload.__bindgen_anon_1.float3Value[0],
              other.payload.__bindgen_anon_1.float3Value[0]
            ) && abs_diff_eq!(
              self.payload.__bindgen_anon_1.float3Value[1],
              other.payload.__bindgen_anon_1.float3Value[1]
            ) && abs_diff_eq!(
              self.payload.__bindgen_anon_1.float3Value[2],
              other.payload.__bindgen_anon_1.float3Value[2]
            )
          }
          CBType_Float4 => {
            abs_diff_eq!(
              self.payload.__bindgen_anon_1.float4Value[0],
              other.payload.__bindgen_anon_1.float4Value[0]
            ) && abs_diff_eq!(
              self.payload.__bindgen_anon_1.float4Value[1],
              other.payload.__bindgen_anon_1.float4Value[1]
            ) && abs_diff_eq!(
              self.payload.__bindgen_anon_1.float4Value[2],
              other.payload.__bindgen_anon_1.float4Value[2]
            ) && abs_diff_eq!(
              self.payload.__bindgen_anon_1.float4Value[3],
              other.payload.__bindgen_anon_1.float4Value[3]
            )
          }
          CBType_Color => {
            self.payload.__bindgen_anon_1.colorValue.r
              == other.payload.__bindgen_anon_1.colorValue.r
              && self.payload.__bindgen_anon_1.colorValue.g
                == other.payload.__bindgen_anon_1.colorValue.g
              && self.payload.__bindgen_anon_1.colorValue.b
                == other.payload.__bindgen_anon_1.colorValue.b
              && self.payload.__bindgen_anon_1.colorValue.a
                == other.payload.__bindgen_anon_1.colorValue.a
          }
          CBType_Block => {
            self.payload.__bindgen_anon_1.blockValue == other.payload.__bindgen_anon_1.blockValue
          }
          CBType_Bytes => {
            self.payload.__bindgen_anon_1.__bindgen_anon_4.bytesSize
              == other.payload.__bindgen_anon_1.__bindgen_anon_4.bytesSize
              && (self.payload.__bindgen_anon_1.__bindgen_anon_4.bytesValue
                == other.payload.__bindgen_anon_1.__bindgen_anon_4.bytesValue
                || {
                  let aslice = slice::from_raw_parts(
                    self.payload.__bindgen_anon_1.__bindgen_anon_4.bytesValue as *const u8,
                    self.payload.__bindgen_anon_1.__bindgen_anon_4.bytesSize as usize,
                  );
                  let bslice = slice::from_raw_parts(
                    other.payload.__bindgen_anon_1.__bindgen_anon_4.bytesValue as *const u8,
                    other.payload.__bindgen_anon_1.__bindgen_anon_4.bytesSize as usize,
                  );

                  aslice == bslice
                })
          }
          CBType_String | CBType_Path | CBType_ContextVar => {
            self.payload.__bindgen_anon_1.__bindgen_anon_2.stringValue
              == other.payload.__bindgen_anon_1.__bindgen_anon_2.stringValue
              || {
                let astr = CStr::from_ptr(
                  self.payload.__bindgen_anon_1.__bindgen_anon_2.stringValue as *mut i8,
                );

                let bstr = CStr::from_ptr(
                  other.payload.__bindgen_anon_1.__bindgen_anon_2.stringValue as *mut i8,
                );

                astr == bstr
              }
          }
          CBType_Image => {
            let aflags: u32 = self.payload.__bindgen_anon_1.imageValue.flags.into();
            let bflags: u32 = other.payload.__bindgen_anon_1.imageValue.flags.into();
            let apixsize = if (aflags & CBIMAGE_FLAGS_16BITS_INT) == CBIMAGE_FLAGS_16BITS_INT {
              2
            } else if (aflags & CBIMAGE_FLAGS_32BITS_FLOAT) == CBIMAGE_FLAGS_32BITS_FLOAT {
              4
            } else {
              1
            };
            let bpixsize = if (bflags & CBIMAGE_FLAGS_16BITS_INT) == CBIMAGE_FLAGS_16BITS_INT {
              2
            } else if (bflags & CBIMAGE_FLAGS_32BITS_FLOAT) == CBIMAGE_FLAGS_32BITS_FLOAT {
              4
            } else {
              1
            };

            apixsize == bpixsize
              && self.payload.__bindgen_anon_1.imageValue.channels
                == other.payload.__bindgen_anon_1.imageValue.channels
              && self.payload.__bindgen_anon_1.imageValue.width
                == other.payload.__bindgen_anon_1.imageValue.width
              && self.payload.__bindgen_anon_1.imageValue.height
                == other.payload.__bindgen_anon_1.imageValue.height
              && (self.payload.__bindgen_anon_1.imageValue.data
                == other.payload.__bindgen_anon_1.imageValue.data
                || {
                  let aslice = slice::from_raw_parts(
                    self.payload.__bindgen_anon_1.imageValue.data,
                    self.payload.__bindgen_anon_1.imageValue.channels as usize
                      * self.payload.__bindgen_anon_1.imageValue.width as usize
                      * self.payload.__bindgen_anon_1.imageValue.height as usize
                      * apixsize,
                  );
                  let bslice = slice::from_raw_parts(
                    other.payload.__bindgen_anon_1.imageValue.data,
                    other.payload.__bindgen_anon_1.imageValue.channels as usize
                      * other.payload.__bindgen_anon_1.imageValue.width as usize
                      * other.payload.__bindgen_anon_1.imageValue.height as usize
                      * bpixsize,
                  );

                  aslice == bslice
                })
          }
          CBType_Seq => {
            if self.payload.__bindgen_anon_1.seqValue.elements
              == other.payload.__bindgen_anon_1.seqValue.elements
            {
              true
            } else if self.payload.__bindgen_anon_1.seqValue.len
              != other.payload.__bindgen_anon_1.seqValue.len
            {
              false
            } else {
              let aseq: Seq = self.try_into().unwrap();
              let bseq: Seq = other.try_into().unwrap();
              aseq.into_iter().eq(bseq.into_iter())
            }
          }
          CBType_Table => {
            let atab: Table = self.try_into().unwrap();
            let btab: Table = other.try_into().unwrap();
            atab.iter().eq(btab.iter())
          }
          CBType_Chain => {
            self.payload.__bindgen_anon_1.chainValue == other.payload.__bindgen_anon_1.chainValue
          }
          CBType_Object => {
            self.payload.__bindgen_anon_1.__bindgen_anon_1.objectValue
              == other.payload.__bindgen_anon_1.__bindgen_anon_1.objectValue
              && self
                .payload
                .__bindgen_anon_1
                .__bindgen_anon_1
                .objectVendorId
                == other
                  .payload
                  .__bindgen_anon_1
                  .__bindgen_anon_1
                  .objectVendorId
              && self.payload.__bindgen_anon_1.__bindgen_anon_1.objectTypeId
                == other.payload.__bindgen_anon_1.__bindgen_anon_1.objectTypeId
          }
          CBType_Array => {
            self.payload.__bindgen_anon_1.arrayValue.len
              == other.payload.__bindgen_anon_1.arrayValue.len
              && self.innerType == other.innerType
              && (self.payload.__bindgen_anon_1.arrayValue.elements
                == other.payload.__bindgen_anon_1.arrayValue.elements
                || {
                  let aslice = slice::from_raw_parts(
                    self.payload.__bindgen_anon_1.arrayValue.elements as *const u8,
                    self.payload.__bindgen_anon_1.arrayValue.len as usize
                      * std::mem::size_of::<CBVarPayload>(),
                  );
                  let bslice = slice::from_raw_parts(
                    other.payload.__bindgen_anon_1.arrayValue.elements as *const u8,
                    other.payload.__bindgen_anon_1.arrayValue.len as usize
                      * std::mem::size_of::<CBVarPayload>(),
                  );

                  aslice == bslice
                })
          }
          _ => true,
        }
      }
    }
  }
}

// TODO share those from C++ ones to reduce binary size
lazy_static! {
  pub static ref ANY_TYPES: Vec<Type> = vec![common_type::any];
  pub static ref NONE_TYPES: Vec<Type> = vec![common_type::none];
  pub static ref FLOAT3_TYPES: Vec<Type> = vec![common_type::float3];
  pub static ref FLOAT4_TYPES: Vec<Type> = vec![common_type::float4];
  pub static ref FLOAT4X4_TYPE: Type = {
    let mut t = common_type::float4s;
    t.fixedSize = 4;
    t
  };
  pub static ref FLOAT4X4_TYPES: Vec<Type> = vec![*FLOAT4X4_TYPE];
  pub static ref FLOAT4X4S_TYPE: Type = Type::seq(&[*FLOAT4X4_TYPE]);
  pub static ref FLOAT3X3_TYPE: Type = {
    let mut t = common_type::float3s;
    t.fixedSize = 3;
    t
  };
  pub static ref FLOAT3X3_TYPES: Vec<Type> = vec![*FLOAT3X3_TYPE];
  pub static ref FLOAT3X3S_TYPE: Type = Type::seq(&[*FLOAT3X3_TYPE]);
  pub static ref FLOAT4X2_TYPE: Type = {
    let mut t = common_type::float4s;
    t.fixedSize = 2;
    t
  };
  pub static ref FLOAT4X2_TYPES: Vec<Type> = vec![*FLOAT4X2_TYPE];
  pub static ref FLOAT4X2S_TYPE: Type = Type::seq(&[*FLOAT4X2_TYPE]);
}
