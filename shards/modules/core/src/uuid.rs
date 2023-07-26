/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2021 Fragcolor Pte. Ltd. */

use shards::core::registerShard;
use shards::shard::Shard;

use shards::types::ClonedVar;
use shards::types::Context;
use shards::types::INT_TYPES_SLICE;
use shards::types::OptionalString;
use shards::types::BOOL_TYPES_SLICE;
use shards::types::BYTES_TYPES;
use shards::types::INT16_TYPES;
use shards::types::NONE_TYPES;

use shards::types::Parameters;

use shards::types::Type;

use shards::types::STRING_TYPES;

use shards::types::Var;

use core::convert::TryInto;

#[derive(Default)]
struct UUIDCreate {}

impl Shard for UUIDCreate {
  fn registerName() -> &'static str {
    cstr!("UUID")
  }

  fn hash() -> u32 {
    compile_time_crc32::crc32!("UUID-rust-0x20200101")
  }

  fn name(&mut self) -> &str {
    "UUID"
  }

  fn help(&mut self) -> OptionalString {
    OptionalString(shccstr!("Outputs a UUID (Universally Unique Identifier)."))
  }

  fn inputTypes(&mut self) -> &std::vec::Vec<Type> {
    &NONE_TYPES
  }

  fn outputTypes(&mut self) -> &std::vec::Vec<Type> {
    &INT16_TYPES
  }

  fn activate(&mut self, _: &Context, _: &Var) -> Result<Var, &str> {
    let uuid = uuid::Uuid::new_v4();
    Ok(uuid.as_bytes().into())
  }
}

lazy_static! {
  static ref PARAMETERS: Parameters = vec![(
    cstr!("Hyphenated"),
    shccstr!("Whether to use hyphens in the output."),
    BOOL_TYPES_SLICE
  )
    .into(),];
}

#[derive(Default)]
struct UUIDToString {
  output: ClonedVar,
  hyphenated: bool,
}

impl Shard for UUIDToString {
  fn registerName() -> &'static str {
    cstr!("UUID.ToString")
  }

  fn hash() -> u32 {
    compile_time_crc32::crc32!("UUID.ToString-rust-0x20200101")
  }

  fn name(&mut self) -> &str {
    "UUID.ToString"
  }

  fn help(&mut self) -> OptionalString {
    OptionalString(shccstr!(
      "Reads an UUID and formats it into a readable string."
    ))
  }

  fn inputTypes(&mut self) -> &std::vec::Vec<Type> {
    &INT16_TYPES
  }

  fn outputTypes(&mut self) -> &std::vec::Vec<Type> {
    &STRING_TYPES
  }

  fn parameters(&mut self) -> Option<&Parameters> {
    Some(&PARAMETERS)
  }

  fn setParam(&mut self, index: i32, value: &Var) -> Result<(), &str> {
    match index {
      0 => Ok(self.hyphenated = value.try_into()?),
      _ => unreachable!(),
    }
  }

  fn getParam(&mut self, index: i32) -> Var {
    match index {
      0 => self.hyphenated.into(),
      _ => unreachable!(),
    }
  }

  fn activate(&mut self, _: &Context, input: &Var) -> Result<Var, &str> {
    let bytes: [u8; 16] = input.try_into()?;
    let uuid = uuid::Uuid::from_bytes(bytes);
    self.output = if self.hyphenated {
      uuid.hyphenated().to_string().into()
    } else {
      uuid.simple().to_string().into()
    };
    Ok(self.output.0)
  }
}

#[derive(Default)]
struct UUIDToBytes {
  output: ClonedVar,
}

impl Shard for UUIDToBytes {
  fn registerName() -> &'static str {
    cstr!("UUID.ToBytes")
  }

  fn hash() -> u32 {
    compile_time_crc32::crc32!("UUID.ToBytes-rust-0x20200101")
  }

  fn name(&mut self) -> &str {
    "UUID.ToBytes"
  }

  fn help(&mut self) -> OptionalString {
    OptionalString(shccstr!("Reads an UUID and formats it into bytes."))
  }

  fn inputTypes(&mut self) -> &std::vec::Vec<Type> {
    &INT16_TYPES
  }

  fn outputTypes(&mut self) -> &std::vec::Vec<Type> {
    &BYTES_TYPES
  }

  fn activate(&mut self, _: &Context, input: &Var) -> Result<Var, &str> {
    let bytes: [u8; 16] = input.try_into()?;
    self.output = bytes.as_ref().into();
    Ok(self.output.0)
  }
}

lazy_static! {
  static ref NANO_PARAMETERS: Parameters = vec![(
    cstr!("Size"),
    shccstr!("The output string length of the created NanoID."),
    INT_TYPES_SLICE
  )
    .into(),];
}

struct NanoIDCreate {
  size: i64,
  output: ClonedVar,
}

impl Default for NanoIDCreate {
  fn default() -> Self {
    Self {
      size: 16,
      output: Default::default(),
    }
  }
}

impl Shard for NanoIDCreate {
  fn registerName() -> &'static str {
    cstr!("NanoID")
  }

  fn hash() -> u32 {
    compile_time_crc32::crc32!("NanoID-rust-0x20200101")
  }

  fn name(&mut self) -> &str {
    "NanoID"
  }

  fn help(&mut self) -> OptionalString {
    OptionalString(shccstr!(
      "Creates a random NanoID."
    ))
  }

  fn inputTypes(&mut self) -> &std::vec::Vec<Type> {
    &NONE_TYPES
  }

  fn outputTypes(&mut self) -> &std::vec::Vec<Type> {
    &STRING_TYPES
  }

  fn parameters(&mut self) -> Option<&Parameters> {
    Some(&PARAMETERS)
  }

  fn setParam(&mut self, index: i32, value: &Var) -> Result<(), &str> {
    match index {
      0 => Ok(self.size = value.try_into()?),
      _ => unreachable!(),
    }
  }

  fn getParam(&mut self, index: i32) -> Var {
    match index {
      0 => self.size.into(),
      _ => unreachable!(),
    }
  }

  fn activate(&mut self, _: &Context, _: &Var) -> Result<Var, &str> {
    let size = self.size as usize;
    let id = nanoid::nanoid!(size);
    self.output = id.into();
    Ok(self.output.0)
  }
}

pub fn registerShards() {
  registerShard::<UUIDCreate>();
  registerShard::<UUIDToString>();
  registerShard::<UUIDToBytes>();
  registerShard::<NanoIDCreate>();
}
