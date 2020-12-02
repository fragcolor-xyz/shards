#![macro_use]

use crate::block::cblock_construct;
use crate::block::Block;
use crate::chainblocksc::CBBool;
use crate::chainblocksc::CBChainState;
use crate::chainblocksc::CBContext;
use crate::chainblocksc::CBCore;
use crate::chainblocksc::CBString;
use crate::chainblocksc::CBStrings;
use crate::chainblocksc::CBVar;
use crate::chainblocksc::CBlockPtr;
use crate::types::ChainState;
use crate::types::Context;
use crate::types::ParameterInfoView;
use crate::types::Var;
use core::convert::TryInto;
use core::ffi::c_void;
use core::slice;
use std::ffi::CStr;
use std::ffi::CString;
use std::os::raw::c_char;

const ABI_VERSION: u32 = 0x20200101;

pub static mut Core: *mut CBCore = core::ptr::null_mut();
static mut init_done: bool = false;

#[cfg(feature = "dllblock")]
mod internal_core_init {
  extern crate dlopen;
  use crate::chainblocksc::chainblocksInterface;
  use crate::core::CBCore;
  use crate::core::ABI_VERSION;
  use crate::Core;
  use dlopen::symbor::Library;

  fn try_load_dlls() -> Option<Library> {
    if let Ok(lib) = Library::open("libcb.dylib") {
      Some(lib)
    } else if let Ok(lib) = Library::open("libcb_shared.dylib") {
      Some(lib)
    } else if let Ok(lib) = Library::open("libcb.so") {
      Some(lib)
    } else if let Ok(lib) = Library::open("libcb_shared.so") {
      Some(lib)
    } else if let Ok(lib) = Library::open("libcb.dll") {
      Some(lib)
    } else {
      None
    }
  }

  pub static mut CBDLL: Option<Library> = None;

  pub unsafe fn initInternal() {
    let exe = Library::open_self().ok().unwrap();

    let exefun = exe
      .symbol::<unsafe extern "C" fn(abi_version: u32) -> *mut CBCore>("chainblocksInterface")
      .ok();
    if let Some(fun) = exefun {
      Core = fun(ABI_VERSION);
      if Core.is_null() {
        panic!("Failed to aquire chainblocks interface, version not compatible.");
      }
    } else {
      let lib = try_load_dlls().unwrap();
      let fun = lib
        .symbol::<unsafe extern "C" fn(abi_version: u32) -> *mut CBCore>("chainblocksInterface")
        .unwrap();
      Core = fun(ABI_VERSION);
      if Core.is_null() {
        panic!("Failed to aquire chainblocks interface, version not compatible.");
      }
      CBDLL = Some(lib);
    }
  }
}

#[cfg(not(feature = "dllblock"))]
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
    (*Core).log.unwrap()(s.as_ptr() as *const i8);
  }
}

#[macro_export]
macro_rules! cblog {
    ($text:expr, $($arg:expr),*) => {
        use std::io::Write as __stdWrite;
        let mut buf = vec![];
        ::std::write!(&mut buf, concat!($text, "\0"), $($arg),*).unwrap();
        log(::std::str::from_utf8(&buf).unwrap());
    };

    ($text:expr) => {
        log(concat!($text, "\0"));
    };
}

#[inline(always)]
pub fn sleep(seconds: f64) {
  unsafe {
    (*Core).sleep.unwrap()(seconds, true);
  }
}

#[inline(always)]
pub fn suspend(context: &CBContext, seconds: f64) -> ChainState {
  unsafe {
    let ctx = context as *const CBContext as *mut CBContext;
    (*Core).suspend.unwrap()(ctx, seconds).into()
  }
}

#[inline(always)]
pub fn getState(context: &CBContext) -> ChainState {
  unsafe {
    let ctx = context as *const CBContext as *mut CBContext;
    (*Core).getState.unwrap()(ctx).into()
  }
}

#[inline(always)]
pub fn abortChain(context: &CBContext, message: &str) {
  let cmsg = CString::new(message).unwrap();
  unsafe {
    let ctx = context as *const CBContext as *mut CBContext;
    (*Core).abortChain.unwrap()(ctx, cmsg.as_ptr());
  }
}

#[inline(always)]
pub fn registerBlock<T: Default + Block>() {
  unsafe {
    (*Core).registerBlock.unwrap()(
      T::registerName().as_ptr() as *const c_char,
      Some(cblock_construct::<T>),
    );
  }
}

