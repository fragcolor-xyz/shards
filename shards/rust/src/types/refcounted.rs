/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2020 Fragcolor Pte. Ltd. */

//! Reference counted object types and trait implementations.

use super::*;
use crate::core::{cloneVar, destroyVar, Core};
use crate::shardsc::*;
use crate::{shlog_error, shlog_warn};
use std::ffi::{CStr, CString};
use std::ops::{Deref, DerefMut};
use std::pin::Pin;
use std::ptr::NonNull;
use std::rc::Rc;
use std::slice;
use std::sync::atomic::{AtomicU32, Ordering};

pub trait RCObjectVar {
  fn get_info() -> *mut SHObjectInfo;
}

pub struct RefCounted<T> {
  rc: AtomicU32,
  weak_rc: AtomicU32,
  value: Option<T>,
}

impl<T> RefCounted<T> {
  pub fn new(value: T) -> Self {
    Self {
      rc: AtomicU32::new(1),
      weak_rc: AtomicU32::new(1), // 1 weak reference to keep the object alive until last weak ref is gone
      value: Some(value),
    }
  }

  pub fn inc_ref(&self) {
    let prev_count = self.rc.fetch_add(1, Ordering::SeqCst);
    assert!(prev_count < u32::MAX, "Reference count overflowed");
  }

  pub fn dec_ref(&mut self) -> u32 {
    let prev_count = self.rc.fetch_sub(1, Ordering::SeqCst);
    assert!(prev_count > 0, "Reference count underflowed");

    if prev_count == 1 {
      self.value = None; // Drop the value when the last strong reference is released
      self.dec_weak_ref(); // Decrease the weak reference count when the strong count goes to 0
    }

    prev_count - 1
  }

  pub fn inc_weak_ref(&self) {
    let prev_count = self.weak_rc.fetch_add(1, Ordering::SeqCst);
    assert!(prev_count < u32::MAX, "Weak reference count overflowed");
  }

  pub fn dec_weak_ref(&self) -> u32 {
    let prev_count = self.weak_rc.fetch_sub(1, Ordering::SeqCst);
    assert!(prev_count > 0, "Weak reference count underflowed");

    if prev_count == 1 && self.rc.load(Ordering::SeqCst) == 0 {
      // Free the object when the last weak reference is released and no strong references exist
      unsafe {
        drop(Box::from_raw(self as *const _ as *mut Self));
      }
    }

    prev_count - 1
  }

  pub fn upgrade_weak(&self) -> Option<NonNull<Self>> {
    loop {
      let strong_count = self.rc.load(Ordering::SeqCst);
      if strong_count == 0 {
        return None;
      }
      if self
        .rc
        .compare_exchange(
          strong_count,
          strong_count + 1,
          Ordering::SeqCst,
          Ordering::SeqCst,
        )
        .is_ok()
      {
        self.dec_weak_ref();
        return Some(NonNull::from(self));
      }
    }
  }
}

#[macro_export]
macro_rules! ref_counted_object_type_impl {
  ($type:ident) => {
    lazy_static! {
      static ref TYPE_OBJECT_NAME: &'static str = concat!(stringify!($type), "\0");
      static ref TYPE_OBJECT_INFO: shards::SHObjectInfo = {
        unsafe extern "C" fn reference(arg1: *mut std::os::raw::c_void) {
          let rc = arg1 as *mut shards::types::RefCounted<$type>;
          (*rc).inc_ref();
        }

        unsafe extern "C" fn release(arg1: *mut std::os::raw::c_void) {
          let rc = arg1 as *mut shards::types::RefCounted<$type>;
          (*rc).dec_ref();
        }

        unsafe extern "C" fn weak_reference(arg1: *mut std::os::raw::c_void) {
          let rc = arg1 as *mut shards::types::RefCounted<$type>;
          (*rc).inc_weak_ref();
        }

        unsafe extern "C" fn weak_release(arg1: *mut std::os::raw::c_void) {
          let rc = arg1 as *mut shards::types::RefCounted<$type>;
          (*rc).dec_weak_ref();
        }

        unsafe extern "C" fn upgrade_weak(
          arg1: *mut std::os::raw::c_void,
        ) -> *mut std::os::raw::c_void {
          let rc = arg1 as *mut shards::types::RefCounted<$type>;
          (*rc)
            .upgrade_weak()
            .map_or(std::ptr::null_mut(), |ptr| ptr.as_ptr() as *mut _)
        }

        shards::SHObjectInfo {
          name: TYPE_OBJECT_NAME.as_ptr() as *const ::core::ffi::c_char,
          serialize: None,
          free: None,
          deserialize: None,
          reference: Some(reference),
          release: Some(release),
          weakReference: Some(weak_reference),
          weakRelease: Some(weak_release),
          upgradeWeak: Some(upgrade_weak),
          hash: None,
          isThreadSafe: false,
        }
      };
    }

    impl shards::types::RCObjectVar for $type {
      fn get_info() -> *mut shards::SHObjectInfo {
        &*TYPE_OBJECT_INFO as *const _ as *mut _
      }
    }
  };
}

impl Var {
  pub fn new_none() -> Self {
    Var {
      valueType: SHType_None,
      ..Default::default()
    }
  }

  pub fn new_any() -> Self {
    Var {
      valueType: SHType_Any,
      ..Default::default()
    }
  }

  pub fn new_bool(b: bool) -> Self {
    Var {
      valueType: SHType_Bool,
      payload: SHVarPayload {
        __bindgen_anon_1: SHVarPayload__bindgen_ty_1 { boolValue: b },
      },
      ..Default::default()
    }
  }

