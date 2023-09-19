use std::ffi::{CStr, CString};

use crate::{
  core::{referenceVariable, releaseVariable},
  types::{Context, ExposedInfo, ExposedTypes, ParamVar, SeqVar, ShardsVar, TableVar, Var},
  SHExposedTypesInfo, SHString, SHVar,
};

pub fn collect_required_variables(
  shared: &SHExposedTypesInfo,
  out: &mut ExposedTypes,
  var: &SHVar,
) -> Result<(), &'static str> {
  match var.valueType {
    crate::SHType_ContextVar => {
      let var_name: &str = var
        .try_into()
        .map_err(|_x| "Invalid context variable name")?;
      for entry in shared {
        let cstr = unsafe { CStr::from_ptr(entry.name) };
        if var_name == cstr.to_str().map_err(|_x| "invalid string")? {
          out.push(ExposedInfo::new(
            unsafe { var.payload.__bindgen_anon_1.__bindgen_anon_2.stringValue },
            entry.exposedType,
          ));
          break;
        }
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
pub fn get_or_var<'a: 'c, 'b: 'c, 'c>(v: &'a Var, ctx: &'b Context) -> &'c Var {
  if v.is_context_var() {
    unsafe {
      let str = v.payload.__bindgen_anon_1.__bindgen_anon_2;
      let result = referenceVariable(
        ctx,
        crate::SHStringWithLen {
          string: str.stringValue,
          len: str.stringLen as usize,
        },
      );
      releaseVariable(result);
      result
    }
  } else {
    &v
  }
}
