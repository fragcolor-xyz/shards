/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2020 Fragcolor Pte. Ltd. */

use crate::core::abortWire;
use crate::core::Core;
use crate::shardsc::SHContext;
use crate::shardsc::SHExposedTypesInfo;
use crate::shardsc::SHInstanceData;
use crate::shardsc::SHOptionalString;
use crate::shardsc::SHParameterInfo;
use crate::shardsc::SHParametersInfo;
use crate::shardsc::SHSeq;
use crate::shardsc::SHTable;
use crate::shardsc::SHTypeInfo;
use crate::shardsc::SHTypesInfo;
use crate::shardsc::SHVar;
use crate::shardsc::SHWire;
use crate::shardsc::Shard as CShard;
use crate::shardsc::ShardPtr;
use crate::types::ComposeResult;
use crate::types::Context;
use crate::types::ExposedTypes;
use crate::types::InstanceData;
use crate::types::OptionalString;
use crate::types::Parameters;
use crate::types::Table;
use crate::types::Type;
use crate::types::Types;
use crate::types::Var;
use crate::types::Wire;
use crate::SHError;
use crate::SHShardComposeResult;
use crate::SHString;
use core::convert::TryInto;
use core::result::Result;
use core::slice;
use std::ffi::CStr;
use std::ffi::CString;
use std::os::raw::c_char;

pub trait Shard {
  fn registerName() -> &'static str
  where
    Self: Sized;
  fn hash() -> u32
  where
    Self: Sized;

  fn name(&mut self) -> &str;
  fn help(&mut self) -> OptionalString {
    OptionalString::default()
  }

  fn setup(&mut self) {}
  fn destroy(&mut self) {}

  fn inputTypes(&mut self) -> &Types;
  fn inputHelp(&mut self) -> OptionalString {
    OptionalString::default()
  }

  fn outputTypes(&mut self) -> &Types;
  fn outputHelp(&mut self) -> OptionalString {
    OptionalString::default()
  }

  fn properties(&mut self) -> Option<&Table> {
    None
  }

  fn exposedVariables(&mut self) -> Option<&ExposedTypes> {
    None
  }
  fn requiredVariables(&mut self) -> Option<&ExposedTypes> {
    None
  }

  fn hasCompose() -> bool
  where
    Self: Sized,
  {
    false
  }
  fn compose(&mut self, _data: &InstanceData) -> Result<Type, &str> {
    Ok(Type::default())
  }

  fn hasComposed() -> bool
  where
    Self: Sized,
  {
    false
  }
  fn composed(&mut self, _wire: &SHWire, _results: &ComposeResult) {}

  fn parameters(&mut self) -> Option<&Parameters> {
    None
  }
  fn setParam(&mut self, _index: i32, _value: &Var) -> Result<(), &str> {
    Ok(())
  }
  fn getParam(&mut self, _index: i32) -> Var {
    Var::default()
  }

  fn warmup(&mut self, _context: &Context) -> Result<(), &str> {
    Ok(())
  }
  fn activate(&mut self, context: &Context, input: &Var) -> Result<Var, &str>;
  fn cleanup(&mut self) -> Result<(), &str> {
    Ok(())
  }

  fn nextFrame(&mut self, _context: &Context) -> Result<(), &str> {
    Ok(())
  }

  fn hasMutate() -> bool
  where
    Self: Sized,
  {
    false
  }
  fn mutate(&mut self, _options: Table) {}

  fn hasCrossover() -> bool
  where
    Self: Sized,
  {
    false
  }
  fn crossover(&mut self, _state0: &Var, _state1: &Var) {}

  fn hasState() -> bool
  where
    Self: Sized,
  {
    false
  }
  fn getState(&mut self) -> Var {
    Var::default()
  }
  fn setState(&mut self, _state: &Var) {}
  fn resetState(&mut self) {}
}

#[repr(C)]
pub struct ShardWrapper<T: Shard> {
  header: CShard,
  pub shard: T,
  name: Option<CString>,
  help: Option<CString>,
  error: Option<CString>,
}

/// # Safety
///
/// Used internally actually
pub unsafe extern "C" fn shard_construct<T: Default + Shard>() -> *mut CShard {
  let wrapper: Box<ShardWrapper<T>> = Box::new(create());
  let wptr = Box::into_raw(wrapper);
  wptr as *mut CShard
}

unsafe extern "C" fn shard_name<T: Shard>(arg1: *mut CShard) -> *const ::std::os::raw::c_char {
  let blk = arg1 as *mut ShardWrapper<T>;
  let name = (*blk).shard.name();
  (*blk).name = Some(CString::new(name).expect("CString::new failed"));
  (*blk).name.as_ref().unwrap().as_ptr()
}

unsafe extern "C" fn shard_hash<T: Shard>(_arg1: *mut CShard) -> u32 {
  T::hash()
}

unsafe extern "C" fn shard_help<T: Shard>(arg1: *mut CShard) -> SHOptionalString {
  let blk = arg1 as *mut ShardWrapper<T>;
  (*blk).shard.help().0
}

