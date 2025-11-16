/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2020 Fragcolor Pte. Ltd. */

//! Parameter variable types (ParamVar and ShardsVar).

use super::*;
use crate::core::{cloneVar, destroyVar, Core};
use crate::shardsc::*;
use crate::{shlog, SHStringWithLen};
use std::mem::transmute;

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

  pub fn new_named(name: &str) -> ParamVar {
    let mut var = ParamVar::default();
    var.set_name(name);
    var
  }

  pub fn cleanup(&mut self, _ctx: Option<&Context>) {
    unsafe {
      if self.parameter.0.valueType == SHType_ContextVar {
        (*Core).releaseVariable.unwrap_unchecked()(self.pointee);
      }
      self.pointee = std::ptr::null_mut();
    }
  }

  pub fn warmup(&mut self, context: &Context) {
    if self.parameter.0.valueType == SHType_ContextVar {
      assert_eq!(self.pointee, std::ptr::null_mut());
      unsafe {
        let ctx = context as *const SHContext as *mut SHContext;
        self.pointee = (*Core).referenceVariable.unwrap_unchecked()(
          ctx,
          SHStringWithLen {
            string: self
              .parameter
              .0
              .payload
              .__bindgen_anon_1
              .__bindgen_anon_2
              .stringValue,
            len: self
              .parameter
              .0
              .payload
              .__bindgen_anon_1
              .__bindgen_anon_2
              .stringLen as u64,
          },
        );
      }
    } else {
      self.pointee = &mut self.parameter.0 as *mut Var;
    }
    assert_ne!(self.pointee, std::ptr::null_mut());
  }

  pub fn set_fast_unsafe(&mut self, value: &Var) {
    // avoid overwrite refcount
    assert_ne!(self.pointee, std::ptr::null_mut());
    unsafe {
      // store flags and rc
      let rc = (*self.pointee).refcount;
      let flags = (*self.pointee).flags;
      // assign
      (*self.pointee) = *value;
      // restore flags and rc
      (*self.pointee).flags = flags;
      (*self.pointee).refcount = rc;
    }
  }

  pub fn set_cloning(&mut self, value: &Var) {
    unsafe { (*Core).cloneVar.unwrap_unchecked()(self.pointee, value) };
    // notice we don't need to fix up ref-counting because cloning does not touch that
  }

  pub fn get(&self) -> &Var {
    assert_ne!(self.pointee, std::ptr::null_mut());
    unsafe { &*self.pointee }
  }

  // Users should never fully overwrite or flags will be lost, unless taken care of
  pub fn get_mut(&mut self) -> &mut Var {
    assert_ne!(self.pointee, std::ptr::null_mut());
    unsafe { &mut *self.pointee }
  }

  pub fn try_get(&self) -> Option<&Var> {
    if self.pointee.is_null() {
      None
    } else {
      Some(unsafe { &*self.pointee })
    }
  }

  pub fn assign(&mut self, value: &Var) {
    self.parameter = value.into();
  }

  pub fn set_param(&mut self, value: &Var) -> Result<(), &'static str> {
    self.assign(value);
    Ok(())
  }

  pub fn get_param(&self) -> Var {
    self.parameter.0
  }

  pub fn is_variable(&self) -> bool {
    self.parameter.0.valueType == SHType_ContextVar
  }

  pub fn is_none(&self) -> bool {
    self.parameter.0.valueType == SHType_None
  }

  pub fn set_name(&mut self, name: &str) {
    let s = Var::ephemeral_string(name);
    self.parameter = s.into(); // clone it!
    self.parameter.0.valueType = SHType_ContextVar;
  }

  pub fn get_name(&self) -> *const std::os::raw::c_char {
    if self.is_variable() {
      (&self.parameter.0)
        .try_into()
        .expect("parameter name is a string")
    } else {
      std::ptr::null()
    }
  }
}

impl Default for ParamVar {
  fn default() -> Self {
    ParamVar::new(().into())
  }
}

impl Drop for ParamVar {
  fn drop(&mut self) {
    self.cleanup(None);
  }
}

// ShardsVar

pub struct ShardsVar {
  param: ClonedVar,
  shards: Vec<ShardRef>,
  compose_result: Option<SHComposeResult>,
  native_shards: Shards,
}

