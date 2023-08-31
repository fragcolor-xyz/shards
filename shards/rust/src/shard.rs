/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2020 Fragcolor Pte. Ltd. */

use crate::core::abortWire;
use crate::core::Core;
use crate::shardsc::SHContext;
use crate::shardsc::SHError;
use crate::shardsc::SHExposedTypesInfo;
use crate::shardsc::SHInstanceData;
use crate::shardsc::SHOptionalString;
use crate::shardsc::SHParameterInfo;
use crate::shardsc::SHParametersInfo;
use crate::shardsc::SHSeq;
use crate::shardsc::SHShardComposeResult;
use crate::shardsc::SHString;
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
use crate::SHStringWithLen;
use core::convert::TryInto;
use core::result::Result;
use core::slice;
use std::ffi::CStr;
use std::ffi::CString;
use std::os::raw::c_char;

pub trait ShardDesc {
  fn register_name() -> &'static str
  where
    Self: Sized;
  fn hash() -> u32
  where
    Self: Sized;
  fn name(&mut self) -> &str;
  fn help(&mut self) -> OptionalString;
  fn parameters(&mut self) -> Option<&Parameters>;
  fn set_param(&mut self, _index: i32, _value: &Var) -> Result<(), &str>;
  fn get_param(&mut self, _index: i32) -> Var;
  fn required_variables(&mut self) -> Option<&ExposedTypes>;
}

pub trait Shard2Generated {
  fn has_setup() -> bool;
  fn has_destroy() -> bool;
  fn has_input_help() -> bool;
  fn has_output_help() -> bool;
  fn has_properties() -> bool;
  fn has_exposed_variables() -> bool;
  fn has_compose() -> bool;
  fn has_warmup() -> bool;
  fn has_cleanup() -> bool;
  fn has_mutate() -> bool;
  fn has_crossover() -> bool;
  fn has_get_state() -> bool;
  fn has_set_state() -> bool;
  fn has_reset_state() -> bool;
}

pub trait Shard2 {
  fn setup(&mut self) {}
  fn destroy(&mut self) {}

  fn input_types(&mut self) -> &Types;
  fn input_help(&mut self) -> OptionalString {
    OptionalString::default()
  }

  fn output_types(&mut self) -> &Types;
  fn output_help(&mut self) -> OptionalString {
    OptionalString::default()
  }

  fn properties(&mut self) -> Option<&Table> {
    None
  }

  fn exposed_variables(&mut self) -> Option<&ExposedTypes> {
    None
  }

  fn compose(&mut self, _data: &InstanceData) -> Result<Type, &str> {
    Ok(Type::default())
  }
  fn warmup(&mut self, _context: &Context) -> Result<(), &str> {
    Ok(())
  }
  fn cleanup(&mut self) -> Result<(), &str> {
    Ok(())
  }
  fn activate(&mut self, context: &Context, input: &Var) -> Result<Var, &str>;

  fn mutate(&mut self, _options: Table) {}

  fn crossover(&mut self, _state0: &Var, _state1: &Var) {}

