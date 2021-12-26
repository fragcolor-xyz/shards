/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2020 Fragcolor Pte. Ltd. */

#![allow(non_upper_case_globals)]
#![allow(non_camel_case_types)]
#![allow(non_snake_case)]
#![allow(unused_imports)]
#![allow(unused_macros)]
#![allow(dead_code)]
#![allow(improper_ctypes)]
#![allow(improper_ctypes_definitions)]

#[macro_use]
#[cfg(test)]
extern crate ctor;

#[macro_use]
extern crate approx;

#[macro_use]
extern crate lazy_static;

#[macro_use]
extern crate compile_time_crc32;

extern crate serde;
extern crate serde_json;

pub mod block;
mod chainblocksc;
pub mod core;
#[macro_use]
pub mod types;
// order matters
#[cfg(feature = "blocks")]
pub mod blocks;

use crate::block::Block;
pub use crate::chainblocksc::*;
use crate::core::log;
use crate::core::Core;
use crate::types::Types;
use crate::types::Var;
use std::convert::TryInto;
use std::ffi::CString;

// use this to develop/debug:
// cargo +nightly rustc --profile=check -- -Zunstable-options --pretty=expanded

macro_rules! var {
    ((-> $($param:tt) *)) => {{
        let blks = blocks!($($param) *);
        let mut vblks = Vec::<$crate::types::Var>::new();
        for blk in blks {
            vblks.push($crate::types::Var::from(blk));
        }
        // this is sad, we do a double copy cos set param will copy too
        // but for now it's the easiest
        $crate::types::ClonedVar::from($crate::types::Var::from(&vblks))
    }};

    ($vexp:expr) => { $crate::types::WrappedVar($crate::types::Var::from($vexp)) }
}

macro_rules! blocks {
    (@block Set :Name .$var:ident) => { blocks!(@block Set (stringify!($var))) };
    (@block Set .$var:ident) => { blocks!(@block Set (stringify!($var))) };
    (@block Set :Name .$var:ident) => { blocks!(@block Set (stringify!($var))) };
    (@block Set .$var:ident) => { blocks!(@block Set (stringify!($var))) };

    // (BlockName)
    (@block $block:ident) => {{
        let blk = $crate::core::createBlockPtr(stringify!($block));
        unsafe {
            (*blk).setup.unwrap()(blk);
        }
            blk
    }};

    // (BlockName.BlockName :ParamName ParamVar ...)
    (@block $block0:ident.$block1:ident $(:$pname:tt $param:tt) *) => {{
      let blkname = concat!(stringify!($block0), ".", stringify!($block1));
      let blk = $crate::core::createBlockPtr(blkname);
      unsafe {
          (*blk).setup.unwrap()(blk);
      }
      let cparams: $crate::chainblocksc::CBParametersInfo;
      unsafe {
          cparams = (*blk).parameters.unwrap()(blk);
      }
      let params: &[$crate::chainblocksc::CBParameterInfo] = cparams.into();
      $(
          {
              let param = stringify!($pname);
              let mut piter = params.iter();
              let idx = piter.position(|&x| -> bool {
                  unsafe {
                      let cname = x.name as *const std::os::raw::c_char as *mut std::os::raw::c_char;
                      let cstr =  std::ffi::CString::from_raw(cname);
                      let s = cstr.to_str();
                      s.is_err() || s.unwrap() == param
                  }
              });
              if let Some(pidx) = idx {
                  let pvar = var!($param);
                  unsafe {
                      (*blk).setParam.unwrap()(blk, pidx as i32, &pvar.0 as * const _);
                  }
              } else {
                  panic!("Parameter not found: {} for block: {}!", param, blkname);
              }
          }
      ) *
          blk
   }};

    // (BlockName :ParamName ParamVar ...)
    (@block $block:ident $(:$pname:tt $param:tt) *) => {{
        let blkname = stringify!($block);
        let blk = $crate::core::createBlockPtr(blkname);
        unsafe {
            (*blk).setup.unwrap()(blk);
        }
        let cparams: $crate::chainblocksc::CBParametersInfo;
        unsafe {
            cparams = (*blk).parameters.unwrap()(blk);
        }
        let params: &[$crate::chainblocksc::CBParameterInfo] = cparams.into();
        $(
            {
                let param = stringify!($pname);
                let mut piter = params.iter();
                let idx = piter.position(|&x| -> bool {
                    unsafe {
                        let cname = x.name as *const std::os::raw::c_char as *mut std::os::raw::c_char;
                        let cstr =  std::ffi::CString::from_raw(cname);
                        let s = cstr.to_str();
                        s.is_err() || s.unwrap() == param
                    }
                });
                if let Some(pidx) = idx {
                    let pvar = var!($param);
                    unsafe {
                        (*blk).setParam.unwrap()(blk, pidx as i32, &pvar.0 as * const _);
                    }
                } else {
                    panic!("Parameter not found: {} for block: {}!", param, blkname);
                }
            }
        ) *
            blk
     }};

     // (BlockName.BlockName ParamVar ...)
    (@block $block0:ident.$block1:ident $($param:tt) *) => {{
      let blk = $crate::core::createBlockPtr(concat!(stringify!($block0), ".", stringify!($block1)));
      unsafe {
          (*blk).setup.unwrap()(blk);
      }
      let mut _pidx: i32 = 0;
      $(
          {
              let pvar = var!($param);
              unsafe {
                  (*blk).setParam.unwrap()(blk, _pidx, &pvar.0 as *const _);
              }
              _pidx += 1;
          }
      ) *
          blk
    }};

    // (BlockName ParamVar ...)
    (@block $block:ident $($param:tt) *) => {{
        let blk = $crate::core::createBlockPtr(stringify!($block));
        unsafe {
            (*blk).setup.unwrap()(blk);
        }
        let mut _pidx: i32 = 0;
        $(
            {
                let pvar = var!($param);
                unsafe {
                    (*blk).setParam.unwrap()(blk, _pidx, &pvar.0 as *const _);
                }
                _pidx += 1;
            }
        ) *
            blk
    }};

    /*
      I wanted to make just regular literals work but I could not
      e.g.: 10 (Log)
      but in this macro (. 10) (Log) has to be used.
      I will implement a (.) builtin maybe in mal to allow copy pasting of such scripts.. but not sure we are going to use either.. again WIP but don't wanna loose it in history.
    */
    (@block . $a:expr) => { blocks!(@block Const $a) };

    // this is the CORE evaluator takes a list of blocks expressions
    // the current limit is that everything has to be between parenthesis (...)
    ($(($block:tt $($param:tt) *)) *) => {{
        let mut blks = Vec::<$crate::chainblocksc::CBlockPtr>::new();
        $(
            blks.push(blocks!(@block $block $($param) *));
        ) *
            blks
    }};
}

