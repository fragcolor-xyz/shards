/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2020 Fragcolor Pte. Ltd. */
#![feature(concat_idents)]
#![cfg_attr(all(target_os = "windows", target_arch = "x86"), feature(abi_thiscall))]
#![allow(non_upper_case_globals)]
#![allow(non_camel_case_types)]
#![allow(non_snake_case)]
#![allow(unused_imports)]
#![allow(unused_macros)]
#![allow(dead_code)]
#![allow(improper_ctypes)]
#![allow(improper_ctypes_definitions)]

#[macro_use]
extern crate approx;

#[macro_use]
extern crate lazy_static;

#[macro_use]
extern crate compile_time_crc32;
pub use compile_time_crc32::crc32;

use git_version::git_version;
pub const GIT_VERSION: &str = git_version!(fallback = "unknown");

#[macro_use]
pub mod core;

#[macro_use]
pub mod shard;
pub mod shardsc;
#[macro_use]
pub mod types;

pub mod util;

#[macro_use]
pub mod logging;

pub use shards_macro::param_set;
pub use shards_macro::shard;
pub use shards_macro::shard_impl;
pub use shards_macro::shards_enum;

use crate::core::Core;
pub use crate::shardsc::*;
use crate::types::Types;
use crate::types::Var;
use std::convert::TryInto;
use std::ffi::CString;

pub const fn fourCharacterCode(val: [u8; 4]) -> i32 {
  let unsigned_val = ((val[0] as u32) << 24 & 0xff000000)
    | ((val[1] as u32) << 16 & 0x00ff0000)
    | ((val[2] as u32) << 8 & 0x0000ff00)
    | ((val[3] as u32) & 0x000000ff);
  unsigned_val as i32
}

pub trait IntoVar {
  fn into_var(self) -> Var;
}

impl IntoVar for &str {
  fn into_var(self) -> Var {
    Var::ephemeral_string(self)
  }
}

impl<T: Into<Var>> IntoVar for T
where
  Var: From<T>,
{
  fn into_var(self) -> Var {
    Var::from(self)
  }
}