  fn get_state(&mut self) -> Var {
    Var::default()
  }
  fn set_state(&mut self, _state: &Var) {}
  fn reset_state(&mut self) {}
}

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

  fn parameters(&mut self) -> Option<&Parameters> {
    None
  }
  fn setParam(&mut self, _index: i32, _value: &Var) -> Result<(), &str> {
    Ok(())
  }
  fn getParam(&mut self, _index: i32) -> Var {
    Var::default()
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

  fn warmup(&mut self, _context: &Context) -> Result<(), &str> {
    Ok(())
  }
  fn activate(&mut self, context: &Context, input: &Var) -> Result<Var, &str>;
  fn cleanup(&mut self) -> Result<(), &str> {
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

#[repr(C, align(16))] // ensure alignment is 16 bytes
pub struct ShardWrapper<T: Shard> {
  header: CShard,
  pub shard: T,
  name: Option<CString>,
  help: Option<CString>,
  error: Option<CString>,
}

#[repr(C, align(16))] // ensure alignment is 16 bytes
pub struct ShardWrapper2<T: ShardDesc + Shard2 + Shard2Generated> {
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

pub unsafe extern "C" fn shard_construct2<T: Default + ShardDesc + Shard2 + Shard2Generated>() -> *mut CShard {
  let wrapper: Box<ShardWrapper2<T>> = Box::new(create2());
  let wptr = Box::into_raw(wrapper);
  wptr as *mut CShard
}

unsafe extern "C" fn shard_name<T: Shard>(arg1: *mut CShard) -> *const ::std::os::raw::c_char {
  let blk = arg1 as *mut ShardWrapper<T>;
  if (*blk).name.is_some() {
    return (*blk).name.as_ref().unwrap().as_ptr();
  } else {
    let name = (*blk).shard.name();
    (*blk).name = Some(CString::new(name).expect("CString::new failed"));
    (*blk).name.as_ref().unwrap().as_ptr()
  }
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
  drop(Box::from_raw(blk)); // this will deallocate the Box
}

unsafe extern "C" fn shard_warmup<T: Shard>(arg1: *mut CShard, arg2: *mut SHContext) -> SHError {
  let blk = arg1 as *mut ShardWrapper<T>;
  match (*blk).shard.warmup(&(*arg2)) {
    Ok(_) => SHError::default(),
    Err(error) => SHError {
      message: SHStringWithLen {
        string: error.as_ptr() as *const c_char,
        len: error.len(),
      },
      code: 1,
    },
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
    Err(error) => SHError {
      message: SHStringWithLen {
        string: error.as_ptr() as *const c_char,
        len: error.len(),
      },
      code: 1,
    },
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
  data: *mut SHInstanceData,
) -> SHShardComposeResult {
  let blk = arg1 as *mut ShardWrapper<T>;
  match (*blk).shard.compose(&*data) {
    Ok(output) => SHShardComposeResult {
      error: SHError::default(),
      result: output,
    },
    Err(error) => SHShardComposeResult {
      error: SHError {
        message: SHStringWithLen {
          string: error.as_ptr() as *const c_char,
          len: error.len(),
        },
        code: 1,
      },
      result: SHTypeInfo::default(),
    },
  }
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
    Err(error) => SHError {
      message: SHStringWithLen {
        string: error.as_ptr() as *const c_char,
        len: error.len(),
      },
      code: 1,
    },
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
  let mut shard = ShardWrapper::<T> {
    header: CShard {
      inlineShardId: 0,
      refCount: 0,
      owned: false,
      nameLength: 0,
      line: 0,
      column: 0,
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
      parameters: Some(shard_parameters::<T>),
      setParam: Some(shard_setParam::<T>),
      getParam: Some(shard_getParam::<T>),
      warmup: Some(shard_warmup::<T>),
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
  };
  shard.header.nameLength = shard.shard.name().len() as u32;
  return shard;
}

unsafe extern "C" fn shard2_name<T: ShardDesc + Shard2 + Shard2Generated>(arg1: *mut CShard) -> *const ::std::os::raw::c_char {
  let blk = arg1 as *mut ShardWrapper2<T>;
  if (*blk).name.is_some() {
    return (*blk).name.as_ref().unwrap().as_ptr();
  } else {
    let name = (*blk).shard.name();
    (*blk).name = Some(CString::new(name).expect("CString::new failed"));
    (*blk).name.as_ref().unwrap().as_ptr()
  }
}

unsafe extern "C" fn shard2_hash<T: ShardDesc + Shard2 + Shard2Generated>(_arg1: *mut CShard) -> u32 {
  T::hash()
}

unsafe extern "C" fn shard2_help<T: ShardDesc + Shard2 + Shard2Generated>(arg1: *mut CShard) -> SHOptionalString {
  let blk = arg1 as *mut ShardWrapper2<T>;
  (*blk).shard.help().0
}

unsafe extern "C" fn shard2_requiredVariables<T: ShardDesc + Shard2 + Shard2Generated>(arg1: *mut CShard) -> SHExposedTypesInfo {
  let blk = arg1 as *mut ShardWrapper2<T>;
  if let Some(required) = (*blk).shard.required_variables() {
    SHExposedTypesInfo::from(required)
  } else {
    SHExposedTypesInfo::default()
  }
}

unsafe extern "C" fn shard2_parameters<T: ShardDesc + Shard2 + Shard2Generated>(arg1: *mut CShard) -> SHParametersInfo {
  let blk = arg1 as *mut ShardWrapper2<T>;
  if let Some(params) = (*blk).shard.parameters() {
    SHParametersInfo::from(params)
  } else {
    SHParametersInfo::default()
  }
}

unsafe extern "C" fn shard2_getParam<T: ShardDesc + Shard2 + Shard2Generated>(
  arg1: *mut CShard,
  arg2: ::std::os::raw::c_int,
) -> SHVar {
  let blk = arg1 as *mut ShardWrapper2<T>;
  (*blk).shard.get_param(arg2)
}

unsafe extern "C" fn shard2_setParam<T: ShardDesc + Shard2 + Shard2Generated>(
  arg1: *mut CShard,
  arg2: ::std::os::raw::c_int,
  arg3: *const SHVar,
) -> SHError {
  let blk = arg1 as *mut ShardWrapper2<T>;
  match (*blk).shard.set_param(arg2, &*arg3) {
    Ok(_) => SHError::default(),
    Err(error) => SHError {
      message: SHStringWithLen {
        string: error.as_ptr() as *const c_char,
        len: error.len(),
      },
      code: 1,
    },
  }
}

unsafe extern "C" fn shard2_inputHelp<T: ShardDesc + Shard2 + Shard2Generated>(arg1: *mut CShard) -> SHOptionalString {
  let blk = arg1 as *mut ShardWrapper2<T>;
  (*blk).shard.input_help().0
}

unsafe extern "C" fn shard2_outputHelp<T: ShardDesc + Shard2 + Shard2Generated>(arg1: *mut CShard) -> SHOptionalString {
  let blk = arg1 as *mut ShardWrapper2<T>;
  (*blk).shard.output_help().0
}

unsafe extern "C" fn shard2_properties<T: ShardDesc + Shard2 + Shard2Generated>(arg1: *mut CShard) -> *const SHTable {
  let blk = arg1 as *mut ShardWrapper2<T>;
  if let Some(properties) = (*blk).shard.properties() {
    &properties.t as *const SHTable
  } else {
    core::ptr::null()
  }
}

unsafe extern "C" fn shard2_inputTypes<T: ShardDesc + Shard2 + Shard2Generated>(arg1: *mut CShard) -> SHTypesInfo {
  let blk = arg1 as *mut ShardWrapper2<T>;
  let t = (*blk).shard.input_types();
  SHTypesInfo::from(t)
}

unsafe extern "C" fn shard2_outputTypes<T: ShardDesc + Shard2 + Shard2Generated>(arg1: *mut CShard) -> SHTypesInfo {
  let blk = arg1 as *mut ShardWrapper2<T>;
  let t = (*blk).shard.output_types();
  SHTypesInfo::from(t)
}

unsafe extern "C" fn shard2_setup<T: ShardDesc + Shard2 + Shard2Generated>(arg1: *mut CShard) {
  let blk = arg1 as *mut ShardWrapper2<T>;
  (*blk).shard.setup();
}

unsafe extern "C" fn shard2_destroy<T: ShardDesc + Shard2 + Shard2Generated>(arg1: *mut CShard) {
  let blk = arg1 as *mut ShardWrapper2<T>;
  (*blk).shard.destroy();
  drop(Box::from_raw(blk)); // this will deallocate the Box
}

unsafe extern "C" fn shard2_warmup<T: ShardDesc + Shard2 + Shard2Generated>(arg1: *mut CShard, arg2: *mut SHContext) -> SHError {
  let blk = arg1 as *mut ShardWrapper2<T>;
  match (*blk).shard.warmup(&(*arg2)) {
    Ok(_) => SHError::default(),
    Err(error) => SHError {
      message: SHStringWithLen {
        string: error.as_ptr() as *const c_char,
        len: error.len(),
      },
      code: 1,
    },
  }
}

unsafe extern "C" fn shard2_activate<T: ShardDesc + Shard2 + Shard2Generated>(
  arg1: *mut CShard,
  arg2: *mut SHContext,
  arg3: *const SHVar,
) -> SHVar {
  let blk = arg1 as *mut ShardWrapper2<T>;
  match (*blk).shard.activate(&(*arg2), &(*arg3)) {
    Ok(value) => value,
    Err(error) => {
      abortWire(&(*arg2), error);
      Var::default()
    }
  }
}

unsafe extern "C" fn shard2_mutate<T: ShardDesc + Shard2 + Shard2Generated>(arg1: *mut CShard, arg2: SHTable) {
  let blk = arg1 as *mut ShardWrapper2<T>;
  (*blk).shard.mutate(arg2.into());
}

unsafe extern "C" fn shard2_cleanup<T: ShardDesc + Shard2 + Shard2Generated>(arg1: *mut CShard) -> SHError {
  let blk = arg1 as *mut ShardWrapper2<T>;
  match (*blk).shard.cleanup() {
    Ok(_) => SHError::default(),
    Err(error) => SHError {
      message: SHStringWithLen {
        string: error.as_ptr() as *const c_char,
        len: error.len(),
      },
      code: 1,
    },
  }
}

unsafe extern "C" fn shard2_exposedVariables<T: ShardDesc + Shard2 + Shard2Generated>(arg1: *mut CShard) -> SHExposedTypesInfo {
  let blk = arg1 as *mut ShardWrapper2<T>;
  if let Some(exposed) = (*blk).shard.exposed_variables() {
    SHExposedTypesInfo::from(exposed)
  } else {
    SHExposedTypesInfo::default()
  }
}

unsafe extern "C" fn shard2_compose<T: ShardDesc + Shard2 + Shard2Generated>(
  arg1: *mut CShard,
  data: *mut SHInstanceData,
) -> SHShardComposeResult {
  let blk = arg1 as *mut ShardWrapper2<T>;
  match (*blk).shard.compose(&*data) {
    Ok(output) => SHShardComposeResult {
      error: SHError::default(),
      result: output,
    },
    Err(error) => SHShardComposeResult {
      error: SHError {
        message: SHStringWithLen {
          string: error.as_ptr() as *const c_char,
          len: error.len(),
        },
        code: 1,
      },
      result: SHTypeInfo::default(),
    },
  }
}

unsafe extern "C" fn shard2_crossover<T: ShardDesc + Shard2 + Shard2Generated>(arg1: *mut CShard, s0: *const Var, s1: *const Var) {
  let blk = arg1 as *mut ShardWrapper2<T>;
  (*blk).shard.crossover(&*s0, &*s1);
}

unsafe extern "C" fn shard2_getState<T: ShardDesc + Shard2 + Shard2Generated>(arg1: *mut CShard) -> Var {
  let blk = arg1 as *mut ShardWrapper2<T>;
  (*blk).shard.get_state()
}

unsafe extern "C" fn shard2_setState<T: ShardDesc + Shard2 + Shard2Generated>(arg1: *mut CShard, state: *const Var) {
  let blk = arg1 as *mut ShardWrapper2<T>;
  (*blk).shard.set_state(&*state);
}

unsafe extern "C" fn shard2_resetState<T: ShardDesc + Shard2 + Shard2Generated>(arg1: *mut CShard) {
  let blk = arg1 as *mut ShardWrapper2<T>;
  (*blk).shard.reset_state();
}

pub fn create2<T: Default + ShardDesc + Shard2 + Shard2Generated>() -> ShardWrapper2<T> {
  let mut shard = ShardWrapper2::<T> {
    header: CShard {
      inlineShardId: 0,
      refCount: 0,
      owned: false,
      nameLength: 0,
      line: 0,
      column: 0,
      name: Some(shard2_name::<T>),
      hash: Some(shard2_hash::<T>),
      help: Some(shard2_help::<T>),
      inputHelp: Some(shard2_inputHelp::<T>),
      outputHelp: Some(shard2_outputHelp::<T>),
      properties: Some(shard2_properties::<T>),
      inputTypes: Some(shard2_inputTypes::<T>),
      outputTypes: Some(shard2_outputTypes::<T>),
      setup: Some(shard2_setup::<T>),
      destroy: Some(shard2_destroy::<T>),
      exposedVariables: Some(shard2_exposedVariables::<T>),
      requiredVariables: Some(shard2_requiredVariables::<T>),
      compose: if T::has_compose() {
        Some(shard2_compose::<T>)
      } else {
        None
      },
      parameters: Some(shard2_parameters::<T>),
      setParam: Some(shard2_setParam::<T>),
      getParam: Some(shard2_getParam::<T>),
      warmup: if T::has_warmup() {
        Some(shard2_warmup::<T>)
      } else {
        None
      },
      activate: Some(shard2_activate::<T>),
      cleanup: if T::has_cleanup() {
        Some(shard2_cleanup::<T>)
      } else {
        None
      },
      mutate: if T::has_mutate() {
        Some(shard2_mutate::<T>)
      } else {
        None
      },
      crossover: if T::has_crossover() {
        Some(shard2_crossover::<T>)
      } else {
        None
      },
      getState: if T::has_get_state() {
        Some(shard2_getState::<T>)
      } else {
        None
      },
      setState: if T::has_set_state() {
        Some(shard2_setState::<T>)
      } else {
        None
      },
      resetState: if T::has_reset_state() {
        Some(shard2_resetState::<T>)
      } else {
        None
      },
    },
    shard: T::default(),
    name: None,
    help: None,
    error: None,
  };
  shard.header.nameLength = shard.shard.name().len() as u32;
  return shard;
}

/// Declares a function as an override to [`Shard::activate`].
///
/// This macro is meant to be invoked from [`Shard::compose`].
///
/// # Examples
///
/// ```ignore (only-for-syntax-highlight)
/// impl Shard for MyShard {
///   fn compose(&mut self, data: &InstanceData) -> Result<Type, &str> {
///     decl_override_activate! {
///       data.activate = MyShard::my_activate_override;
///     }
///   }
/// }
/// ```
///
/// See also: [`impl_override_activate!`]
#[macro_export]
macro_rules! decl_override_activate {
  (
    $data:ident.activate = $shard_name:ident::$override_name:ident;
  ) => {
    unsafe {
      (*$data.shard).activate = Some($shard_name::$override_name);
    }
  };
}

/// Implements an override of [`Shard::activate`] that can be called from C/C++.
///
/// This macro is meant to be invoked from the custom implementation of a [`Shard`].
///
/// # Examples
///
/// ```ignore (only-for-syntax-highlight)
/// impl MyShard {
///   fn activateOverride() {
///     todo!()
///   }
///
///   impl_override_activate! {
///     extern "C" fn my_activate_override() -> Var {
///       MyShard::activateOverride()
///     }
///   }
/// }
/// ```
///
/// See also: [`decl_override_activate!`]
#[macro_export]
macro_rules! impl_override_activate {
  (
    $(#[$meta:meta])*
    extern "C" fn $override_name:ident() -> Var {
      $shard_name:ident::$override_impl:ident()
    }
  ) => {
    $(#[$meta])*
    unsafe extern "C" fn $override_name(
      arg1: *mut $crate::shardsc::Shard,
      arg2: *mut Context,
      arg3: *const Var,
    ) -> Var {
      let blk = arg1 as *mut $crate::shard::ShardWrapper<$shard_name>;
      match (*blk).shard.$override_impl(&(*arg2), &(*arg3)) {
        Ok(value) => value,
        Err(error) => {
          $crate::core::abortWire(&(*arg2), error);
          Var::default()
        }
      }
    }
  };
}