  pub fn new_float(f: f64) -> Self {
    Var {
      valueType: SHType_Float,
      payload: SHVarPayload {
        __bindgen_anon_1: SHVarPayload__bindgen_ty_1 { floatValue: f },
      },
      ..Default::default()
    }
  }

  pub fn new_int(i: i64) -> Self {
    Var {
      valueType: SHType_Int,
      payload: SHVarPayload {
        __bindgen_anon_1: SHVarPayload__bindgen_ty_1 { intValue: i },
      },
      ..Default::default()
    }
  }

  pub fn new_int2(i0: i64, i1: i64) -> Self {
    Var {
      valueType: SHType_Int2,
      payload: SHVarPayload {
        __bindgen_anon_1: SHVarPayload__bindgen_ty_1 {
          int2Value: [i0, i1],
        },
      },
      ..Default::default()
    }
  }

  pub fn new_int3(i0: i32, i1: i32, i2: i32) -> Self {
    Var {
      valueType: SHType_Int3,
      payload: SHVarPayload {
        __bindgen_anon_1: SHVarPayload__bindgen_ty_1 {
          int3Value: [i0, i1, i2, 0],
        },
      },
      ..Default::default()
    }
  }

  pub fn new_int4(i0: i32, i1: i32, i2: i32, i3: i32) -> Self {
    Var {
      valueType: SHType_Int4,
      payload: SHVarPayload {
        __bindgen_anon_1: SHVarPayload__bindgen_ty_1 {
          int4Value: [i0, i1, i2, i3],
        },
      },
      ..Default::default()
    }
  }

  pub fn new_int8(values: [i16; 8]) -> Self {
    Var {
      valueType: SHType_Int8,
      payload: SHVarPayload {
        __bindgen_anon_1: SHVarPayload__bindgen_ty_1 { int8Value: values },
      },
      ..Default::default()
    }
  }

  pub fn new_int16(values: [i8; 16]) -> Self {
    Var {
      valueType: SHType_Int16,
      payload: SHVarPayload {
        __bindgen_anon_1: SHVarPayload__bindgen_ty_1 { int16Value: values },
      },
      ..Default::default()
    }
  }

  pub fn new_color(r: u8, g: u8, b: u8, a: u8) -> Self {
    Var {
      valueType: SHType_Color,
      payload: SHVarPayload {
        __bindgen_anon_1: SHVarPayload__bindgen_ty_1 {
          colorValue: SHColor { r, g, b, a },
        },
      },
      ..Default::default()
    }
  }

  pub fn new_float2(f0: f64, f1: f64) -> Self {
    Var {
      valueType: SHType_Float2,
      payload: SHVarPayload {
        __bindgen_anon_1: SHVarPayload__bindgen_ty_1 {
          float2Value: [f0, f1],
        },
      },
      ..Default::default()
    }
  }

  pub fn new_float3(f0: f32, f1: f32, f2: f32) -> Self {
    Var {
      valueType: SHType_Float3,
      payload: SHVarPayload {
        __bindgen_anon_1: SHVarPayload__bindgen_ty_1 {
          float3Value: [f0, f1, f2, 0.0],
        },
      },
      ..Default::default()
    }
  }

  pub fn new_float4(f0: f32, f1: f32, f2: f32, f3: f32) -> Self {
    Var {
      valueType: SHType_Float4,
      payload: SHVarPayload {
        __bindgen_anon_1: SHVarPayload__bindgen_ty_1 {
          float4Value: [f0, f1, f2, f3],
        },
      },
      ..Default::default()
    }
  }

  pub fn new_enum(value: i32, vendor_id: i32, type_id: i32) -> Self {
    Var {
      valueType: SHType_Enum,
      payload: SHVarPayload {
        __bindgen_anon_1: SHVarPayload__bindgen_ty_1 {
          __bindgen_anon_3: SHVarPayload__bindgen_ty_1__bindgen_ty_3 {
            enumValue: value,
            enumVendorId: vendor_id,
            enumTypeId: type_id,
          },
        },
      },
      ..Default::default()
    }
  }

  pub fn get_type(&self) -> crate::types::common::SHType {
    self.valueType.into()
  }

  pub fn color_u8s(r: u8, g: u8, b: u8, a: u8) -> Var {
    SHVar {
      valueType: SHType_Color,
      payload: SHVarPayload {
        __bindgen_anon_1: SHVarPayload__bindgen_ty_1 {
          colorValue: SHColor { r, g, b, a },
        },
      },
      ..Default::default()
    }
  }