// --features "dummy"
#[cfg(any(all(test, not(feature = "cb_static")), target_arch = "wasm32"))]
#[macro_use]
mod dummy_block {
  // run with: RUST_BACKTRACE=1 cargo test -- --nocapture

  use super::block::create;
  use super::Types;
  use crate::block::cblock_construct;
  use crate::block::Block;
  use crate::block::BlockWrapper;
  use crate::cblog;
  use crate::chainblocksc::CBContext;
  use crate::chainblocksc::CBTypeInfo;
  use crate::chainblocksc::CBType_Int;
  use crate::chainblocksc::CBTypesInfo;
  use crate::chainblocksc::CBVar;
  use crate::chainblocksc::CBlock;
  use crate::core::cloneVar;
  use crate::core::createBlock;
  use crate::core::init;
  use crate::core::log;
  use crate::core::registerBlock;
  use crate::core::sleep;
  use crate::core::suspend;
  use crate::core::Core;
  use crate::types::common_type;
  use crate::types::ClonedVar;
  use crate::types::Table;
  use crate::types::Var;
  use core::convert::TryInto;
  use std::ffi::CStr;
  use std::ffi::CString;

  lazy_static! {
    static ref PROPERTIES: Table = {
      let mut table = Table::default();
      table.insert_fast_static(cstr!("experimental"), true.into());
      table
    };
  }

  pub struct DummyBlock {
    inputTypes: Types,
    outputTypes: Types,
  }

  impl Default for DummyBlock {
    fn default() -> Self {
      DummyBlock {
        inputTypes: vec![common_type::none],
        outputTypes: vec![common_type::any],
      }
    }
  }

  impl Block for DummyBlock {
    fn name(&mut self) -> &str {
      "Dummy"
    }

    fn inputTypes(&mut self) -> &Types {
      &self.inputTypes
    }

    fn outputTypes(&mut self) -> &Types {
      &self.outputTypes
    }

    fn properties(&mut self) -> Option<&Table> {
      Some(&PROPERTIES)
    }

    fn activate(&mut self, context: &CBContext, _input: &Var) -> Result<Var, &str> {
      log("Dummy - activate: Ok!\0");
      let mut x: String = "Before...\0".to_string();
      log(&x);
      suspend(context, 2.0);
      x.push_str(" - and After!\0");
      log(&x);
      log("Dummy - activate: Resumed!\0");
      Ok(Var::default())
    }

    fn registerName() -> &'static str {
      "Dummy\0"
    }