unsafe extern "C" fn shard_inputHelp<T: Shard>(arg1: *mut CShard) -> SHOptionalString {
  let blk = arg1 as *mut ShardWrapper<T>;
  (*blk).shard.inputHelp().0
}

unsafe extern "C" fn shard_outputHelp<T: Shard>(arg1: *mut CShard) -> SHOptionalString {
  let blk = arg1 as *mut ShardWrapper<T>;
  (*blk).shard.outputHelp().0
}

unsafe extern "C" fn shard_properties<T: Shard>(arg1: *mut CShard) -> *const SHTable {
  let blk = arg1 as *mut ShardWrapper<T>;
  if let Some(properties) = (*blk).shard.properties() {
    &properties.t as *const SHTable
  } else {
    core::ptr::null()
  }
}

unsafe extern "C" fn shard_inputTypes<T: Shard>(arg1: *mut CShard) -> SHTypesInfo {
  let blk = arg1 as *mut ShardWrapper<T>;
  let t = (*blk).shard.inputTypes();
  SHTypesInfo::from(t)
}

unsafe extern "C" fn shard_outputTypes<T: Shard>(arg1: *mut CShard) -> SHTypesInfo {
  let blk = arg1 as *mut ShardWrapper<T>;
  let t = (*blk).shard.outputTypes();
  SHTypesInfo::from(t)
}

unsafe extern "C" fn shard_setup<T: Shard>(arg1: *mut CShard) {
  let blk = arg1 as *mut ShardWrapper<T>;
  (*blk).shard.setup();
}

unsafe extern "C" fn shard_destroy<T: Shard>(arg1: *mut CShard) {
  let blk = arg1 as *mut ShardWrapper<T>;
  (*blk).shard.destroy();
  Box::from_raw(blk); // this will deallocate the Box
}

unsafe extern "C" fn shard_warmup<T: Shard>(arg1: *mut CShard, arg2: *mut SHContext) -> SHError {
  let blk = arg1 as *mut ShardWrapper<T>;
  match (*blk).shard.warmup(&(*arg2)) {
    Ok(_) => SHError::default(),
    Err(error) => {
      (*blk).error = Some(CString::new(error).expect("CString::new failed"));
      SHError {
        message: (*blk).error.as_ref().unwrap().as_ptr(),
        code: 1,
      }
    }
  }
}

unsafe extern "C" fn shard_nextFrame<T: Shard>(arg1: *mut CShard, arg2: *mut SHContext) {
  let blk = arg1 as *mut ShardWrapper<T>;
  if let Err(error) = (*blk).shard.nextFrame(&(*arg2)) {
    abortWire(&(*arg2), error);
  }
}

unsafe extern "C" fn shard_activate<T: Shard>(
  arg1: *mut CShard,
  arg2: *mut SHContext,
  arg3: *const SHVar,
) -> SHVar {
  let blk = arg1 as *mut ShardWrapper<T>;
  match (*blk).shard.activate(&(*arg2), &(*arg3)) {
    Ok(value) => value,
    Err(error) => {
      abortWire(&(*arg2), error);
      Var::default()
    }
  }
}

unsafe extern "C" fn shard_mutate<T: Shard>(arg1: *mut CShard, arg2: SHTable) {
  let blk = arg1 as *mut ShardWrapper<T>;
  (*blk).shard.mutate(arg2.into());
}

unsafe extern "C" fn shard_cleanup<T: Shard>(arg1: *mut CShard) -> SHError {
  let blk = arg1 as *mut ShardWrapper<T>;
  match (*blk).shard.cleanup() {
    Ok(_) => SHError::default(),
    Err(error) => {
      (*blk).error = Some(CString::new(error).expect("CString::new failed"));
      SHError {
        message: (*blk).error.as_ref().unwrap().as_ptr(),
        code: 1,
      }
    }
  }
}

unsafe extern "C" fn shard_exposedVariables<T: Shard>(arg1: *mut CShard) -> SHExposedTypesInfo {
  let blk = arg1 as *mut ShardWrapper<T>;
  if let Some(exposed) = (*blk).shard.exposedVariables() {
    SHExposedTypesInfo::from(exposed)
  } else {
    SHExposedTypesInfo::default()
  }
}

unsafe extern "C" fn shard_requiredVariables<T: Shard>(arg1: *mut CShard) -> SHExposedTypesInfo {
  let blk = arg1 as *mut ShardWrapper<T>;
  if let Some(required) = (*blk).shard.requiredVariables() {
    SHExposedTypesInfo::from(required)
  } else {
    SHExposedTypesInfo::default()
  }
}