  pub fn color_bytes(var: &Var) -> Result<(u8, u8, u8, u8), &'static str> {
    if var.valueType != SHType_Color {
      return Err("Invalid type");
    }
    unsafe {
      Ok((
        var.payload.__bindgen_anon_1.colorValue.r,
        var.payload.__bindgen_anon_1.colorValue.g,
        var.payload.__bindgen_anon_1.colorValue.b,
        var.payload.__bindgen_anon_1.colorValue.a,
      ))
    }
  }

  pub fn color_ints(r: i32, g: i32, b: i32, a: i32) -> Result<Var, &'static str> {
    // ensure all values are in range [0, 255]
    if r < 0 || r > 255 {
      return Err("r is out of range");
    }
    if g < 0 || g > 255 {
      return Err("g is out of range");
    }
    if b < 0 || b > 255 {
      return Err("b is out of range");
    }
    if a < 0 || a > 255 {
      return Err("a is out of range");
    }
    Ok(Var::color_u8s(r as u8, g as u8, b as u8, a as u8))
  }

  pub fn color_floats(r: f32, g: f32, b: f32, a: f32) -> Result<Var, &'static str> {
    // ensure all values are in range [0.0, 1.0]
    if r < 0.0 || r > 1.0 {
      return Err("r is out of range");
    }
    if g < 0.0 || g > 1.0 {
      return Err("g is out of range");
    }
    if b < 0.0 || b > 1.0 {
      return Err("b is out of range");
    }
    if a < 0.0 || a > 1.0 {
      return Err("a is out of range");
    }
    Ok(Var::color_u8s(
      (r * 255.0) as u8,
      (g * 255.0) as u8,
      (b * 255.0) as u8,
      (a * 255.0) as u8,
    ))
  }

  /// To be used while &str is in scope.
  /// Such string likely doesn't have NULL terminator!
  /// CloneVar is safe but the rest might not be!
  pub fn ephemeral_string(s: &str) -> Var {
    let len = s.len();
    let p = if len > 0 {
      s.as_ptr()
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

  pub fn ephemeral_slice(b: &[u8]) -> Var {
    SHVar {
      valueType: SHType_Bytes,
      payload: SHVarPayload {
        __bindgen_anon_1: SHVarPayload__bindgen_ty_1 {
          __bindgen_anon_4: SHVarPayload__bindgen_ty_1__bindgen_ty_4 {
            bytesValue: b.as_ptr() as *mut u8,
            bytesSize: b.len() as u32,
            bytesCapacity: 0,
          },
        },
      },
      ..Default::default()
    }
  }

  /// Caller must call cloneVar to increase strong reference count
  pub fn new_ref_counted<T: RCObjectVar>(obj: T, info: &Type) -> Var {
    let rc = Box::new(RefCounted::<T> {
      rc: AtomicU32::new(0),
      weak_rc: AtomicU32::new(1),
      value: Some(obj),
    });
    unsafe {
      Var {
        valueType: SHType_Object,
        flags: SHVAR_FLAGS_USES_OBJINFO as u16,
        __bindgen_anon_1: SHVar__bindgen_ty_1 {
          objectInfo: T::get_info(),
        },
        payload: SHVarPayload {
          __bindgen_anon_1: SHVarPayload__bindgen_ty_1 {
            __bindgen_anon_1: SHVarPayload__bindgen_ty_1__bindgen_ty_1 {
              objectValue: Box::into_raw(rc) as *mut _,
              objectVendorId: info.details.object.vendorId,
              objectTypeId: info.details.object.typeId,
            },
          },
        },
        ..Default::default()
      }
    }
  }

  pub unsafe fn weak_object_var(dst: &mut Var, src: &Var) {
    assert_eq!(src.valueType, SHType_Object);
    assert!(!src.__bindgen_anon_1.objectInfo.is_null());
    assert!(!(*src.__bindgen_anon_1.objectInfo).weakReference.is_none());

    destroyVar(dst);
    dst.valueType = SHType_Object;
    dst.payload.__bindgen_anon_1.__bindgen_anon_1.objectValue =
      src.payload.__bindgen_anon_1.__bindgen_anon_1.objectValue;
    dst.payload.__bindgen_anon_1.__bindgen_anon_1.objectVendorId =
      src.payload.__bindgen_anon_1.__bindgen_anon_1.objectVendorId;
    dst.payload.__bindgen_anon_1.__bindgen_anon_1.objectTypeId =
      src.payload.__bindgen_anon_1.__bindgen_anon_1.objectTypeId;
    dst.flags |= SHVAR_FLAGS_USES_OBJINFO as u16 | SHVAR_FLAGS_WEAK_OBJECT as u16;
    dst.__bindgen_anon_1.objectInfo = src.__bindgen_anon_1.objectInfo;
    unsafe {
      (*dst.__bindgen_anon_1.objectInfo)
        .weakReference
        .unwrap_unchecked()(dst.payload.__bindgen_anon_1.__bindgen_anon_1.objectValue);
    }
  }

  pub unsafe fn from_ref_counted_object<T>(var: &Var, info: &Type) -> Result<*mut T, &'static str> {
    if var.valueType != SHType_Object
      || var.payload.__bindgen_anon_1.__bindgen_anon_1.objectVendorId
        != info.details.object.vendorId
      || var.payload.__bindgen_anon_1.__bindgen_anon_1.objectTypeId != info.details.object.typeId
    {
      Err("Failed to cast Var into custom ref counted object")
    } else {
      let aptr = var.payload.__bindgen_anon_1.__bindgen_anon_1.objectValue as *mut RefCounted<T>;
      if let Some(value) = &mut (*aptr).value {
        Ok(value as *mut T)
      } else {
        Err("Failed to cast Var into custom ref counted object: value is None")
      }
    }
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

  pub fn from_object_as_clone<T>(var: &Var, info: &Type) -> Result<Rc<T>, &'static str> {
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
  pub fn get_mut_from_clone<'a, T>(c: &Option<Rc<Option<T>>>) -> Result<&'a mut T, &'static str> {
    let c = c.as_ref().ok_or("No Var reference found")?;
    let c = Rc::as_ptr(c) as *mut Option<T>;
    let c = unsafe { (*c).as_mut().ok_or("Failed to unwrap Rc-ed reference")? };
    Ok(c)
  }

  // This pattern is often used in shards storing Rcs of Vars
  pub fn get_mut_from_clone1<'a, T>(c: &Option<Rc<T>>) -> Result<&'a mut T, &'static str> {
    let c = c.as_ref().ok_or("No Var reference found")?;
    let c = Rc::as_ptr(c) as *mut T;
    Ok(unsafe { &mut *c })
  }

  pub fn from_object_mut_ref<'a, T>(var: &Var, info: &Type) -> Result<&'a mut T, &'static str> {
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

  pub fn from_object_ptr_mut_ref<'a, T>(var: &Var, info: &Type) -> Result<&'a mut T, &'static str> {
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

  pub fn from_object_ptr_ref<'a, T>(var: &Var, info: &Type) -> Result<&'a T, &'static str> {
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

  pub fn is_table(&self) -> bool {
    self.valueType == SHType_Table
  }

  pub fn is_none(&self) -> bool {
    self.valueType == SHType_None
  }

  pub fn is_bool(&self) -> bool {
    self.valueType == SHType_Bool
  }

  pub fn is_int(&self) -> bool {
    self.valueType == SHType_Int
  }

  pub fn is_float(&self) -> bool {
    self.valueType == SHType_Float
  }

  pub fn is_string(&self) -> bool {
    self.valueType == SHType_String
  }

  pub fn is_bytes(&self) -> bool {
    self.valueType == SHType_Bytes
  }

  pub fn is_path(&self) -> bool {
    self.valueType == SHType_Path
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

  pub fn as_seq(&self) -> Result<&SeqVar, &'static str> {
    if self.valueType != SHType_Seq {
      Err("Variable is not a sequence")
    } else {
      Ok(unsafe { &*(self as *const Var as *const SeqVar) })
    }
  }

  /// The returned SeqVar needs to be wrapped in ClonedVar or destroyed with destroyVar or ownership should be delegated to another Var
  /// SeqVar WON'T call DROP
  pub fn as_mut_seq_creating(&mut self) -> Result<&mut SeqVar, &'static str> {
    if self.valueType != SHType_Seq {
      if self.valueType == SHType_None {
        let sv = SeqVar::new();
        *self = sv.0;
        Ok(unsafe { &mut *(self as *mut Var as *mut SeqVar) })
      } else {
        Err("Variable is not a sequence")
      }
    } else {
      Ok(unsafe { &mut *(self as *mut Var as *mut SeqVar) })
    }
  }

  pub fn as_mut_seq(&mut self) -> Result<&mut SeqVar, &'static str> {
    if self.valueType != SHType_Seq {
      Err("Variable is not a sequence")
    } else {
      Ok(unsafe { &mut *(self as *mut Var as *mut SeqVar) })
    }
  }

  pub fn as_table(&self) -> Result<&TableVar, &'static str> {
    if self.valueType != SHType_Table {
      Err("Variable is not a table")
    } else {
      Ok(unsafe { &*(self as *const Var as *const TableVar) })
    }
  }

  /// The returned TableVar needs to be wrapped in ClonedVar or destroyed with destroyVar or ownership should be delegated to another Var
  /// TableVar WON'T call DROP
  pub fn as_mut_table_creating(&mut self) -> Result<&mut TableVar, &'static str> {
    if self.valueType != SHType_Table {
      if self.valueType == SHType_None {
        let sv = TableVar::new();
        *self = sv.0;
        Ok(unsafe { &mut *(self as *mut Var as *mut TableVar) })
      } else {
        Err("Variable is not a table")
      }
    } else {
      Ok(unsafe { &mut *(self as *mut Var as *mut TableVar) })
    }
  }

  pub fn as_mut_table(&mut self) -> Result<&mut TableVar, &'static str> {
    if self.valueType != SHType_Table {
      Err("Variable is not a table")
    } else {
      Ok(unsafe { &mut *(self as *mut Var as *mut TableVar) })
    }
  }

  pub fn as_str(&self) -> Result<&str, &'static str> {
    self.try_into()
  }

  pub fn serialize(&self) -> ClonedVar {
    ClonedVar(unsafe { (*Core).serializeVar.unwrap_unchecked()(self) })
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
        if var.payload.__bindgen_anon_1.__bindgen_anon_4.bytesSize == 0 {
          return Ok(&[]);
        }
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
      Err("Expected String, Path or ContextVar variable, but casting failed.")
    } else {
      unsafe {
        if var.payload.__bindgen_anon_1.__bindgen_anon_2.stringLen == 0 {
          return Ok("");
        }
      }
      std::str::from_utf8(unsafe {
        slice::from_raw_parts(
          var.payload.__bindgen_anon_1.__bindgen_anon_2.stringValue as *const u8,
          var.payload.__bindgen_anon_1.__bindgen_anon_2.stringLen as usize,
        )
      })
      .map_err(|e| {
        shlog_error!(
          "Expected valid UTF-8 string, but casting failed: {:?}, string len: {}",
          e,
          unsafe { var.payload.__bindgen_anon_1.__bindgen_anon_2.stringLen }
        );
        "Expected valid UTF-8 string, but casting failed."
      })
    }
  }
}

