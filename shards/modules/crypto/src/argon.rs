/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2023 Fragcolor Pte. Ltd. */

use shards::core::register_shard;
use shards::shard::Shard;
use shards::types::{
  common_type, ClonedVar, Context, ExposedTypes, InstanceData, ParamVar, Type, Types, Var,
  BOOL_TYPES, STRING_TYPES,
};

use argon2::{
  password_hash::{PasswordHash, PasswordHasher, PasswordVerifier, SaltString},
  Argon2,
};
use rand_core::OsRng;
use std::convert::TryInto;

#[derive(shards::shard)]
#[shard_info("Argon2id.Hash", "Hashes a password using the Argon2id algorithm.")]
struct Argon2idHashShard {
  #[shard_required]
  required: ExposedTypes,

  #[shard_param("MemoryCost", "The amount of memory to use in KiB. Default is 8192 (8 MB).", [common_type::int])]
  memory_cost: ParamVar,

  #[shard_param("TimeCost", "The number of iterations to perform. Default is 4.", [common_type::int])]
  time_cost: ParamVar,

  #[shard_param("Parallelism", "The degree of parallelism to use. Default is 1.", [common_type::int])]
  parallelism: ParamVar,

  output: ClonedVar,
}

impl Default for Argon2idHashShard {
  fn default() -> Self {
    Self {
      required: ExposedTypes::new(),
      memory_cost: ParamVar::new(8192.into()), // 8 MB
      time_cost: ParamVar::new(4.into()),
      parallelism: ParamVar::new(1.into()),
      output: ClonedVar::default(),
    }
  }
}

#[shards::shard_impl]
impl Shard for Argon2idHashShard {
  fn input_types(&mut self) -> &Types {
    &STRING_TYPES
  }

  fn output_types(&mut self) -> &Types {
    &STRING_TYPES
  }

  fn warmup(&mut self, ctx: &Context) -> Result<(), &str> {
    self.warmup_helper(ctx)?;
    Ok(())
  }

  fn cleanup(&mut self, ctx: Option<&Context>) -> Result<(), &str> {
    self.cleanup_helper(ctx)?;
    Ok(())
  }

  fn compose(&mut self, data: &InstanceData) -> Result<Type, &str> {
    self.compose_helper(data)?;
    // Remove the checks for None, as we now have default values
    Ok(self.output_types()[0])
  }

  fn activate(&mut self, _context: &Context, input: &Var) -> Result<Option<Var>, &str> {
    let password: &str = input.try_into()?;

    let memory_cost: i64 = self.memory_cost.get().try_into()?;
    let time_cost: i64 = self.time_cost.get().try_into()?;
    let parallelism: i64 = self.parallelism.get().try_into()?;

    // Convert parameters to u32
    let memory_cost = u32::try_from(memory_cost).map_err(|_| "Invalid memory cost")?;
    let time_cost = u32::try_from(time_cost).map_err(|_| "Invalid time cost")?;
    let parallelism = u32::try_from(parallelism).map_err(|_| "Invalid parallelism")?;

    // Create an Argon2 instance
    let argon2 = Argon2::new(
      argon2::Algorithm::Argon2id,
      argon2::Version::V0x13,
      argon2::Params::new(memory_cost, time_cost, parallelism, None).unwrap(),
    );

    // Generate a random salt
    let salt = SaltString::generate(&mut OsRng);

    // Hash the password
    let password_hash = argon2
      .hash_password(password.as_bytes(), &salt)
      .map_err(|_| "Failed to hash password")?;

    // Convert the PasswordHash to a string
    let hash_string = password_hash.serialize();

    self.output = Var::ephemeral_string(hash_string.as_str()).into();
    Ok(Some(self.output.0))
  }
}

#[derive(shards::shard)]
#[shard_info("Argon2id.Verify", "Verifies a password against an Argon2id hash.")]
struct Argon2idVerifyShard {
  #[shard_required]
  required: ExposedTypes,

  #[shard_param("Hash", "The Argon2id hash to verify against.", [common_type::string, common_type::string_var])]
  hash: ParamVar,
}

impl Default for Argon2idVerifyShard {
  fn default() -> Self {
    Self {
      required: ExposedTypes::new(),
      hash: ParamVar::default(),
    }
  }
}

#[shards::shard_impl]
impl Shard for Argon2idVerifyShard {
  fn input_types(&mut self) -> &Types {
    &STRING_TYPES
  }

  fn output_types(&mut self) -> &Types {
    &BOOL_TYPES
  }

  fn warmup(&mut self, ctx: &Context) -> Result<(), &str> {
    self.warmup_helper(ctx)?;
    Ok(())
  }

  fn cleanup(&mut self, ctx: Option<&Context>) -> Result<(), &str> {
    self.cleanup_helper(ctx)?;
    Ok(())
  }

  fn compose(&mut self, data: &InstanceData) -> Result<Type, &str> {
    self.compose_helper(data)?;
    if self.hash.is_none() {
      return Err("Hash parameter is required");
    }
    Ok(self.output_types()[0])
  }

  fn activate(&mut self, _context: &Context, input: &Var) -> Result<Option<Var>, &str> {
    let password: &str = input.try_into()?;
    let hash: &str = self.hash.get().try_into()?;

    let parsed_hash = PasswordHash::new(hash).map_err(|_| "Failed to parse the provided hash")?;

    let result = Argon2::default()
      .verify_password(password.as_bytes(), &parsed_hash)
      .is_ok();

    Ok(Some(result.into()))
  }
}

pub fn register_shards() {
  register_shard::<Argon2idHashShard>();
  register_shard::<Argon2idVerifyShard>();
}
