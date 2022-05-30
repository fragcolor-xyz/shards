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

pub mod shard;
mod shardsc;
pub mod core;
#[macro_use]
pub mod types;
// order matters
#[cfg(feature = "shards")]
pub mod shards;

use crate::shard::Shard;
pub use crate::shardsc::*;
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
        let blks = shards!($($param) *);
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

macro_rules! shards {
    (@shard Set :Name .$var:ident) => { shards!(@shard Set (stringify!($var))) };
    (@shard Set .$var:ident) => { shards!(@shard Set (stringify!($var))) };
    (@shard Set :Name .$var:ident) => { shards!(@shard Set (stringify!($var))) };
    (@shard Set .$var:ident) => { shards!(@shard Set (stringify!($var))) };

    // (ShardName)
    (@shard $shard:ident) => {{
        let blk = $crate::core::createShardPtr(stringify!($shard));
        unsafe {
            (*blk).setup.unwrap()(blk);
        }
            blk
    }};

    // (ShardName.ShardName :ParamName ParamVar ...)
    (@shard $shard0:ident.$shard1:ident $(:$pname:tt $param:tt) *) => {{
      let blkname = concat!(stringify!($shard0), ".", stringify!($shard1));
      let blk = $crate::core::createShardPtr(blkname);
      unsafe {
          (*blk).setup.unwrap()(blk);
      }
      let cparams: $crate::shardsc::SHParametersInfo;
      unsafe {
          cparams = (*blk).parameters.unwrap()(blk);
      }
      let params: &[$crate::shardsc::SHParameterInfo] = cparams.into();
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
                  panic!("Parameter not found: {} for shard: {}!", param, blkname);
              }
          }
      ) *
          blk
   }};

    // (ShardName :ParamName ParamVar ...)
    (@shard $shard:ident $(:$pname:tt $param:tt) *) => {{
        let blkname = stringify!($shard);
        let blk = $crate::core::createShardPtr(blkname);
        unsafe {
            (*blk).setup.unwrap()(blk);
        }
        let cparams: $crate::shardsc::SHParametersInfo;
        unsafe {
            cparams = (*blk).parameters.unwrap()(blk);
        }
        let params: &[$crate::shardsc::SHParameterInfo] = cparams.into();
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
                    panic!("Parameter not found: {} for shard: {}!", param, blkname);
                }
            }
        ) *
            blk
     }};

     // (ShardName.ShardName ParamVar ...)
    (@shard $shard0:ident.$shard1:ident $($param:tt) *) => {{
      let blk = $crate::core::createShardPtr(concat!(stringify!($shard0), ".", stringify!($shard1)));
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

    // (ShardName ParamVar ...)
    (@shard $shard:ident $($param:tt) *) => {{
        let blk = $crate::core::createShardPtr(stringify!($shard));
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
    (@shard . $a:expr) => { shards!(@shard Const $a) };

    // this is the CORE evaluator takes a list of shards expressions
    // the current limit is that everything has to be between parenthesis (...)
    ($(($shard:tt $($param:tt) *)) *) => {{
        let mut blks = Vec::<$crate::shardsc::ShardPtr>::new();
        $(
            blks.push(shards!(@shard $shard $($param) *));
        ) *
            blks
    }};
}

// --features "dummy"
#[cfg(any(all(test, not(feature = "sh_static")), target_arch = "wasm32"))]
#[macro_use]
mod dummy_shard {
  // run with: RUST_BACKTRACE=1 cargo test -- --nocapture

  use super::shard::create;
  use super::Types;
  use crate::shard::shard_construct;
  use crate::shard::Shard;
  use crate::shard::ShardWrapper;
  use crate::shlog;
  use crate::shardsc::SHContext;
  use crate::shardsc::SHTypeInfo;
  use crate::shardsc::SHType_Int;
  use crate::shardsc::SHTypesInfo;
  use crate::shardsc::SHVar;
  use crate::shardsc::{Shard as CShard};
  use crate::core::cloneVar;
  use crate::core::createShard;
  use crate::core::init;
  use crate::core::log;
  use crate::core::registerShard;
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

  pub struct DummyShard {
    inputTypes: Types,
    outputTypes: Types,
  }

  impl Default for DummyShard {
    fn default() -> Self {
      DummyShard {
        inputTypes: vec![common_type::none],
        outputTypes: vec![common_type::any],
      }
    }
  }

  impl Shard for DummyShard {
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

    fn activate(&mut self, context: &SHContext, _input: &Var) -> Result<Var, &str> {
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
    registerShard::<DummyShard>();
  }

  fn macroTest() {
    let rust_variable = "Hello World!";
    let rust_variable2 = 2;
    shards!(
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

    shards!(
      (. "Hello")
      (Msg)
    );
  }

  #[test]
  fn instanciate() {
    init();

    let mut blk = create::<DummyShard>();
    assert_eq!("Dummy".to_string(), blk.shard.name());

    let blkname = CString::new("Dummy").expect("CString failed...");
    unsafe {
      let shlk = (*Core).createShard.unwrap()(blkname.as_ptr());
      (*shlk).setup.unwrap()(shlk);
      (*shlk).destroy.unwrap()(shlk);
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
      assert_eq!(a.valueType, SHType_Int);
      assert_eq!(b.valueType, SHType_Int);
      assert_eq!(a.payload.__bindgen_anon_1.intValue, 10);
      assert_eq!(
        a.payload.__bindgen_anon_1.intValue,
        b.payload.__bindgen_anon_1.intValue
      );
    }

    let _v: ClonedVar = a.into();

    shlog!("Hello shards-rs");
  }
}

#[cfg(feature = "scripting")]
pub mod scripting {
  use crate::types::ClonedVar;
  use std::convert::TryInto;

  pub struct ScriptEnv(pub *mut ::core::ffi::c_void);

  impl Drop for ScriptEnv {
    fn drop(&mut self) {
      unsafe {
        crate::core::ScriptEnvDestroy.unwrap()(self.0);
      }
    }
  }

  pub fn new_env() -> ScriptEnv {
    unsafe {
      let current_dir = std::env::current_dir().unwrap();
      let current_dir = current_dir.to_str().unwrap();
      let current_dir = std::ffi::CString::new(current_dir).unwrap();
      let env = crate::core::ScriptEnvCreate.unwrap()(current_dir.as_ptr());
      ScriptEnv(env)
    }
  }

  pub fn new_sub_env(env: &ScriptEnv) -> ScriptEnv {
    unsafe {
      let cenv = crate::core::ScriptEnvCreateSub.unwrap()(env.0);
      ScriptEnv(cenv)
    }
  }

  #[macro_export]
  macro_rules! shards_no_env {
    ($code:expr) => {
      unsafe {
        let code = std::ffi::CString::new($code).expect("CString failed...");
        let mut output = ClonedVar::default();
        let ok = $crate::core::shLispEval(::core::ptr::null_mut(), code.as_ptr(), &mut output.0);
        if ok {
          Some(output)
        } else {
          None
        }
      }
    };
  }

  #[macro_export]
  macro_rules! shards {
    ($code:expr) => {{
      thread_local! {
        static ENV: $crate::scripting::ScriptEnv = {
          let current_dir = std::env::current_dir().unwrap();
          let current_dir = current_dir.to_str().unwrap();
          let current_dir = std::ffi::CString::new(current_dir).unwrap();
          unsafe { $crate::scripting::ScriptEnv($crate::core::ScriptEnvCreate.unwrap()(current_dir.as_ptr())) }
        }
      }
      unsafe {
        let code = std::ffi::CString::new($code).expect("CString failed...");
        let mut output = ClonedVar::default();
        let ok = ENV.with(|env| $crate::core::ScriptEval.unwrap()((*env).0, code.as_ptr(), &mut output.0));
        if ok {
          Some(output)
        } else {
          None
        }
      }
    }};
  }

  #[macro_export]
  macro_rules! shards_env {
    ($env:expr, $code:expr) => {{
      unsafe {
        let code = std::ffi::CString::new($code).expect("CString failed...");
        let mut output = ClonedVar::default();
        let env = $env.0;
        let ok = $crate::core::ScriptEval.unwrap()(env, code.as_ptr(), &mut output.0);
        if ok {
          Some(output)
        } else {
          None
        }
      }
    }};
  }

  pub fn test_shl() {
    // the string is stored at compile time, ideally we should compress them all!
    let res = shards!(include_str!("test.edn")).unwrap();
    let res: &str = res.0.as_ref().try_into().unwrap();
    assert_eq!(res, "Hello");
  }
}

#[cfg(feature = "shards")]
#[no_mangle]
pub extern "C" fn registerRustShards(core: *mut SHCore) {
  unsafe {
    Core = core;
  }

  #[cfg(not(target_arch = "wasm32"))]
  shards::http::registerShards();

  shards::casting::registerShards();
  shards::hash::registerShards();
  shards::ecdsa::registerShards();
  shards::physics::simulation::registerShards();
  shards::physics::shapes::registerShards();
  shards::physics::rigidbody::registerShards();
  shards::physics::queries::registerShards();
  shards::physics::forces::registerShards();
  shards::svg::registerShards();
  shards::eth::registerShards();
  shards::csv::registerShards();
  shards::curve25519::registerShards();
  shards::substrate::registerShards();
  shards::chachapoly::registerShards();

  #[cfg(not(any(target_arch = "wasm32", target_os = "ios")))]
  shards::browse::registerShards();
}

#[no_mangle]
pub extern "C" fn runRuntimeTests() {
  #[cfg(feature = "scripting")]
  scripting::test_shl();
}
