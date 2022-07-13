/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2020 Fragcolor Pte. Ltd. */

#![macro_use]

use crate::shard::shard_construct;
use crate::shard::Shard;
use crate::shardsc::SHBool;
use crate::shardsc::SHContext;
use crate::shardsc::SHCore;
use crate::shardsc::SHOptionalString;
use crate::shardsc::SHString;
use crate::shardsc::SHStrings;
use crate::shardsc::SHVar;
use crate::shardsc::SHWireState;
use crate::shardsc::ShardPtr;
use crate::types::ClonedVar;
use crate::types::Context;
use crate::types::DerivedType;
use crate::types::ExternalVar;
use crate::types::InstanceData;
use crate::types::Mesh;
use crate::types::ParameterInfo;
use crate::types::Parameters;
use crate::types::Var;
use crate::types::WireRef;
use crate::types::WireState;
use core::convert::TryInto;
use core::ffi::c_void;
use core::slice;
use std::ffi::CStr;
use std::ffi::CString;
use std::os::raw::c_char;

const ABI_VERSION: u32 = 0x20200101;

pub static mut Core: *mut SHCore = core::ptr::null_mut();
pub static mut ScriptEnvCreate: Option<
  unsafe extern "C" fn(path: *const ::std::os::raw::c_char) -> *mut ::core::ffi::c_void,
> = None;
pub static mut ScriptEnvCreateSub: Option<
  unsafe extern "C" fn(parent_env: *mut ::core::ffi::c_void) -> *mut ::core::ffi::c_void,
> = None;
pub static mut ScriptEnvDestroy: Option<unsafe extern "C" fn(env: *mut ::core::ffi::c_void)> = None;
pub static mut ScriptEval: Option<
  unsafe extern "C" fn(
    env: *mut ::core::ffi::c_void,
    script: *const ::std::os::raw::c_char,
    output: *mut SHVar,
  ) -> bool,
> = None;
static mut init_done: bool = false;

#[cfg(feature = "dllshard")]
mod internal_core_init {
  extern crate dlopen;
  use super::*;
  use crate::core::SHCore;
  use crate::core::ABI_VERSION;
  use crate::shardsc::shardsInterface;
  use dlopen::symbor::Library;

  fn try_load_dlls() -> Option<Library> {
    if let Ok(lib) = Library::open("libshards.dylib") {
      Some(lib)
    } else if let Ok(lib) = Library::open("libshards.so") {
      Some(lib)
    } else if let Ok(lib) = Library::open("libshards.dll") {
      Some(lib)
    } else {
      None
    }
  }

  pub static mut SHDLL: Option<Library> = None;

  pub unsafe fn initScripting(lib: &Library) {
    let fun = lib.symbol::<unsafe extern "C" fn(
      path: *const ::std::os::raw::c_char,
    ) -> *mut ::core::ffi::c_void>("shLispCreate");
    if let Ok(fun) = fun {
      ScriptEnvCreate = Some(*fun);
    } else {
      // short circuit here
      return;
    }

    let fun = lib.symbol::<unsafe extern "C" fn(env: *mut ::core::ffi::c_void)>("shLispDestroy");
    ScriptEnvDestroy = Some(*fun.unwrap());

    let fun = lib
      .symbol::<unsafe extern "C" fn(env: *mut ::core::ffi::c_void) -> *mut ::core::ffi::c_void>(
        "shLispCreateSub",
      );
    ScriptEnvCreateSub = Some(*fun.unwrap());

    let fun = lib.symbol::<unsafe extern "C" fn(
      env: *mut ::core::ffi::c_void,
      script: *const ::std::os::raw::c_char,
      output: *mut SHVar,
    ) -> bool>("shLispEval");
    ScriptEval = Some(*fun.unwrap());

    // trigger initializations... fix me in the future to something more elegant
    let current_dir = std::env::current_dir().unwrap();
    let current_dir = current_dir.to_str().unwrap();
    let current_dir = std::ffi::CString::new(current_dir).unwrap();
    let env = ScriptEnvCreate.unwrap()(current_dir.as_ptr());
    ScriptEnvDestroy.unwrap()(env);
  }

  pub unsafe fn initInternal() {
    let exe = Library::open_self().ok().unwrap();

    let exefun = exe
      .symbol::<unsafe extern "C" fn(abi_version: u32) -> *mut SHCore>("shardsInterface")
      .ok();
    if let Some(fun) = exefun {
      // init scripting first if possible!
      initScripting(&exe);
      Core = fun(ABI_VERSION);
      if Core.is_null() {
        panic!("Failed to aquire shards interface, version not compatible.");
      }
    } else {
      let lib = try_load_dlls().unwrap();
      let fun = lib
        .symbol::<unsafe extern "C" fn(abi_version: u32) -> *mut SHCore>("shardsInterface")
        .unwrap();
      // init scripting first if possible!
      initScripting(&lib);
      Core = fun(ABI_VERSION);
      if Core.is_null() {
        panic!("Failed to aquire shards interface, version not compatible.");
      }
      SHDLL = Some(lib);
    }
  }
}

