use crate::chainblocksc::CBChain;
use crate::chainblocksc::CBComposeResult;
use crate::chainblocksc::CBContext;
use crate::chainblocksc::CBExposedTypeInfo;
use crate::chainblocksc::CBExposedTypesInfo;
use crate::chainblocksc::CBInstanceData;
use crate::chainblocksc::CBParameterInfo;
use crate::chainblocksc::CBParametersInfo;
use crate::chainblocksc::CBPointer;
use crate::chainblocksc::CBSeq;
use crate::chainblocksc::CBString;
use crate::chainblocksc::CBStrings;
use crate::chainblocksc::CBTable;
use crate::chainblocksc::CBTypeInfo;
use crate::chainblocksc::CBTypeInfo_Details;
use crate::chainblocksc::CBTypeInfo_Details_Object;
use crate::chainblocksc::CBTypeInfo_Details_Table;
use crate::chainblocksc::CBType_Bool;
use crate::chainblocksc::CBType_Bytes;
use crate::chainblocksc::CBType_ContextVar;
use crate::chainblocksc::CBType_Float;
use crate::chainblocksc::CBType_Int;
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
use crate::chainblocksc::CBVAR_FLAGS_REF_COUNTED;
use crate::core::cloneVar;
use crate::core::Core;
use core::mem::transmute;
use core::ops::Index;
use std::convert::TryFrom;
use std::convert::TryInto;
use std::ffi::CStr;
use std::ffi::CString;
use std::rc::Rc;

pub type Context = CBContext;
pub type Var = CBVar;
pub type Type = CBTypeInfo;
pub type InstanceData = CBInstanceData;
pub type Chain = CBChain;
pub type ComposeResult = CBComposeResult;
pub type Block = CBlock;
pub type ExposedInfo = CBExposedTypeInfo;

unsafe impl Send for Var {}
unsafe impl Send for Context {}
unsafe impl Send for Block {}

/*
CBTypeInfo & co
*/
unsafe impl std::marker::Sync for CBTypeInfo {}

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
      help: chelp,
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
      help: help.as_ptr(),
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
      help: chelp,
      isMutable: false,
      isProtected: false,
      isTableEntry: false,
      global: false,
      scope: core::ptr::null_mut(),
    }
  }

  pub const fn new_static_with_help(
    name: &'static str,
    help: &'static str,
    ctype: CBTypeInfo,
  ) -> Self {
    let cname = name.as_ptr() as *const i8;
    let chelp = help.as_ptr() as *const i8;
    CBExposedTypeInfo {
      exposedType: ctype,
      name: cname,
      help: chelp,
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
pub struct ParameterInfo(pub CBParameterInfo);

impl ParameterInfo {
  fn new(name: &str, types: Types) -> Self {
    let cname = CString::new(name).unwrap();
    let chelp = core::ptr::null();
    let res = CBParameterInfo {
      name: cname.into_raw() as *mut i8,
      help: chelp,
      valueTypes: internal_from_types(types),
    };
    ParameterInfo(res)
  }

  fn new1(name: &str, help: &str, types: Types) -> Self {
    let cname = CString::new(name).unwrap();
    let chelp = CString::new(help).unwrap();
    let res = CBParameterInfo {
      name: cname.into_raw() as *mut i8,
      help: chelp.into_raw() as *mut i8,
      valueTypes: internal_from_types(types),
    };
    ParameterInfo(res)
  }
}

impl From<(&str, Types)> for ParameterInfo {
  fn from(v: (&str, Types)) -> ParameterInfo {
    ParameterInfo::new(v.0, v.1)
  }
}

impl From<(&str, &str, Types)> for ParameterInfo {
  fn from(v: (&str, &str, Types)) -> ParameterInfo {
    ParameterInfo::new1(v.0, v.1, v.2)
  }
}

impl Drop for ParameterInfo {
  fn drop(&mut self) {
    if self.0.name != core::ptr::null() {
      unsafe {
        let cname = CString::from_raw(self.0.name as *mut i8);
        drop(cname);
      }
    }
    if self.0.help != core::ptr::null() {
      unsafe {
        let chelp = CString::from_raw(self.0.help as *mut i8);
        drop(chelp);
      }
    }
    unsafe {
      internal_drop_types(self.0.valueTypes);
    }
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
  use crate::chainblocksc::CBTypeInfo;
  use crate::chainblocksc::CBTypeInfo_Details;
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
  use crate::chainblocksc::CBTypesInfo;

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
    }
  }

  const fn make_none() -> CBTypeInfo {
    let mut res = base_info();
    res.basicType = CBType_None;
    res
  }

  pub static none: CBTypeInfo = make_none();

  macro_rules! cbtype {
    ($fname:ident, $type:expr, $name:ident, $names:ident, $name_var:ident) => {
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
      };
    };
  }

  cbtype!(make_any, CBType_Any, any, anys, any_var);
  cbtype!(make_string, CBType_String, string, strings, string_var);
  cbtype!(make_bytes, CBType_Bytes, bytes, bytezs, bytes_var);
  cbtype!(make_int, CBType_Int, int, ints, int_var);
  cbtype!(make_float, CBType_Float, float, floats, float_var);
  cbtype!(make_bool, CBType_Bool, bool, bools, bool_var);
  cbtype!(make_block, CBType_Block, block, blocks, block_var);
  cbtype!(make_chain, CBType_Chain, chain, chains, chain_var);
  cbtype!(make_path, CBType_Path, path, paths, path_var);
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
    }
  }

  pub const fn object(vendorId: i32, typeId: i32) -> Type {
    Type {
      basicType: CBType_Object,
      details: CBTypeInfo_Details {
        object: CBTypeInfo_Details_Object {
          vendorId: vendorId,
          typeId: typeId,
        },
      },
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
      Core.cloneVar.unwrap()(rv, sv);
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
      Core.cloneVar.unwrap()(rv, sv);
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
      Core.cloneVar.unwrap()(rv, sv);
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
      Core.cloneVar.unwrap()(rv, sv);
    }
    res
  }
}