    fn hash() -> u32 {
      compile_time_crc32::crc32!("Dummy-rust-0x20200101")
    }
  }

  #[cfg(test)]
  #[ctor]
  fn registerDummy() {
    init();
    registerBlock::<DummyBlock>();
  }

  fn macroTest() {
    let rust_variable = "Hello World!";
    let rust_variable2 = 2;
    blocks!(
      (. 10)
      (Math.Multiply rust_variable2)
      (Math.Subtract :Operand 1)
      (Set .x)
      (Log)
      (ToString)
      (Set :Name .x)
      (Repeat
        (->
        (Msg "repeating...")
        (Log)
        (. rust_variable) (Log)))
      (Msg :Message "Done"));

    blocks!(
      (. "Hello")
      (Msg)
    );
  }

  #[test]
  fn instanciate() {
    init();

    let mut blk = create::<DummyBlock>();
    assert_eq!("Dummy".to_string(), blk.block.name());

    let blkname = CString::new("Dummy").expect("CString failed...");
    unsafe {
      let cblk = (*Core).createBlock.unwrap()(blkname.as_ptr());
      (*cblk).setup.unwrap()(cblk);
      (*cblk).destroy.unwrap()(cblk);
    }

    let svar1: Var = "Hello\0".into();
    let svar2: Var = "Hello\0".into();
    let svar3: Var = 10.into();
    let sstr1: &str = svar1.as_ref().try_into().unwrap();
    assert_eq!("Hello", sstr1);
    assert!(svar1 == svar2);
    assert!(svar1 != svar3);

    let a = Var::from(10);
    let mut b = Var::from(true);
    cloneVar(&mut b, &a);
    unsafe {
      assert_eq!(a.valueType, CBType_Int);
      assert_eq!(b.valueType, CBType_Int);
      assert_eq!(a.payload.__bindgen_anon_1.intValue, 10);
      assert_eq!(
        a.payload.__bindgen_anon_1.intValue,
        b.payload.__bindgen_anon_1.intValue
      );
    }

    let _v: ClonedVar = a.into();

    cblog!("Hello chainblocks-rs");
  }
}

#[cfg(feature = "cblisp")]
pub mod cblisp {
  use crate::Var;
  use std::convert::TryInto;

  pub struct ScriptEnv(pub *mut ::core::ffi::c_void);

  impl Drop for ScriptEnv {
    fn drop(&mut self) {
      unsafe {
        crate::core::ScriptEnvDestroy.unwrap()(self.0);
      }
    }
  }

  #[macro_export]
  macro_rules! cbl_no_env {
    ($code:expr) => {
      unsafe {
        let code = std::ffi::CString::new($code).expect("CString failed...");
        $crate::core::cbLispEval(::core::ptr::null_mut(), code.as_ptr())
      }
    };
  }

  #[macro_export]
  macro_rules! cbl_env {
    ($code:expr) => {{
      thread_local! {
        static ENV: $crate::cblisp::ScriptEnv = {
          let current_dir = std::env::current_dir().unwrap();
          let current_dir = current_dir.to_str().unwrap();
          unsafe { $crate::cblisp::ScriptEnv($crate::core::ScriptEnvCreate.unwrap()(std::ffi::CString::new(current_dir).unwrap().as_ptr())) }
        }
      }
      unsafe {
        let code = std::ffi::CString::new($code).expect("CString failed...");
        ENV.with(|env| $crate::core::ScriptEval.unwrap()((*env).0, code.as_ptr()))
      }
    }};
  }

  pub fn test_cbl() {
    // the string is stored at compile time, ideally we should compress them all!
    let res = cbl_env!(include_str!("test.edn"));
    let res: &str = res.as_ref().try_into().unwrap();
    assert_eq!(res, "Hello");
  }
}

#[cfg(feature = "blocks")]
#[no_mangle]
pub extern "C" fn registerRustBlocks(core: *mut CBCore) {
  unsafe {
    Core = core;
  }

  #[cfg(not(target_arch = "wasm32"))]
  blocks::http::registerBlocks();

  blocks::casting::registerBlocks();
  blocks::hash::registerBlocks();
  blocks::ecdsa::registerBlocks();
  blocks::physics::simulation::registerBlocks();
  blocks::physics::shapes::registerBlocks();
  blocks::physics::rigidbody::registerBlocks();
  blocks::physics::queries::registerBlocks();
  blocks::physics::forces::registerBlocks();
  blocks::svg::registerBlocks();
  blocks::eth::registerBlocks();
  blocks::cb_csv::registerBlocks();
  blocks::curve25519::registerBlocks();
  blocks::substrate::registerBlocks();
  blocks::chachapoly::registerBlocks();

  #[cfg(not(any(target_arch = "wasm32", target_os = "ios")))]
  blocks::browse::registerBlocks();
}

#[no_mangle]
pub extern "C" fn runRuntimeTests() {
  #[cfg(feature = "cblisp")]
  cblisp::test_cbl();
}