#[cfg(not(feature = "dllshard"))]
mod internal_core_init {
  pub unsafe fn initInternal() {}
}

#[inline(always)]
pub fn init() {
  unsafe {
    if !init_done {
      internal_core_init::initInternal();
      init_done = true;
    }
  }
}

#[inline(always)]
pub fn log(s: &str) {
  unsafe {
    (*Core).log.unwrap()(s.as_ptr() as *const std::os::raw::c_char);
  }
}

#[inline(always)]
pub fn logLevel(level: i32, s: &str) {
  unsafe {
    (*Core).logLevel.unwrap()(level, s.as_ptr() as *const std::os::raw::c_char);
  }
}

#[macro_export]
#[cfg(debug_assertions)]
macro_rules! shlog_debug {
  ($text:expr, $($arg:expr),*) => {
      use std::io::Write as __stdWrite;
      let mut buf = vec![];
      ::std::write!(&mut buf, concat!($text, "\0"), $($arg),*).unwrap();
      $crate::core::logLevel(1, ::std::str::from_utf8(&buf).unwrap());
  };

  ($text:expr) => {
    $crate::core::logLevel(1, concat!($text, "\0"));
  };
}

#[macro_export]
#[cfg(not(debug_assertions))]
macro_rules! shlog_debug {
  ($text:expr, $($arg:expr),*) => {};

  ($text:expr) => {};
}

#[macro_export]
macro_rules! shlog {
    ($text:expr, $($arg:expr),*) => {
        use std::io::Write as __stdWrite;
        let mut buf = vec![];
        ::std::write!(&mut buf, concat!($text, "\0"), $($arg),*).unwrap();
        $crate::core::log(::std::str::from_utf8(&buf).unwrap());
    };

    ($text:expr) => {
      $crate::core::log(concat!($text, "\0"));
    };
}

#[inline(always)]
pub fn sleep(seconds: f64) {
  unsafe {
    (*Core).sleep.unwrap()(seconds, true);
  }
}

#[inline(always)]
pub fn suspend(context: &SHContext, seconds: f64) -> WireState {
  unsafe {
    let ctx = context as *const SHContext as *mut SHContext;
    (*Core).suspend.unwrap()(ctx, seconds).into()
  }
}

#[inline(always)]
pub fn getState(context: &SHContext) -> WireState {
  unsafe {
    let ctx = context as *const SHContext as *mut SHContext;
    (*Core).getState.unwrap()(ctx).into()
  }
}

#[inline(always)]
pub fn abortWire(context: &SHContext, message: &str) {
  let cmsg = CString::new(message).unwrap();
  unsafe {
    let ctx = context as *const SHContext as *mut SHContext;
    (*Core).abortWire.unwrap()(ctx, cmsg.as_ptr());
  }
}

#[inline(always)]
pub fn registerShard<T: Default + Shard>() {
  unsafe {
    (*Core).registerShard.unwrap()(
      T::registerName().as_ptr() as *const c_char,
      Some(shard_construct::<T>),
    );
  }
}

pub fn getShards() -> Vec<&'static CStr> {
  unsafe {
    let shard_names = (*Core).getShards.unwrap()();
    let mut res = Vec::new();
    let len = shard_names.len;
    let slice = slice::from_raw_parts(shard_names.elements, len.try_into().unwrap());
    for name in slice.iter() {
      res.push(CStr::from_ptr(*name));
    }
    (*Core).stringsFree.unwrap()(&shard_names as *const SHStrings as *mut SHStrings);
    res
  }
}

#[inline(always)]
pub fn getRootPath() -> &'static str {
  unsafe {
    CStr::from_ptr((*Core).getRootPath.unwrap()())
      .to_str()
      .unwrap()
  }
}

#[inline(always)]
pub fn createShardPtr(name: &str) -> ShardPtr {
  let cname = CString::new(name).unwrap();
  unsafe { (*Core).createShard.unwrap()(cname.as_ptr()) }
}

#[inline(always)]
pub fn createShard(name: &str) -> ShardInstance {
  let cname = CString::new(name).unwrap();
  unsafe {
    ShardInstance {
      ptr: (*Core).createShard.unwrap()(cname.as_ptr()),
    }
  }
}

#[inline(always)]
pub fn cloneVar(dst: &mut Var, src: &Var) {
  unsafe {
    (*Core).cloneVar.unwrap()(dst, src);
  }
}

#[inline(always)]
pub fn destroyVar(v: &mut Var) {
  unsafe {
    (*Core).destroyVar.unwrap()(v);
  }
}

pub fn readCachedString(id: u32) -> &'static str {
  unsafe {
    let s = (*Core).readCachedString.unwrap()(id);
    CStr::from_ptr(s.string).to_str().unwrap()
  }
}

