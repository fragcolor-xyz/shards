/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2021 Fragcolor Pte. Ltd. */

use crate::core::do_blocking;
use crate::core::log;
use crate::core::registerShard;
use crate::shard::Shard;
use crate::types::common_type;
use crate::types::ClonedVar;
use crate::types::Context;
use crate::types::ParamVar;
use crate::types::Parameters;
use crate::types::Seq;
use crate::types::Table;
use crate::types::Type;
use crate::types::BYTES_TYPES;
use crate::types::INT_TYPES;
use crate::types::STRING_TYPES;
use crate::CString;
use crate::Types;
use crate::Var;
use core::time::Duration;
use std::convert::TryFrom;
use std::convert::TryInto;
use std::ffi::CStr;
use wasabi_leb128::{ReadLeb128, WriteLeb128};

lazy_static! {
  pub static ref INPUT_TYPES: Vec<Type> = vec![common_type::bytes, common_type::string,];
}

struct ToBase58 {
  output: String,
}
impl Default for ToBase58 {
  fn default() -> Self {
    ToBase58 {
      output: String::new(),
    }
  }
}
impl Shard for ToBase58 {
  fn registerName() -> &'static str {
    cstr!("ToBase58")
  }

  fn hash() -> u32 {
    compile_time_crc32::crc32!("ToBase58-rust-0x20200101")
  }

  fn name(&mut self) -> &str {
    "ToBase58"
  }

  fn inputTypes(&mut self) -> &std::vec::Vec<Type> {
    &INPUT_TYPES
  }

  fn outputTypes(&mut self) -> &std::vec::Vec<Type> {
    &STRING_TYPES
  }

  fn activate(&mut self, _: &Context, input: &Var) -> Result<Var, &str> {
    let bytes: Result<&[u8], &str> = input.as_ref().try_into();
    if let Ok(bytes) = bytes {
      self.output = bs58::encode(bytes).into_string();
    } else {
      let string: Result<&str, &str> = input.as_ref().try_into();
      if let Ok(string) = string {
        self.output = bs58::encode(string).into_string();
      }
    }
    Ok(self.output.as_str().into())
  }
}

struct FromBase58 {
  output: Vec<u8>,
}
impl Default for FromBase58 {
  fn default() -> Self {
    FromBase58 { output: Vec::new() }
  }
}
impl Shard for FromBase58 {
  fn registerName() -> &'static str {
    cstr!("FromBase58")
  }

  fn hash() -> u32 {
    compile_time_crc32::crc32!("FromBase58-rust-0x20200101")
  }

  fn name(&mut self) -> &str {
    "FromBase58"
  }

  fn inputTypes(&mut self) -> &std::vec::Vec<Type> {
    &STRING_TYPES
  }

  fn outputTypes(&mut self) -> &std::vec::Vec<Type> {
    &BYTES_TYPES
  }

  fn activate(&mut self, _: &Context, input: &Var) -> Result<Var, &str> {
    let str_input: &str = input.as_ref().try_into()?;
    self.output = bs58::decode(str_input).into_vec().map_err(|e| {
      shlog!("{}", e);
      "Failed to decode base58"
    })?;
    Ok(self.output.as_slice().into())
  }
}

lazy_static! {
  static ref LEB_PARAMETERS: Parameters = vec![(
    cstr!("Signed"),
    shccstr!("If the integer to encode/decode is signed and can be negative."),
    vec![common_type::bool]
  )
    .into()];
}

struct ToLEB128 {
  output: Vec<u8>,
  signed: bool,
}

impl Default for ToLEB128 {
  fn default() -> Self {
    ToLEB128 {
      output: Vec::new(),
      signed: false,
    }
  }
}

impl Shard for ToLEB128 {
  fn registerName() -> &'static str {
    cstr!("ToLEB128")
  }

  fn hash() -> u32 {
    compile_time_crc32::crc32!("ToLEB128-rust-0x20200101")
  }

  fn name(&mut self) -> &str {
    "ToLEB128"
  }

  fn inputTypes(&mut self) -> &std::vec::Vec<Type> {
    &INT_TYPES
  }

  fn outputTypes(&mut self) -> &std::vec::Vec<Type> {
    &BYTES_TYPES
  }

  fn parameters(&mut self) -> Option<&Parameters> {
    Some(&LEB_PARAMETERS)
  }

  fn setParam(&mut self, index: i32, value: &Var) -> Result<(), &str> {
    match index {
      0 => Ok(self.signed = value.try_into()?),
      _ => unreachable!(),
    }
  }

  fn getParam(&mut self, index: i32) -> Var {
    match index {
      0 => self.signed.into(),
      _ => unreachable!(),
    }
  }

  fn activate(&mut self, _: &Context, input: &Var) -> Result<Var, &str> {
    if self.signed {
      let int_input: i64 = input.as_ref().try_into()?;
      self.output.clear();
      self
        .output
        .write_leb128(int_input)
        .map_err(|_| "Failed to convert int to leb128")?;
    } else {
      let int_input: u64 = input.as_ref().try_into()?;
      self.output.clear();
      self
        .output
        .write_leb128(int_input)
        .map_err(|_| "Failed to convert uint to leb128")?;
    }
    Ok(self.output.as_slice().into())
  }
}

struct FromLEB128 {
  signed: bool,
}

impl Default for FromLEB128 {
  fn default() -> Self {
    FromLEB128 { signed: false }
  }
}

impl Shard for FromLEB128 {
  fn registerName() -> &'static str {
    cstr!("FromLEB128")
  }

  fn hash() -> u32 {
    compile_time_crc32::crc32!("FromLEB128-rust-0x20200101")
  }

  fn name(&mut self) -> &str {
    "FromLEB128"
  }

  fn inputTypes(&mut self) -> &std::vec::Vec<Type> {
    &BYTES_TYPES
  }

  fn outputTypes(&mut self) -> &std::vec::Vec<Type> {
    &INT_TYPES
  }

  fn parameters(&mut self) -> Option<&Parameters> {
    Some(&LEB_PARAMETERS)
  }

  fn setParam(&mut self, index: i32, value: &Var) -> Result<(), &str> {
    match index {
      0 => Ok(self.signed = value.try_into()?),
      _ => unreachable!(),
    }
  }

  fn getParam(&mut self, index: i32) -> Var {
    match index {
      0 => self.signed.into(),
      _ => unreachable!(),
    }
  }

  fn activate(&mut self, _: &Context, input: &Var) -> Result<Var, &str> {
    let mut bytes: &[u8] = input.as_ref().try_into()?;
    if self.signed {
      let value: (i64, usize) = bytes
        .read_leb128()
        .map_err(|_| "Failed to convert LEB128 bytes into integer")?;
      Ok(value.0.into())
    } else {
      let value: (u64, usize) = bytes
        .read_leb128()
        .map_err(|_| "Failed to convert LEB128 bytes into integer")?;
      Ok(value.0.into())
    }
  }
}

pub fn registerShards() {
  registerShard::<ToBase58>();
  registerShard::<FromBase58>();
  registerShard::<ToLEB128>();
  registerShard::<FromLEB128>();
}
