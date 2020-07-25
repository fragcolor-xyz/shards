use crate::chainblocksc::CBChain;
use crate::chainblocksc::CBComposeResult;
use crate::chainblocksc::CBContext;
use crate::chainblocksc::CBExposedTypeInfo;
use crate::chainblocksc::CBExposedTypesInfo;
use crate::chainblocksc::CBInstanceData;
use crate::chainblocksc::CBParameterInfo;
use crate::chainblocksc::CBParametersInfo;
use crate::chainblocksc::CBSeq;
use crate::chainblocksc::CBString;
use crate::chainblocksc::CBTable;
use crate::chainblocksc::CBTypeInfo;
use crate::chainblocksc::CBType_Bool;
use crate::chainblocksc::CBType_Bytes;
use crate::chainblocksc::CBType_ContextVar;
use crate::chainblocksc::CBType_Float;
use crate::chainblocksc::CBType_Int;
use crate::chainblocksc::CBType_None;
use crate::chainblocksc::CBType_Path;
use crate::chainblocksc::CBType_Seq;
use crate::chainblocksc::CBType_String;
use crate::chainblocksc::CBTypesInfo;
use crate::chainblocksc::CBVar;
use crate::chainblocksc::CBVarPayload;
use crate::chainblocksc::CBVarPayload__bindgen_ty_1;
use crate::chainblocksc::CBVarPayload__bindgen_ty_1__bindgen_ty_2;
use crate::chainblocksc::CBlock;
use crate::chainblocksc::CBlockPtr;
use crate::chainblocksc::CBlocks;
use crate::core::Core;
use std::convert::TryFrom;
use std::convert::TryInto;
use std::ffi::CStr;
use std::ffi::CString;

pub type Context = CBContext;
pub type Var = CBVar;
pub type Type = CBTypeInfo;
pub type String = CBString;
pub type InstanceData = CBInstanceData;
pub type Table = CBTable;
pub type Chain = CBChain;
pub type ComposeResult = CBComposeResult;
pub type Block = CBlock;

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
#[repr(transparent)]
pub struct ExposedInfo(CBExposedTypeInfo);

impl ExposedInfo {
    fn new(name: &str, ctype: CBTypeInfo) -> Self {
        let cname = CString::new(name).unwrap();
        let chelp = core::ptr::null();
        let res = CBExposedTypeInfo {
            exposedType: ctype,
            name: cname.into_raw(),
            help: chelp,
            isMutable: false,
            isTableEntry: false,
            global: false,
            scope: core::ptr::null_mut(),
        };
        ExposedInfo(res)
    }
}

impl Drop for ExposedInfo {
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
pub struct ParameterInfo(CBParameterInfo);

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
        ($fname:ident, $type:expr, $name:ident, $names:ident, $name_seq:ident) => {
            const fn $fname() -> CBTypeInfo {
                let mut res = base_info();
                res.basicType = $type;
                res
            }

            pub static $name: CBTypeInfo = $fname();

            pub static $name_seq: &'static [CBTypeInfo] = &[$name];

            pub static $names: CBTypeInfo = CBTypeInfo {
                basicType: CBType_Seq,
                details: CBTypeInfo_Details {
                    seqTypes: CBTypesInfo {
                        elements: $name_seq.as_ptr() as *mut CBTypeInfo,
                        len: 1,
                        cap: 0,
                    },
                },
            };
        };
    }

    cbtype!(make_any, CBType_Any, any, anys, any_seq);
    cbtype!(make_string, CBType_String, string, strings, string_seq);
    cbtype!(make_bytes, CBType_Bytes, bytes, bytezs, bytes_seq);
    cbtype!(make_int, CBType_Int, int, ints, int_seq);
    cbtype!(make_float, CBType_Float, float, floats, float_seq);
    cbtype!(make_bool, CBType_Bool, bool, bools, bool_seq);
    cbtype!(make_block, CBType_Block, block, blocks, block_seq);
    cbtype!(make_chain, CBType_Chain, chain, chains, chain_seq);
    cbtype!(make_path, CBType_Path, path, paths, path_seq);
}

/*
CBVar utility
 */

#[repr(transparent)] // force it same size of original
pub struct ClonedVar(pub Var);

#[repr(transparent)] // force it same size of original
pub struct WrappedVar(pub Var); // used in DSL macro, ignore it

impl From<Var> for ClonedVar {
    fn from(v: Var) -> Self {
        let res = ClonedVar(Var::default());
        unsafe {
            let rv = &res.0 as *const CBVar as *mut CBVar;
            let sv = &v as *const CBVar;
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

var_from!(bool, boolValue, CBType_Bool);
var_from!(i64, intValue, CBType_Int);
var_from!(f64, floatValue, CBType_Float);

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

impl From<&'static str> for Var {
    #[inline(always)]
    fn from(v: &'static str) -> Self {
        let res = CBVar {
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
        };
        res
    }
}

impl From<&CStr> for Var {
    #[inline(always)]
    fn from(v: &CStr) -> Self {
        let res = CBVar {
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
        };
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
        let res = CBVar {
            valueType: CBType_None,
            ..Default::default()
        };
        res
    }
}

impl From<&Vec<Var>> for Var {
    #[inline(always)]
    fn from(vec: &Vec<Var>) -> Self {
        let res = CBVar {
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
        };
        res
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
                let cstr =
                    CStr::from_ptr(var.payload.__bindgen_anon_1.__bindgen_anon_2.stringValue);
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
                let cstr =
                    CStr::from_ptr(var.payload.__bindgen_anon_1.__bindgen_anon_2.stringValue);
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