pub fn writeCachedString(id: u32, string: &'static str) -> &'static str {
  unsafe {
    let s = (*Core).writeCachedString.unwrap()(id, string.as_ptr() as *const std::os::raw::c_char);
    CStr::from_ptr(s.string).to_str().unwrap()
  }
}

pub fn readCachedString1(id: u32) -> SHOptionalString {
  unsafe { (*Core).readCachedString.unwrap()(id) }
}

pub fn writeCachedString1(id: u32, string: &'static str) -> SHOptionalString {
  unsafe { (*Core).writeCachedString.unwrap()(id, string.as_ptr() as *const std::os::raw::c_char) }
}

pub fn deriveType(var: &Var, data: &InstanceData) -> DerivedType {
  let t = unsafe { (*Core).deriveTypeInfo.unwrap()(var as *const _, data as *const _) };
  DerivedType(t)
}

macro_rules! shccstr {
  ($string:literal) => {
    if cfg!(debug_assertions) {
      crate::core::writeCachedString1(compile_time_crc32::crc32!($string), cstr!($string))
    } else {
      crate::core::readCachedString1(compile_time_crc32::crc32!($string))
    }
  };
}

pub fn referenceMutVariable(context: &SHContext, name: SHString) -> &mut SHVar {
  unsafe {
    let ctx = context as *const SHContext as *mut SHContext;
    let shptr = (*Core).referenceVariable.unwrap()(ctx, name);
    shptr.as_mut().unwrap()
  }
}

pub fn referenceVariable(context: &SHContext, name: SHString) -> &SHVar {
  unsafe {
    let ctx = context as *const SHContext as *mut SHContext;
    let shptr = (*Core).referenceVariable.unwrap()(ctx, name);
    shptr.as_mut().unwrap()
  }
}

pub fn releaseMutVariable(var: &mut SHVar) {
  unsafe {
    let v = var as *mut SHVar;
    (*Core).releaseVariable.unwrap()(v);
  }
}

pub fn releaseVariable(var: &SHVar) {
  unsafe {
    let v = var as *const SHVar as *mut SHVar;
    (*Core).releaseVariable.unwrap()(v);
  }
}

impl WireRef {
  pub fn set_external(&self, name: &str, var: &mut ExternalVar) {
    let cname = CString::new(name).unwrap();
    unsafe {
      (*Core).setExternalVariable.unwrap()(
        self.0,
        cname.as_ptr() as *const _,
        &var.0 as *const _ as *mut _,
      );
    }
  }

  pub fn remove_external(&self, name: &str) {
    let cname = CString::new(name).unwrap();
    unsafe {
      (*Core).removeExternalVariable.unwrap()(self.0, cname.as_ptr() as *const _);
    }
  }

  pub fn get_result(&self) -> Result<Option<ClonedVar>, &str> {
    let info = unsafe { (*Core).getWireInfo.unwrap()(self.0) };

    if !info.isRunning {
      if info.failed {
        let msg = unsafe { CStr::from_ptr(info.failureMessage) };
        Err(msg.to_str().unwrap())
      } else {
        unsafe { Ok(Some((*info.finalOutput).into())) }
      }
    } else {
      Ok(None)
    }
  }
}

//--------------------------------------------------------------------------------------------------

pub trait BlockingShard {
  fn activate_blocking(&mut self, context: &Context, input: &Var) -> Result<Var, &str>;
  fn cancel_activation(&mut self, _context: &Context) {}
}

struct AsyncCallData<T: BlockingShard> {
  caller: *mut T,
  input: *const SHVar,
}

unsafe extern "C" fn activate_blocking_c_call<T: BlockingShard>(
  context: *mut SHContext,
  arg2: *mut c_void,
) -> SHVar {
  let data = arg2 as *mut AsyncCallData<T>;
  let res = (*(*data).caller).activate_blocking(&*context, &*(*data).input);
  match res {
    Ok(value) => value,
    Err(error) => {
      shlog_debug!("activate_blocking failure detected"); // in case error leads to crash
      shlog_debug!("activate_blocking failure: {}", error);
      abortWire(&(*context), error);
      Var::default()
    }
  }
}

unsafe extern "C" fn cancel_blocking_c_call<T: BlockingShard>(
  context: *mut SHContext,
  arg2: *mut c_void,
) {
  let data = arg2 as *mut AsyncCallData<T>;
  (*(*data).caller).cancel_activation(&*context);
}

pub fn run_blocking<'a, T>(
  caller: &'a mut T,
  context: &'a SHContext,
  input: &'a SHVar,
) -> SHVar
where
  T: BlockingShard,
{
  unsafe {
    let data = AsyncCallData {
      caller: caller as *mut T,
      input: input as *const SHVar,
    };
    let ctx = context as *const SHContext as *mut SHContext;
    let data_ptr = &data as *const AsyncCallData<T> as *mut AsyncCallData<T> as *mut c_void;
    (*Core).asyncActivate.unwrap()(
      ctx,
      data_ptr,
      Some(activate_blocking_c_call::<T>),
      Some(cancel_blocking_c_call::<T>),
    )
  }
}

//--------------------------------------------------------------------------------------------------

pub struct ShardInstance {
  ptr: ShardPtr,
}

impl Drop for ShardInstance {
  fn drop(&mut self) {
    unsafe {
      if !(*self.ptr).owned {
        (*self.ptr).destroy.unwrap()(self.ptr);
      }
    }
  }
}