impl Default for ShardsVar {
  fn default() -> Self {
    // Initialize with nullptr terminator to guarantee native_shards.elements is ALWAYS valid
    // Even if set_param is never called, activate() can safely be called (will do nothing)
    let mut shards = Vec::new();
    shards.push(ShardRef(std::ptr::null_mut()));

    let mut result = ShardsVar {
      param: ClonedVar::default(),
      shards,
      compose_result: None,
      native_shards: Shards {
        elements: std::ptr::null_mut(),
        len: 0,
        cap: 0,
      },
    };

    // Update pointer AFTER move to ensure it points to the Vec's final location
    result.native_shards.elements = result.shards.as_mut_ptr() as *mut *mut _;
    result
  }
}

impl Drop for ShardsVar {
  fn drop(&mut self) {
    self.destroy();
  }
}

impl Clone for ShardsVar {
  fn clone(&self) -> Self {
    let mut c = ShardsVar::default();
    c.set_param(&self.param.0).unwrap();
    c
  }
}

impl ShardsVar {
  fn destroy(&mut self) {
    // Skip the last element (NULL terminator) if present
    let shards_to_cleanup = if !self.shards.is_empty() && self.shards.last().unwrap().0.is_null() {
      &self.shards[..self.shards.len() - 1]
    } else {
      &self.shards[..]
    };

    for shard in shards_to_cleanup {
      if let Err(e) = shard.cleanup(None) {
        shlog!("Errors during shard cleanup: {}", e);
      }
    }
    self.shards.clear();
    destroyVar(&mut self.param.0);

    // clear old results if any
    if let Some(compose_result) = self.compose_result {
      unsafe { (*Core).freeComposeResult.unwrap_unchecked()(&compose_result as *const _ as *mut _) }
      self.compose_result = None;
    }
  }

  #[inline(always)]
  pub fn cleanup(&mut self, ctx: Option<&Context>) {
    // Skip the last element (NULL terminator) if present
    let shards_to_cleanup = if !self.shards.is_empty() && self.shards.last().unwrap().0.is_null() {
      &self.shards[..self.shards.len() - 1]
    } else {
      &self.shards[..]
    };

    for shard in shards_to_cleanup.iter().rev() {
      if let Err(e) = shard.cleanup(ctx) {
        shlog!("Errors during shard cleanup: {}", e);
      }
    }
  }

  #[inline(always)]
  pub fn warmup(&self, context: &Context) -> Result<(), &'static str> {
    // Skip the last element (NULL terminator) if present
    let shards_to_warmup = if !self.shards.is_empty() && self.shards.last().unwrap().0.is_null() {
      &self.shards[..self.shards.len() - 1]
    } else {
      &self.shards[..]
    };