unsafe extern "C" fn shard_compose<T: Shard>(
  arg1: *mut CShard,
  data: SHInstanceData,
) -> SHShardComposeResult {
  let blk = arg1 as *mut ShardWrapper<T>;
  match (*blk).shard.compose(&data) {
    Ok(output) => SHShardComposeResult {
      error: SHError::default(),
      result: output,
    },
    Err(error) => {
      (*blk).error = Some(CString::new(error).expect("CString::new failed"));
      data.reportError.unwrap()(
        data.privateContext,
        (*blk).error.as_ref().unwrap().as_ptr(),
        false,
      );
      SHShardComposeResult {
        error: SHError {
          message: (*blk).error.as_ref().unwrap().as_ptr(),
          code: 1,
        },
        result: SHTypeInfo::default(),
      }
    }
  }
}

unsafe extern "C" fn shard_composed<T: Shard>(
  arg1: *mut CShard,
  wire: *const SHWire,
  results: *const ComposeResult,
) {
  let blk = arg1 as *mut ShardWrapper<T>;
  (*blk).shard.composed(&(*wire), &(*results));
}

unsafe extern "C" fn shard_parameters<T: Shard>(arg1: *mut CShard) -> SHParametersInfo {
  let blk = arg1 as *mut ShardWrapper<T>;
  if let Some(params) = (*blk).shard.parameters() {
    SHParametersInfo::from(params)
  } else {
    SHParametersInfo::default()
  }
}

unsafe extern "C" fn shard_getParam<T: Shard>(
  arg1: *mut CShard,
  arg2: ::std::os::raw::c_int,
) -> SHVar {
  let blk = arg1 as *mut ShardWrapper<T>;
  (*blk).shard.getParam(arg2)
}

unsafe extern "C" fn shard_setParam<T: Shard>(
  arg1: *mut CShard,
  arg2: ::std::os::raw::c_int,
  arg3: *const SHVar,
) -> SHError {
  let blk = arg1 as *mut ShardWrapper<T>;
  match (*blk).shard.setParam(arg2, &*arg3) {
    Ok(_) => SHError::default(),
    Err(error) => {
      (*blk).error = Some(CString::new(error).expect("CString::new failed"));
      SHError {
        message: (*blk).error.as_ref().unwrap().as_ptr(),
        code: 1,
      }
    }
  }
}

unsafe extern "C" fn shard_crossover<T: Shard>(arg1: *mut CShard, s0: *const Var, s1: *const Var) {
  let blk = arg1 as *mut ShardWrapper<T>;
  (*blk).shard.crossover(&*s0, &*s1);
}

unsafe extern "C" fn shard_getState<T: Shard>(arg1: *mut CShard) -> Var {
  let blk = arg1 as *mut ShardWrapper<T>;
  (*blk).shard.getState()
}

unsafe extern "C" fn shard_setState<T: Shard>(arg1: *mut CShard, state: *const Var) {
  let blk = arg1 as *mut ShardWrapper<T>;
  (*blk).shard.setState(&*state);
}

unsafe extern "C" fn shard_resetState<T: Shard>(arg1: *mut CShard) {
  let blk = arg1 as *mut ShardWrapper<T>;
  (*blk).shard.resetState();
}

pub fn create<T: Default + Shard>() -> ShardWrapper<T> {
  ShardWrapper::<T> {
    header: CShard {
      inlineShardId: 0,
      owned: false,
      name: Some(shard_name::<T>),
      hash: Some(shard_hash::<T>),
      help: Some(shard_help::<T>),
      inputHelp: Some(shard_inputHelp::<T>),
      outputHelp: Some(shard_outputHelp::<T>),
      properties: Some(shard_properties::<T>),
      inputTypes: Some(shard_inputTypes::<T>),
      outputTypes: Some(shard_outputTypes::<T>),
      setup: Some(shard_setup::<T>),
      destroy: Some(shard_destroy::<T>),
      exposedVariables: Some(shard_exposedVariables::<T>),
      requiredVariables: Some(shard_requiredVariables::<T>),
      compose: if T::hasCompose() {
        Some(shard_compose::<T>)
      } else {
        None
      },
      composed: if T::hasComposed() {
        Some(shard_composed::<T>)
      } else {
        None
      },
      parameters: Some(shard_parameters::<T>),
      setParam: Some(shard_setParam::<T>),
      getParam: Some(shard_getParam::<T>),
      warmup: Some(shard_warmup::<T>),
      nextFrame: Some(shard_nextFrame::<T>),
      activate: Some(shard_activate::<T>),
      cleanup: Some(shard_cleanup::<T>),
      mutate: if T::hasMutate() {
        Some(shard_mutate::<T>)
      } else {
        None
      },
      crossover: if T::hasCrossover() {
        Some(shard_crossover::<T>)
      } else {
        None
      },
      getState: if T::hasState() {
        Some(shard_getState::<T>)
      } else {
        None
      },
      setState: if T::hasState() {
        Some(shard_setState::<T>)
      } else {
        None
      },
      resetState: if T::hasState() {
        Some(shard_resetState::<T>)
      } else {
        None
      },
    },
    shard: T::default(),
    name: None,
    help: None,
    error: None,
  }
}