impl<'a> TryFrom<&'a Var> for &'a SHImage {
  type Error = &'static str;

  #[inline(always)]
  fn try_from(var: &'a Var) -> Result<Self, Self::Error> {
    if var.valueType != SHType_Image {
      Err("Expected Image variable, but casting failed.")
    } else {
      unsafe {
        Ok(
          &var
            .payload
            .__bindgen_anon_1
            .imageValue
            .as_ref()
            .unwrap_unchecked(),
        )
      }
    }
  }
}

// 64-bit precision :i64
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

impl<'a> TryFrom<&'a mut Var> for &'a mut i64 {
  type Error = &'static str;

  #[inline(always)]
  fn try_from(var: &'a mut Var) -> Result<Self, Self::Error> {
    if var.valueType != SHType_Int {
      Err("Expected Int variable, but casting failed.")
    } else {
      unsafe { Ok(&mut var.payload.__bindgen_anon_1.intValue) }
    }
  }
}

// 64-bit precision :u64
impl TryFrom<&Var> for u64 {
  type Error = &'static str;

  #[inline(always)]
  fn try_from(var: &Var) -> Result<Self, Self::Error> {
    if var.valueType != SHType_Int {
      Err("Expected Int variable, but casting failed.")
    } else {
      unsafe { Ok(var.payload.__bindgen_anon_1.intValue as u64) }
    }
  }
}