impl From<&[ClonedVar]> for ClonedVar {
  fn from(vec: &[ClonedVar]) -> Self {
    let res = ClonedVar(Var::default());
    unsafe {
      let src: &[Var] = std::mem::transmute(vec);
      let vsrc: Var = src.into();
      let rv = &res.0 as *const CBVar as *mut CBVar;
      let sv = &vsrc as *const CBVar;
      Core.cloneVar.unwrap()(rv, sv);
    }
    res
  }
}

impl Drop for ClonedVar {
  #[inline(always)]
  fn drop(&mut self) {
    unsafe {
      let rv = &self.0 as *const CBVar as *mut CBVar;
      Core.destroyVar.unwrap()(rv);
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
    if v.is_none() {
      Var::default()
    } else {
      Var::from(v.unwrap())
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

  pub fn into_object<'a, T>(var: Var, info: &'a Type) -> Result<Rc<T>, &'a str> {
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

  pub fn is_none(&self) -> bool {
    self.valueType == CBType_None
  }

  pub fn is_path(&self) -> bool {
    self.valueType == CBType_Path
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
    } else {
      if var.is_none() {
        Ok(None)
      } else {
        Ok(Some(var.try_into().unwrap_or(CString::new("").unwrap())))
      }
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
          .or_else(|_| Err("Int conversion failed, likely out of range (usize)"))
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
        Core.releaseVariable.unwrap()(self.pointee);
      }
      self.pointee = std::ptr::null_mut();
    }
  }

  pub fn warmup(&mut self, context: &Context) {
    if self.parameter.0.valueType == CBType_ContextVar {
      assert_eq!(self.pointee, std::ptr::null_mut());
      unsafe {
        let ctx = context as *const CBContext as *mut CBContext;
        self.pointee = Core.referenceVariable.unwrap()(
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
}

impl Drop for ParamVar {
  fn drop(&mut self) {
    self.cleanup();
  }
}

// Seq / CBSeq

pub struct Seq {
  s: CBSeq,
  owned: bool,
}

impl Drop for Seq {
  fn drop(&mut self) {
    if self.owned {
      unsafe {
        Core.seqFree.unwrap()(&self.s as *const CBSeq as *mut CBSeq);
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
    self.i = self.i + 1;
    if self.i < self.s.s.len {
      unsafe { Some(*self.s.s.elements.offset(self.i.try_into().unwrap())) }
    } else {
      None
    }
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
  fn index(&self, idx: usize) -> &Self::Output {
    &self.s[idx]
  }
  type Output = Var;
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

  pub fn push(&mut self, value: Var) {
    // we need to clone to own the memory chainblocks side
    let mut tmp = CBVar::default();
    cloneVar(&mut tmp, &value);
    unsafe {
      Core.seqPush.unwrap()(&self.s as *const CBSeq as *mut CBSeq, &tmp);
    }
  }

  pub fn insert(&mut self, index: usize, value: Var) {
    // we need to clone to own the memory chainblocks side
    let mut tmp = CBVar::default();
    cloneVar(&mut tmp, &value);
    unsafe {
      Core.seqInsert.unwrap()(
        &self.s as *const CBSeq as *mut CBSeq,
        index.try_into().unwrap(),
        &tmp,
      );
    }
  }

  pub fn len(&self) -> usize {
    self.s.len.try_into().unwrap()
  }

  pub fn pop(&mut self) -> Option<ClonedVar> {
    unsafe {
      if self.len() > 0 {
        let v = Core.seqPop.unwrap()(&self.s as *const CBSeq as *mut CBSeq);
        Some(transmute(v))
      } else {
        None
      }
    }
  }

  pub fn clear(&mut self) {
    unsafe {
      Core.seqResize.unwrap()(&self.s as *const CBSeq as *mut CBSeq, 0);
    }
  }
}

impl From<&Seq> for Var {
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

impl From<Var> for Seq {
  fn from(v: Var) -> Self {
    unsafe {
      Seq {
        s: v.payload.__bindgen_anon_1.seqValue,
        owned: false,
      }
    }
  }
}

// Table / CBTable

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

impl Table {
  pub fn new() -> Table {
    unsafe {
      Table {
        t: Core.tableNew.unwrap()(),
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

  pub fn get_mut_fast(&self, k: &CString) -> &mut Var {
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
}

impl From<CBTable> for Table {
  fn from(t: CBTable) -> Self {
    Table { t: t, owned: false }
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
