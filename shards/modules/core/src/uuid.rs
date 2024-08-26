/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2021 Fragcolor Pte. Ltd. */

use shards::core::register_legacy_shard;
use shards::core::register_shard;
use shards::shard::LegacyShard;
use shards::shard::Shard;

use shards::types::ClonedVar;
use shards::types::Context;
use shards::types::ExposedTypes;
use shards::types::InstanceData;
use shards::types::OptionalString;
use shards::types::BOOL_TYPES_SLICE;
use shards::types::BYTES_OR_STRING_TYPES;
use shards::types::BYTES_TYPES;
use shards::types::INT16_TYPES;
use shards::types::INT_TYPES;
use shards::types::INT_TYPES_SLICE;
use shards::types::NONE_TYPES;

use shards::types::Parameters;

use shards::types::Type;

use shards::types::STRING_TYPES;

use shards::types::common_type;
use shards::types::Types;
use shards::types::Var;

use core::convert::TryInto;
use std::str::FromStr;
use std::sync::RwLock;

#[derive(Default)]
struct UUIDCreate {}

impl LegacyShard for UUIDCreate {
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

  fn activate(&mut self, _: &Context, _: &Var) -> Result<Option<Var>, &str> {
    let uuid = uuid::Uuid::new_v4();
    Ok(Some(uuid.as_bytes().into()))
  }
}

#[derive(Default)]
struct UUIDConvert {}

impl LegacyShard for UUIDConvert {
  fn registerName() -> &'static str {
    cstr!("UUID.Convert")
  }

  fn hash() -> u32 {
    compile_time_crc32::crc32!("UUID.Convert-rust-0x20200101")
  }

  fn name(&mut self) -> &str {
    "UUID.Convert"
  }

  fn help(&mut self) -> OptionalString {
    OptionalString(shccstr!(
      "Outputs a UUID (Universally Unique Identifier) as Int16."
    ))
  }

  fn inputTypes(&mut self) -> &std::vec::Vec<Type> {
    &BYTES_OR_STRING_TYPES
  }

  fn outputTypes(&mut self) -> &std::vec::Vec<Type> {
    &INT16_TYPES
  }

  fn activate(&mut self, _: &Context, input: &Var) -> Result<Option<Var>, &str> {
    let uuid = match (<&str>::try_from(input), <&[u8]>::try_from(input)) {
      (Ok(str), _) => uuid::Uuid::from_str(str).map_err(|e| {
        shlog_error!("Failed to parse UUID: {}", e);
        "Failed to parse UUID."
      })?,
      (_, Ok(bytes)) => uuid::Uuid::from_slice(bytes).map_err(|e| {
        shlog_error!("Failed to parse UUID: {}", e);
        "Failed to parse UUID."
      })?,
      _ => return Err("Invalid input type."),
    };
    Ok(Some(uuid.as_bytes().into()))
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

impl LegacyShard for UUIDToString {
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

  fn activate(&mut self, _: &Context, input: &Var) -> Result<Option<Var>, &str> {
    let bytes: [u8; 16] = input.try_into()?;
    let uuid = uuid::Uuid::from_bytes(bytes);
    self.output = if self.hyphenated {
      uuid.hyphenated().to_string().into()
    } else {
      uuid.simple().to_string().into()
    };
    Ok(Some(self.output.0))
  }
}

#[derive(Default)]
struct UUIDToBytes {
  output: ClonedVar,
}

impl LegacyShard for UUIDToBytes {
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

  fn activate(&mut self, _: &Context, input: &Var) -> Result<Option<Var>, &str> {
    let bytes: [u8; 16] = input.try_into()?;
    self.output = bytes.as_ref().into();
    Ok(Some(self.output.0))
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
      size: 21,
      output: Default::default(),
    }
  }
}

impl LegacyShard for NanoIDCreate {
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
    OptionalString(shccstr!("Creates a random NanoID."))
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

  fn activate(&mut self, _: &Context, _: &Var) -> Result<Option<Var>, &str> {
    let size = self.size as usize;
    let id = nanoid::nanoid!(size);
    self.output = id.into();
    Ok(Some(self.output.0))
  }
}

lazy_static! {
  static ref SNOWFLAKE_GENERATOR: RwLock<snowflake::SnowflakeIdGenerator> =
    RwLock::new(snowflake::SnowflakeIdGenerator::new(0, 0));
}

#[derive(shards::shard)]
#[shard_info("Snowflake", "Creates a Snowflake ID.")]
struct SnowflakeShard {
  #[shard_required]
  required: ExposedTypes,
  #[shard_param("MachineId", "The machine ID, must be less than 32", [common_type::int])]
  machine_id: ClonedVar,
  #[shard_param("NodeId", "The node ID, must be less than 32", [common_type::int])]
  node_id: ClonedVar,
}

impl Default for SnowflakeShard {
  fn default() -> Self {
    Self {
      required: ExposedTypes::new(),
      machine_id: 0.into(),
      node_id: 0.into(),
    }
  }
}

#[shards::shard_impl]
impl Shard for SnowflakeShard {
  fn input_types(&mut self) -> &Types {
    &NONE_TYPES
  }

  fn output_types(&mut self) -> &Types {
    &INT_TYPES
  }

  fn warmup(&mut self, ctx: &Context) -> Result<(), &'static str> {
    self.warmup_helper(ctx)?;

    let machine_id: i32 = self.machine_id.0.as_ref().try_into()?;
    let node_id: i32 = self.node_id.0.as_ref().try_into()?;

    if machine_id > 31 {
      return Err("Machine ID must be less than 32.");
    }

    if node_id > 31 {
      return Err("Node ID must be less than 32.");
    }

    Ok(())
  }

  fn cleanup(&mut self, ctx: Option<&Context>) -> Result<(), &str> {
    self.cleanup_helper(ctx)?;

    Ok(())
  }

  fn compose(&mut self, data: &InstanceData) -> Result<Type, &str> {
    self.compose_helper(data)?;
    Ok(self.output_types()[0])
  }

  fn activate(&mut self, _context: &Context, _input: &Var) -> Result<Option<Var>, &str> {
    let mut generator = SNOWFLAKE_GENERATOR.write().unwrap();
    generator.machine_id = self.machine_id.0.as_ref().try_into().unwrap();
    generator.node_id = self.node_id.0.as_ref().try_into().unwrap();
    let id = generator.real_time_generate();
    Ok(Some(id.into()))
  }
}

pub fn register_shards() {
  register_legacy_shard::<UUIDCreate>();
  register_legacy_shard::<UUIDToString>();
  register_legacy_shard::<UUIDToBytes>();
  register_legacy_shard::<NanoIDCreate>();
  register_legacy_shard::<UUIDConvert>();
  register_shard::<SnowflakeShard>();
}