impl TryFrom<&Var> for u128 {
  type Error = &'static str;

  #[inline(always)]
  fn try_from(value: &Var) -> Result<Self, Self::Error> {
    if value.valueType != SHType_Int16 {
      return Err("Expected Int16 variable, but casting failed.");
    }
    // so this is simply our Int16 16 bytes to u128
    let value: [i8; 16] = unsafe { value.payload.__bindgen_anon_1.int16Value };
    // reinterpret as u128
    Ok(unsafe { std::mem::transmute(value) })
  }
}

// 64-bit precision :usize
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

// 64-bit precision :f64
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

impl<'a> TryFrom<&'a mut Var> for &'a mut f64 {
  type Error = &'static str;

  #[inline(always)]
  fn try_from(var: &'a mut Var) -> Result<Self, Self::Error> {
    if var.valueType != SHType_Float {
      Err("Expected Float variable, but casting failed.")
    } else {
      unsafe { Ok(&mut var.payload.__bindgen_anon_1.floatValue) }
    }
  }
}

// 64-bit precision :[i64;2]
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

impl<'a> TryFrom<&'a Var> for &'a [i64; 2] {
  type Error = &'static str;

  #[inline(always)]
  fn try_from(var: &'a Var) -> Result<Self, Self::Error> {
    if var.valueType != SHType_Int2 {
      Err("Expected Int2 variable, but casting failed.")
    } else {
      unsafe { Ok(&var.payload.__bindgen_anon_1.int2Value) }
    }
  }
}

impl<'a> TryFrom<&'a mut Var> for &'a mut [i64; 2] {
  type Error = &'static str;

  #[inline(always)]
  fn try_from(var: &'a mut Var) -> Result<Self, Self::Error> {
    if var.valueType != SHType_Int2 {
      Err("Expected Int2 variable, but casting failed.")
    } else {
      unsafe { Ok(&mut var.payload.__bindgen_anon_1.int2Value) }
    }
  }
}

// 64-bit precision :[u64;2]
impl TryFrom<&Var> for (u64, u64) {
  type Error = &'static str;

  #[inline(always)]
  fn try_from(var: &Var) -> Result<Self, Self::Error> {
    if var.valueType != SHType_Int2 {
      Err("Expected Int2 variable, but casting failed.")
    } else {
      unsafe {
        Ok((
          var.payload.__bindgen_anon_1.int2Value[0] as u64,
          var.payload.__bindgen_anon_1.int2Value[1] as u64,
        ))
      }
    }
  }
}

// 64-bit precision :[f64;2]
impl TryFrom<&Var> for (f64, f64) {
  type Error = &'static str;

  #[inline(always)]
  fn try_from(var: &Var) -> Result<Self, Self::Error> {
    if var.valueType != SHType_Float2 {
      Err("Expected Float2 variable, but casting failed.")
    } else {
      unsafe {
        Ok((
          var.payload.__bindgen_anon_1.float2Value[0],
          var.payload.__bindgen_anon_1.float2Value[1],
        ))
      }
    }
  }
}

impl<'a> TryFrom<&'a Var> for &'a [f64; 2] {
  type Error = &'static str;

  #[inline(always)]
  fn try_from(var: &'a Var) -> Result<Self, Self::Error> {
    if var.valueType != SHType_Float2 {
      Err("Expected Float2 variable, but casting failed.")
    } else {
      unsafe { Ok(&var.payload.__bindgen_anon_1.float2Value) }
    }
  }
}

impl<'a> TryFrom<&'a mut Var> for &'a mut [f64; 2] {
  type Error = &'static str;

  #[inline(always)]
  fn try_from(var: &'a mut Var) -> Result<Self, Self::Error> {
    if var.valueType != SHType_Float2 {
      Err("Expected Float2 variable, but casting failed.")
    } else {
      unsafe { Ok(&mut var.payload.__bindgen_anon_1.float2Value) }
    }
  }
}

// 32-bit precision :i32
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

// 32-bit precision :[i32;2]
impl TryFrom<&Var> for (i32, i32) {
  type Error = &'static str;

  #[inline(always)]
  fn try_from(var: &Var) -> Result<Self, Self::Error> {
    if var.valueType != SHType_Int2 {
      Err("Expected Int2 variable, but casting failed.")
    } else {
      unsafe {
        Ok((
          var.payload.__bindgen_anon_1.int2Value[0] as i32,
          var.payload.__bindgen_anon_1.int2Value[1] as i32,
        ))
      }
    }
  }
}