    for shard in shards_to_warmup.iter() {
      if let Err(e) = shard.warmup(context) {
        shlog!("Errors during shard warmup: {}", e);
        return Err(e);
      }
    }
    Ok(())
  }

  pub fn set_param(&mut self, value: &Var) -> Result<(), &'static str> {
    self.destroy(); // destroy old blocks

    self.param = value.into(); // clone it

    if let Ok(s) = Seq::try_from(self.param.0.as_ref()) {
      for shard in s.iter() {
        self.shards.push(shard.as_ref().try_into()?);
      }
    } else if let Ok(s) = ShardRef::try_from(&self.param.0) {
      self.shards.push(s);
    } else if !value.is_none() {
      return Err("Expected sequence or shard variable, but casting failed.");
    }
    // else: value.is_none() is allowed, we just have an empty array

    // ALWAYS add NULL terminator for NULL-terminated array iteration
    // Even for empty arrays, this ensures native_shards.elements is never nullptr
    self.shards.push(ShardRef(std::ptr::null_mut()));

    self.native_shards = Shards {
      elements: self.shards.as_mut_ptr() as *mut *mut _,
      len: (self.shards.len() - 1) as u32, // Don't count the NULL terminator in length
      cap: 0,
    };

    Ok(())
  }

  #[inline(always)]
  pub fn get_param(&self) -> Var {
    self.param.0
  }

  #[inline(always)]
  pub fn compose(&mut self, data: &InstanceData) -> Result<&ComposeResult, &'static str> {
    // clear old results if any
    if let Some(compose_result) = self.compose_result {
      unsafe { (*Core).freeComposeResult.unwrap_unchecked()(&compose_result as *const _ as *mut _) }
      self.compose_result = None;
    }

    // native_shards is ALWAYS valid, even for empty arrays (they have [nullptr])
    let result = unsafe { (*Core).composeShards.unwrap_unchecked()(self.native_shards, *data) };

    if result.failed {
      unsafe { (*Core).freeComposeResult.unwrap_unchecked()(&result as *const _ as *mut _) }
      Err("Composition failed.")
    } else {
      self.compose_result = Some(result);
      Ok(self.compose_result.as_ref().unwrap())
    }
  }

  #[inline(always)]
  pub fn activate(&self, context: &Context, input: &Var, output: &mut Var) -> WireState {
    // native_shards.elements is ALWAYS valid (never nullptr)
    // Even empty shards arrays have [nullptr] terminator, which VM loop handles correctly
    unsafe {
      (*Core).runShards.unwrap_unchecked()(
        self.native_shards.elements,
        context as *const _ as *mut _,
        input,
        output,
      )
      .into()
    }
  }

  #[inline(always)]
  pub fn activate_handling_return(
    &self,
    context: &Context,
    input: &Var,
    output: &mut Var,
  ) -> WireState {
    // native_shards.elements is ALWAYS valid (never nullptr)
    // Even empty shards arrays have [nullptr] terminator, which VM loop handles correctly
    unsafe {
      (*Core).runShards2.unwrap_unchecked()(
        self.native_shards.elements,
        context as *const _ as *mut _,
        input,
        output,
      )
      .into()
    }
  }

  #[inline(always)]
  pub fn is_empty(&self) -> bool {
    self.param.0.is_none()
  }

  #[inline(always)]
  pub fn get_exposing(&self) -> Option<&[ExposedInfo]> {
    self.compose_result.map(|compose_result| unsafe {
      let elems = compose_result.exposedInfo.elements;
      let len = compose_result.exposedInfo.len;
      if len == 0 {
        &[]
      } else {
        std::slice::from_raw_parts(elems, len as usize)
      }
    })
  }

  #[inline(always)]
  pub fn get_requiring(&self) -> Option<&[ExposedInfo]> {
    self.compose_result.map(|compose_result| unsafe {
      let elems = compose_result.requiredInfo.elements;
      let len = compose_result.requiredInfo.len;
      if len == 0 {
        &[]
      } else {
        std::slice::from_raw_parts(elems, len as usize)
      }
    })
  }
}

impl From<&ShardsVar> for Shards {
  fn from(v: &ShardsVar) -> Self {
    v.native_shards
  }
}

impl From<&ShardsVar> for &SHVar {
  fn from(v: &ShardsVar) -> Self {
    (&v.param).into()
  }
}

impl From<&ShardsVar> for Var {
  fn from(v: &ShardsVar) -> Self {
    (&v.param).into()
  }
}

impl From<&ClonedVar> for &SHVar {
  fn from(value: &ClonedVar) -> Self {
    unsafe { transmute(value) }
  }
}

impl From<&ClonedVar> for Var {
  fn from(value: &ClonedVar) -> Self {
    let v: &Var = unsafe { transmute(value) };
    *v
  }
}

impl From<&ParamVar> for &SHVar {
  fn from(value: &ParamVar) -> Self {
    unsafe { transmute(value) }
  }
}

impl From<&ParamVar> for Var {
  fn from(value: &ParamVar) -> Self {
    let v: &Var = unsafe { transmute(value) };
    *v
  }
}

impl IntoIterator for TableVar {
  type Item = (Var, Var);
  type IntoIter = TableIterator;

  fn into_iter(self) -> Self::IntoIter {
    unsafe {
      let it = TableIterator {
        table: self.0.payload.__bindgen_anon_1.tableValue,
        citer: [0; 64],
      };
      (*it.table.api).tableGetIterator.unwrap_unchecked()(
        it.table,
        &it.citer as *const _ as *mut _,
      );
      it
    }
  }
}

impl IntoIterator for SeqVar {
  type Item = SHVar;
  type IntoIter = SeqVarIterator;

  fn into_iter(self) -> Self::IntoIter {
    self.iter()
  }
}

impl IntoIterator for &SeqVar {
  type Item = SHVar;
  type IntoIter = SeqVarIterator;

  fn into_iter(self) -> Self::IntoIter {
    self.iter()
  }
}