pub fn getBlocks() -> Vec<&'static CStr> {
  unsafe {
    let block_names = (*Core).getBlocks.unwrap()();
    let mut res = Vec::new();
    let len = block_names.len;
    let slice = slice::from_raw_parts(block_names.elements, len.try_into().unwrap());
    for name in slice.iter() {
      res.push(CStr::from_ptr(*name));
    }
    (*Core).stringsFree.unwrap()(&block_names as *const CBStrings as *mut CBStrings);
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
pub fn createBlockPtr(name: &str) -> CBlockPtr {
  let cname = CString::new(name).unwrap();
  unsafe { (*Core).createBlock.unwrap()(cname.as_ptr()) }
}

#[inline(always)]
pub fn createBlock(name: &str) -> BlockInstance {
  let cname = CString::new(name).unwrap();
  unsafe {
    BlockInstance {
      ptr: (*Core).createBlock.unwrap()(cname.as_ptr()),
    }
  }
}

#[inline(always)]
pub fn cloneVar(dst: &mut Var, src: &Var) {
  unsafe {
    (*Core).cloneVar.unwrap()(dst, src);
  }
}

pub fn referenceMutVariable(context: &CBContext, name: CBString) -> &mut CBVar {
  unsafe {
    let ctx = context as *const CBContext as *mut CBContext;
    let cbptr = (*Core).referenceVariable.unwrap()(ctx, name);
    cbptr.as_mut().unwrap()
  }
}

pub fn referenceVariable(context: &CBContext, name: CBString) -> &CBVar {
  unsafe {
    let ctx = context as *const CBContext as *mut CBContext;
    let cbptr = (*Core).referenceVariable.unwrap()(ctx, name);
    cbptr.as_mut().unwrap()
  }
}

pub fn releaseMutVariable(var: &mut CBVar) {
  unsafe {
    let v = var as *mut CBVar;
    (*Core).releaseVariable.unwrap()(v);
  }
}

pub fn releaseVariable(var: &CBVar) {
  unsafe {
    let v = var as *const CBVar as *mut CBVar;
    (*Core).releaseVariable.unwrap()(v);
  }
}

unsafe extern "C" fn do_blocking_c_call<'a>(context: *mut CBContext, arg2: *mut c_void) -> CBVar {
  let trait_obj_ref: &mut &mut dyn FnMut() -> Result<CBVar, &'a str> = { &mut *(arg2 as *mut _) };
  match trait_obj_ref() {
    Ok(value) => value,
    Err(error) => {
      abortChain(&(*context), error);
      Var::default()
    }
  }
}

pub fn do_blocking<'a, F>(context: &CBContext, f: F) -> CBVar
where
  F: FnMut() -> Result<CBVar, &'a str>,
{
  unsafe {
    let ctx = context as *const CBContext as *mut CBContext;
    let mut trait_obj: &dyn FnMut() -> Result<CBVar, &'a str> = &f;
    let trait_obj_ref = &mut trait_obj;
    let closure_pointer_pointer = trait_obj_ref as *mut _ as *mut c_void;
    (*Core).asyncActivate.unwrap()(ctx, closure_pointer_pointer, Some(do_blocking_c_call))
  }
}

pub trait BlockingBlock {
  fn activate_blocking(&mut self, context: &Context, input: &Var) -> Result<Var, &str>;
}

struct AsyncCallData<T: BlockingBlock> {
  caller: *mut T,
  input: *const CBVar,
}

unsafe extern "C" fn activate_blocking_c_call<T: BlockingBlock>(
  context: *mut CBContext,
  arg2: *mut c_void,
) -> CBVar {
  let data = arg2 as *mut AsyncCallData<T>;
  let res = (*(*data).caller).activate_blocking(&*context, &*(*data).input);
  match res {
    Ok(value) => value,
    Err(error) => {
      abortChain(&(*context), error);
      Var::default()
    }
  }
}

pub fn activate_blocking<'a, T>(
  caller: &'a mut T,
  context: &'a CBContext,
  input: &'a CBVar,
) -> CBVar
where
  T: BlockingBlock,
{
  unsafe {
    let data = AsyncCallData {
      caller: caller as *mut T,
      input: input as *const CBVar,
    };
    let ctx = context as *const CBContext as *mut CBContext;
    let data_ptr = &data as *const AsyncCallData<T> as *mut AsyncCallData<T> as *mut c_void;
    (*Core).asyncActivate.unwrap()(ctx, data_ptr, Some(activate_blocking_c_call::<T>))
  }
}

pub struct BlockInstance {
  ptr: CBlockPtr,
}

impl Drop for BlockInstance {
  fn drop(&mut self) {
    unsafe {
      if !(*self.ptr).owned {
        (*self.ptr).destroy.unwrap()(self.ptr);
      }
    }
  }
}

impl BlockInstance {
  pub fn parameters(self) -> Vec<ParameterInfoView> {
    let mut res = Vec::new();
    unsafe {
      let params = (*self.ptr).parameters.unwrap()(self.ptr);
      let slice = slice::from_raw_parts(params.elements, params.len.try_into().unwrap());
      for info in slice.iter() {
        res.push(ParameterInfoView(*info));
      }
    }
    res
  }
}