// 32-bit precision :[i32;3]
impl TryFrom<&Var> for (i32, i32, i32) {
  type Error = &'static str;

  #[inline(always)]
  fn try_from(var: &Var) -> Result<Self, Self::Error> {
    if var.valueType != SHType_Int3 {
      Err("Expected Int3 variable, but casting failed.")
    } else {
      unsafe {
        Ok((
          var.payload.__bindgen_anon_1.int3Value[0],
          var.payload.__bindgen_anon_1.int3Value[1],
          var.payload.__bindgen_anon_1.int3Value[2],
        ))
      }
    }
  }
}

impl<'a> TryFrom<&'a Var> for &'a [i32; 3] {
  type Error = &'static str;

  #[inline(always)]
  fn try_from(var: &'a Var) -> Result<Self, Self::Error> {
    if var.valueType != SHType_Int3 {
      Err("Expected Int3 variable, but casting failed.")
    } else {
      unsafe {
        Ok(core::mem::transmute(
          &var.payload.__bindgen_anon_1.int3Value,
        ))
      }
    }
  }
}

impl<'a> TryFrom<&'a mut Var> for &'a mut [i32; 3] {
  type Error = &'static str;

  #[inline(always)]
  fn try_from(var: &'a mut Var) -> Result<Self, Self::Error> {
    if var.valueType != SHType_Int3 {
      Err("Expected Int3 variable, but casting failed.")
    } else {
      unsafe {
        Ok(core::mem::transmute(
          &mut var.payload.__bindgen_anon_1.int3Value,
        ))
      }
    }
  }
}

// 32-bit precision :[i32;4]
impl TryFrom<&Var> for (i32, i32, i32, i32) {
  type Error = &'static str;

  #[inline(always)]
  fn try_from(var: &Var) -> Result<Self, Self::Error> {
    if var.valueType != SHType_Int4 {
      Err("Expected Int4 variable, but casting failed.")
    } else {
      unsafe {
        Ok((
          var.payload.__bindgen_anon_1.int4Value[0],
          var.payload.__bindgen_anon_1.int4Value[1],
          var.payload.__bindgen_anon_1.int4Value[2],
          var.payload.__bindgen_anon_1.int4Value[3],
        ))
      }
    }
  }
}

impl<'a> TryFrom<&'a Var> for &'a [i32; 4] {
  type Error = &'static str;

  #[inline(always)]
  fn try_from(var: &'a Var) -> Result<Self, Self::Error> {
    if var.valueType != SHType_Int4 {
      Err("Expected Int4 variable, but casting failed.")
    } else {
      unsafe { Ok(&var.payload.__bindgen_anon_1.int4Value) }
    }
  }
}

impl<'a> TryFrom<&'a mut Var> for &'a mut [i32; 4] {
  type Error = &'static str;

  #[inline(always)]
  fn try_from(var: &'a mut Var) -> Result<Self, Self::Error> {
    if var.valueType != SHType_Int4 {
      Err("Expected Int4 variable, but casting failed.")
    } else {
      unsafe { Ok(&mut var.payload.__bindgen_anon_1.int4Value) }
    }
  }
}

// 32-bit precision :u32
impl TryFrom<&Var> for u32 {
  type Error = &'static str;

  #[inline(always)]
  fn try_from(var: &Var) -> Result<Self, Self::Error> {
    if var.valueType != SHType_Int {
      Err("Expected Int variable, but casting failed.")
    } else {
      unsafe { Ok(var.payload.__bindgen_anon_1.intValue as u32) }
    }
  }
}

// 32-bit precision :[u32;2]
impl TryFrom<&Var> for (u32, u32) {
  type Error = &'static str;

  #[inline(always)]
  fn try_from(var: &Var) -> Result<Self, Self::Error> {
    if var.valueType != SHType_Int2 {
      Err("Expected Int2 variable, but casting failed.")
    } else {
      unsafe {
        Ok((
          u32::from_ne_bytes((var.payload.__bindgen_anon_1.int2Value[0] as i32).to_ne_bytes()),
          u32::from_ne_bytes((var.payload.__bindgen_anon_1.int2Value[1] as i32).to_ne_bytes()),
        ))
      }
    }
  }
}

// 32-bit precision :[u32;3]
impl TryFrom<&Var> for (u32, u32, u32) {
  type Error = &'static str;

  #[inline(always)]
  fn try_from(var: &Var) -> Result<Self, Self::Error> {
    if var.valueType != SHType_Int3 {
      Err("Expected Int3 variable, but casting failed.")
    } else {
      unsafe {
        Ok((
          u32::from_ne_bytes((var.payload.__bindgen_anon_1.int3Value[0]).to_ne_bytes()),
          u32::from_ne_bytes((var.payload.__bindgen_anon_1.int3Value[1]).to_ne_bytes()),
          u32::from_ne_bytes((var.payload.__bindgen_anon_1.int3Value[2]).to_ne_bytes()),
        ))
      }
    }
  }
}

// 32-bit precision :[u32;4]
impl TryFrom<&Var> for (u32, u32, u32, u32) {
  type Error = &'static str;

  #[inline(always)]
  fn try_from(var: &Var) -> Result<Self, Self::Error> {
    if var.valueType != SHType_Int4 {
      Err("Expected Int4 variable, but casting failed.")
    } else {
      unsafe {
        Ok((
          u32::from_ne_bytes((var.payload.__bindgen_anon_1.int4Value[0]).to_ne_bytes()),
          u32::from_ne_bytes((var.payload.__bindgen_anon_1.int4Value[1]).to_ne_bytes()),
          u32::from_ne_bytes((var.payload.__bindgen_anon_1.int4Value[2]).to_ne_bytes()),
          u32::from_ne_bytes((var.payload.__bindgen_anon_1.int4Value[3]).to_ne_bytes()),
        ))
      }
    }
  }
}

