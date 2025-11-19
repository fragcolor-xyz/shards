/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2021 Fragcolor Pte. Ltd. */

use shards::core::register_legacy_shard;
use shards::core::register_shard;
use shards::shard::LegacyShard;
use shards::shard::Shard;
use shards::simple_shard;

use shards::types::ClonedVar;
use shards::types::Context;
use shards::types::ExposedTypes;
use shards::types::InstanceData;
use shards::types::OptionalString;
use shards::types::BYTES_OR_STRING_TYPES;
use shards::types::INT_TYPES;

use shards::types::Type;

use shards::types::common_type;
use shards::types::Types;
use shards::types::Var;

use core::convert::TryInto;
use std::str::FromStr;
use std::sync::RwLock;

// Simple shards using the new macro

#[simple_shard("UUID", "Outputs a UUID (Universally Unique Identifier).")]
fn uuid_create(_: ()) -> [u8; 16] {
  let uuid = uuid::Uuid::new_v4();
  *uuid.as_bytes()
}

#[simple_shard("UUID.ToString", "Reads a UUID and formats it into a readable string.")]
fn uuid_to_string(
  input: [u8; 16],
  #[param("Hyphenated", "Whether to use hyphens in the output.", default = false)]
  hyphenated: bool,
) -> String {
  let uuid = uuid::Uuid::from_bytes(input);
  if hyphenated {
    uuid.hyphenated().to_string()
  } else {
    uuid.simple().to_string()
  }
}

#[simple_shard("UUID.ToBytes", "Reads a UUID and formats it into bytes.")]
fn uuid_to_bytes(input: [u8; 16]) -> Vec<u8> {
  input.to_vec()
}

#[simple_shard("NanoID", "Creates a random NanoID.")]
fn nanoid_create(
  _: (),
  #[param("Size", "The output string length of the created NanoID.", default = 21i64)]
  size: i64,
) -> String {
  let size = size as usize;
  nanoid::nanoid!(size)
}

// Legacy shard for UUID.Convert (handles multiple input types)

#[derive(Default)]
struct UUIDConvert {}

impl LegacyShard for UUIDConvert {
  fn registerName() -> &'static str {
    cstr!("UUID.Convert")
  }

  fn hash() -> u32 {
    compile_time_crc32::crc32!("UUID.Convert-rust-0x20250822")
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
    &shards::types::INT16_TYPES
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

// Snowflake shard (uses global state and custom warmup validation)

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
    &shards::types::NONE_TYPES
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
  register_shard::<UUIDShard>();
  register_shard::<UUIDToStringShard>();
  register_shard::<UUIDToBytesShard>();
  register_shard::<NanoIDShard>();
  register_legacy_shard::<UUIDConvert>();
  register_shard::<SnowflakeShard>();
}
