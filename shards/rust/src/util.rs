use std::ffi::{CStr, CString};
use std::os::raw::c_char;

use crate::{
  core::{deriveType, VarRef},
  types::{
    Context, DerivedType, ExposedInfo, ExposedTypes, ParamVar, SeqVar, ShardsVar, TableVar, Type,
    Var,
  },
  SHExposedTypeInfo, SHExposedTypesInfo, SHInstanceData, SHString, SHTypesInfo, SHVar,
};

pub enum TypeOrDerived {
  Derived(DerivedType),
  Static(Type),
}

impl<'a> From<&'a TypeOrDerived> for &'a Type {
  fn from(value: &'a TypeOrDerived) -> &'a Type {
    match value {
      TypeOrDerived::Derived(t) => &t.0,
      TypeOrDerived::Static(t) => &t,
    }
  }
}

pub fn get_param_var_type(
  instance_data: &SHInstanceData,
  var: &ParamVar,
) -> Result<TypeOrDerived, &'static str> {
  if var.is_variable() {
    let exp_type = find_exposed_variable(&instance_data.shared, &var.get_param())?
      .ok_or("Could not find exposed variable for parameter")?;
    Ok(TypeOrDerived::Static(exp_type.exposedType))
  } else {
    Ok(TypeOrDerived::Derived(deriveType(
      &var.get_param(),
      instance_data,
      false,
    )))
  }
}

pub fn find_exposed_variable(
  shared: &SHExposedTypesInfo,
  var: &SHVar,
) -> Result<Option<SHExposedTypeInfo>, &'static str> {
  let var_name: &str = var
    .try_into()
    .map_err(|_x| "find_exposed_variable: Invalid context variable name")?;
  for entry in shared {
    let cstr = unsafe { CStr::from_ptr(entry.name) };
    if var_name
      == cstr
        .to_str()
        .map_err(|_x| "find_exposed_variable: Invalid string")?
    {
      return Ok(Some(ExposedInfo::new(
        unsafe { var.payload.__bindgen_anon_1.__bindgen_anon_2.stringValue },
        entry.exposedType,
      )));
    }
  }
  return Ok(None);
}

pub fn collect_required_variables(
  shared: &SHExposedTypesInfo,
  out: &mut ExposedTypes,
  var: &SHVar,
) -> Result<(), &'static str> {
  match var.valueType {
    crate::SHType_ContextVar => {
      if let Some(exposed_var) = find_exposed_variable(shared, var)? {
        out.push(exposed_var);
      }
    }
    crate::SHType_Seq => {
      for v in SeqVar(*var) {
        collect_required_variables(shared, out, &v)?;
      }
    }
    crate::SHType_Table => {
      for (_k, v) in TableVar(*var) {
        collect_required_variables(shared, out, &v)?;
      }
    }
    _ => {}
  }
  Ok(())
}

extern "C" {
  fn shards_collect_required_variables_typed(
    data: *const SHInstanceData,
    out: *mut SHExposedTypesInfo,
    var: *const SHVar,
    valid_types: *const SHTypesInfo,
    debug_tag: *const c_char,
  ) -> bool;
}

/// Check if a type can possibly contain context variables
/// This is used to optimize compose by skipping collection when impossible
pub fn has_context_variables(type_: &Type) -> bool {
  use crate::SHType_ContextVar;
  use crate::SHType_Seq;
  use crate::SHType_Table;

  match type_.basicType {
    SHType_ContextVar => true,
    SHType_Seq => {
      let seq_types = unsafe { type_.details.seqTypes };
      if seq_types.len > 0 && !seq_types.elements.is_null() {
        for i in 0..seq_types.len {
          let t = unsafe { &*seq_types.elements.offset(i as isize) };
          if has_context_variables(t) {
            return true;
          }
        }
      }
      false
    }
    SHType_Table => {
      let table_types = unsafe { type_.details.table.types };
      if table_types.len > 0 && !table_types.elements.is_null() {
        for i in 0..table_types.len {
          let t = unsafe { &*table_types.elements.offset(i as isize) };
          if has_context_variables(t) {
            return true;
          }
        }
      }
      false
    }
    _ => false,
  }
}

/// Collects required variables with type validation
/// This validates that the variable type matches one of the validTypes before collecting
pub fn collect_required_variables_typed(
  data: &SHInstanceData,
  out: &mut ExposedTypes,
  var: &SHVar,
  valid_types: &[Type],
  param_name: &str,
) -> Result<(), &'static str> {
  // Handle empty types array - create a properly aligned non-null pointer
  let types_info = if valid_types.is_empty() {
    SHTypesInfo {
      elements: std::ptr::NonNull::dangling().as_ptr(),
      len: 0,
      cap: 0,
    }
  } else {
    SHTypesInfo {
      elements: valid_types.as_ptr() as *mut Type,
      len: valid_types.len() as u32,
      cap: 0,
    }
  };

  let c_param_name = CString::new(param_name).map_err(|_| "Invalid parameter name")?;

  let success = unsafe {
    shards_collect_required_variables_typed(
      data as *const SHInstanceData,
      out as *mut ExposedTypes as *mut SHExposedTypesInfo,
      var as *const SHVar,
      &types_info as *const SHTypesInfo,
      c_param_name.as_ptr(),
    )
  };

  if success {
    Ok(())
  } else {
    Err("Type validation failed for parameter")
  }
}

/// Adds required variables from inside a ShardsVar
pub fn require_shards_contents(required: &mut ExposedTypes, contents: &ShardsVar) -> bool {
  if !contents.is_empty() {
    if let Some(requiring) = contents.get_requiring() {
      for exp in requiring {
        required.push(*exp);
      }
      true
    } else {
      false
    }
  } else {
    false
  }
}

/// Adds exposed variables from within a ShardsVar
pub fn expose_shards_contents(exposed: &mut ExposedTypes, contents: &ShardsVar) -> bool {
  if !contents.is_empty() {
    if let Some(exposing) = contents.get_exposing() {
      for exp in exposing {
        exposed.push(*exp);
      }
      true
    } else {
      false
    }
  } else {
    false
  }
}

/// Use to resolve variables for ContextVars nested inside parameters
/// (Very ugly but works, don't use it in other places)
pub fn get_or_var<'a: 'c, 'b: 'c, 'c>(v: &'a Var, ctx: &'b Context) -> &'c Var {
  let maybeStr: Result<&str, _> = v.try_into();
  if let Ok(str) = maybeStr {
    let var_ref = VarRef::reference(ctx, str); // this triggers a lot of lookups etc
    let ptr = var_ref.as_ptr();
    unsafe { ptr.as_mut().unwrap() }
  } else {
    &v
  }
}

pub fn merge_exposed_types(exposed: &mut ExposedTypes, types: &SHExposedTypesInfo) {
  for t in types {
    exposed.push(t);
  }
}

/// Creates a slice from a raw pointer and a length, allowing a null pointer when length is 0.
///
/// # Safety
///
/// The caller must ensure that the pointer is valid and points to `len` elements of type `T`.
/// If `len` is 0, the pointer can be null. The resulting slice must not be used after the memory
/// it points to has been deallocated or modified.
///
/// # Panics
///
/// This function does not panic.
#[inline]
pub unsafe fn from_raw_parts_allow_null<'a, T>(data: *const T, len: usize) -> &'a [T] {
  if len == 0 {
    &[]
  } else {
    std::slice::from_raw_parts(data, len)
  }
}