// 32-bit precision :f32
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

// 32-bit precision :[f32;2]
impl TryFrom<&Var> for (f32, f32) {
  type Error = &'static str;

  #[inline(always)]
  fn try_from(var: &Var) -> Result<Self, Self::Error> {
    if var.valueType != SHType_Float2 {
      Err("Expected Float2 variable, but casting failed.")
    } else {
      unsafe {
        Ok((
          var.payload.__bindgen_anon_1.float2Value[0] as f32,
          var.payload.__bindgen_anon_1.float2Value[1] as f32,
        ))
      }
    }
  }
}

// 32-bit precision :[f32;3]
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

impl<'a> TryFrom<&'a Var> for &'a [f32; 3] {
  type Error = &'static str;

  #[inline(always)]
  fn try_from(var: &'a Var) -> Result<Self, Self::Error> {
    if var.valueType != SHType_Float3 {
      Err("Expected Float3 variable, but casting failed.")
    } else {
      unsafe {
        Ok(core::mem::transmute(
          &var.payload.__bindgen_anon_1.float3Value,
        ))
      }
    }
  }
}

impl<'a> TryFrom<&'a mut Var> for &'a mut [f32; 3] {
  type Error = &'static str;

  #[inline(always)]
  fn try_from(var: &'a mut Var) -> Result<Self, Self::Error> {
    if var.valueType != SHType_Float3 {
      Err("Expected Float3 variable, but casting failed.")
    } else {
      unsafe {
        Ok(core::mem::transmute(
          &mut var.payload.__bindgen_anon_1.float3Value,
        ))
      }
    }
  }
}

// 32-bit precision :[f32;4]
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

impl<'a> TryFrom<&'a Var> for &'a [f32; 4] {
  type Error = &'static str;

  #[inline(always)]
  fn try_from(var: &'a Var) -> Result<Self, Self::Error> {
    if var.valueType != SHType_Float4 {
      Err("Expected Float4 variable, but casting failed.")
    } else {
      unsafe { Ok(&var.payload.__bindgen_anon_1.float4Value) }
    }
  }
}

impl<'a> TryFrom<&'a mut Var> for &'a mut [f32; 4] {
  type Error = &'static str;

  #[inline(always)]
  fn try_from(var: &'a mut Var) -> Result<Self, Self::Error> {
    if var.valueType != SHType_Float4 {
      Err("Expected Float4 variable, but casting failed.")
    } else {
      unsafe { Ok(&mut var.payload.__bindgen_anon_1.float4Value) }
    }
  }
}

// 16-bit precision :i16
impl TryFrom<&Var> for i16 {
  type Error = &'static str;

  #[inline(always)]
  fn try_from(var: &Var) -> Result<Self, Self::Error> {
    if var.valueType != SHType_Int {
      Err("Expected Int variable, but casting failed.")
    } else {
      unsafe { Ok(var.payload.__bindgen_anon_1.intValue as i16) }
    }
  }
}

// 16-bit precision :[i16;2]
impl TryFrom<&Var> for (i16, i16) {
  type Error = &'static str;

  #[inline(always)]
  fn try_from(var: &Var) -> Result<Self, Self::Error> {
    if var.valueType != SHType_Int2 {
      Err("Expected Int2 variable, but casting failed.")
    } else {
      unsafe {
        Ok((
          var.payload.__bindgen_anon_1.int2Value[0] as i16,
          var.payload.__bindgen_anon_1.int2Value[1] as i16,
        ))
      }
    }
  }
}

// 16-bit precision :[i16;3]
impl TryFrom<&Var> for (i16, i16, i16) {
  type Error = &'static str;

  #[inline(always)]
  fn try_from(var: &Var) -> Result<Self, Self::Error> {
    if var.valueType != SHType_Int3 {
      Err("Expected Int3 variable, but casting failed.")
    } else {
      unsafe {
        Ok((
          var.payload.__bindgen_anon_1.int3Value[0] as i16,
          var.payload.__bindgen_anon_1.int3Value[1] as i16,
          var.payload.__bindgen_anon_1.int3Value[2] as i16,
        ))
      }
    }
  }
}

// 16-bit precision :[i16;4]
impl TryFrom<&Var> for (i16, i16, i16, i16) {
  type Error = &'static str;

  #[inline(always)]
  fn try_from(var: &Var) -> Result<Self, Self::Error> {
    if var.valueType != SHType_Int4 {
      Err("Expected Int4 variable, but casting failed.")
    } else {
      unsafe {
        Ok((
          var.payload.__bindgen_anon_1.int4Value[0] as i16,
          var.payload.__bindgen_anon_1.int4Value[1] as i16,
          var.payload.__bindgen_anon_1.int4Value[2] as i16,
          var.payload.__bindgen_anon_1.int4Value[3] as i16,
        ))
      }
    }
  }
}

// 16-bit precision :u16
impl TryFrom<&Var> for u16 {
  type Error = &'static str;

  #[inline(always)]
  fn try_from(var: &Var) -> Result<Self, Self::Error> {
    if var.valueType != SHType_Int {
      Err("Expected Int variable, but casting failed.")
    } else {
      unsafe { Ok(var.payload.__bindgen_anon_1.intValue as u16) }
    }
  }
}

// 16-bit precision :[u16;2]
impl TryFrom<&Var> for (u16, u16) {
  type Error = &'static str;

  #[inline(always)]
  fn try_from(var: &Var) -> Result<Self, Self::Error> {
    if var.valueType != SHType_Int2 {
      Err("Expected Int2 variable, but casting failed.")
    } else {
      unsafe {
        Ok((
          u16::from_ne_bytes((var.payload.__bindgen_anon_1.int2Value[0] as i16).to_ne_bytes()),
          u16::from_ne_bytes((var.payload.__bindgen_anon_1.int2Value[1] as i16).to_ne_bytes()),
        ))
      }
    }
  }
}

// 16-bit precision :[u16;3]
impl TryFrom<&Var> for (u16, u16, u16) {
  type Error = &'static str;

  #[inline(always)]
  fn try_from(var: &Var) -> Result<Self, Self::Error> {
    if var.valueType != SHType_Int3 {
      Err("Expected Int3 variable, but casting failed.")
    } else {
      unsafe {
        Ok((
          u16::from_ne_bytes((var.payload.__bindgen_anon_1.int3Value[0] as i16).to_ne_bytes()),
          u16::from_ne_bytes((var.payload.__bindgen_anon_1.int3Value[1] as i16).to_ne_bytes()),
          u16::from_ne_bytes((var.payload.__bindgen_anon_1.int3Value[2] as i16).to_ne_bytes()),
        ))
      }
    }
  }
}

// 16-bit precision :[u16;4]
impl TryFrom<&Var> for (u16, u16, u16, u16) {
  type Error = &'static str;

  #[inline(always)]
  fn try_from(var: &Var) -> Result<Self, Self::Error> {
    if var.valueType != SHType_Int4 {
      Err("Expected Int4 variable, but casting failed.")
    } else {
      unsafe {
        Ok((
          u16::from_ne_bytes((var.payload.__bindgen_anon_1.int4Value[0] as i16).to_ne_bytes()),
          u16::from_ne_bytes((var.payload.__bindgen_anon_1.int4Value[1] as i16).to_ne_bytes()),
          u16::from_ne_bytes((var.payload.__bindgen_anon_1.int4Value[2] as i16).to_ne_bytes()),
          u16::from_ne_bytes((var.payload.__bindgen_anon_1.int4Value[3] as i16).to_ne_bytes()),
        ))
      }
    }
  }
}

impl TryFrom<&Var> for SHColor {
  type Error = &'static str;

  #[inline(always)]
  fn try_from(var: &Var) -> Result<Self, Self::Error> {
    if var.valueType != SHType_Color {
      Err("Expected Color variable, but casting failed.")
    } else {
      unsafe { Ok(var.payload.__bindgen_anon_1.colorValue) }
    }
  }
}

impl TryFrom<&Var> for bool {
  type Error = &'static str;

  #[inline(always)]
  fn try_from(var: &Var) -> Result<Self, Self::Error> {
    if var.valueType != SHType_Bool {
      Err("Expected Bool variable, but casting failed.")
    } else {
      unsafe { Ok(var.payload.__bindgen_anon_1.boolValue) }
    }
  }
}

impl<'a> TryFrom<&'a mut Var> for &'a mut bool {
  type Error = &'static str;

  #[inline(always)]
  fn try_from(var: &'a mut Var) -> Result<Self, Self::Error> {
    if var.valueType != SHType_Bool {
      Err("Expected Bool variable, but casting failed.")
    } else {
      unsafe { Ok(&mut var.payload.__bindgen_anon_1.boolValue) }
    }
  }
}

impl TryFrom<&Var> for &[Var] {
  type Error = &'static str;

  #[inline(always)]
  fn try_from(var: &Var) -> Result<Self, Self::Error> {
    if var.valueType != SHType_Seq {
      Err("Expected Seq variable, but casting failed.")
    } else {
      unsafe {
        let elems = var.payload.__bindgen_anon_1.seqValue.elements;
        let len = var.payload.__bindgen_anon_1.seqValue.len;
        if len == 0 {
          return Ok(&[]);
        }
        let res = std::slice::from_raw_parts(elems, len as usize);
        Ok(res)
      }
    }
  }
}

impl TryFrom<&Var> for WireRef {
  type Error = &'static str;

  #[inline(always)]
  fn try_from(var: &Var) -> Result<Self, Self::Error> {
    if var.valueType != SHType_Wire {
      Err("Expected Wire variable, but casting failed.")
    } else {
      unsafe { Ok(WireRef(var.payload.__bindgen_anon_1.wireValue)) }
    }
  }
}

impl TryFrom<&Var> for TableVar {
  type Error = &'static str;

  #[inline(always)]
  fn try_from(var: &Var) -> Result<Self, Self::Error> {
    if var.valueType != SHType_Table {
      Err("Expected Table variable, but casting failed.")
    } else {
      Ok(TableVar(*var))
    }
  }
}

impl TryFrom<&Var> for SeqVar {
  type Error = &'static str;

  #[inline(always)]
  fn try_from(var: &Var) -> Result<Self, Self::Error> {
    if var.valueType != SHType_Seq {
      Err("Expected Seq variable, but casting failed.")
    } else {
      Ok(SeqVar(*var))
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

impl AsRef<Var> for ClonedVar {
  #[inline(always)]
  fn as_ref(&self) -> &Var {
    &self.0
  }
}

