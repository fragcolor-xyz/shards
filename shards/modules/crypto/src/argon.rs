/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2023 Fragcolor Pte. Ltd. */

use shards::core::register_shard;
use shards::simple_shard;

use argon2::{
  password_hash::{PasswordHash, PasswordHasher, PasswordVerifier, SaltString},
  Argon2,
};
use rand_core::OsRng;

#[simple_shard("Argon2id.Hash", "Hashes a password using the Argon2id algorithm.")]
fn argon2id_hash(
  password: &str,
  #[param("MemoryCost", "The amount of memory to use in KiB. Default is 8192 (8 MB).", default = 8192i64)]
  memory_cost: i64,
  #[param("TimeCost", "The number of iterations to perform. Default is 4.", default = 4i64)]
  time_cost: i64,
  #[param("Parallelism", "The degree of parallelism to use. Default is 1.", default = 1i64)]
  parallelism: i64,
) -> Result<String, &'static str> {
  // Convert parameters to u32
  let memory_cost = u32::try_from(memory_cost).map_err(|_| "Invalid memory cost")?;
  let time_cost = u32::try_from(time_cost).map_err(|_| "Invalid time cost")?;
  let parallelism = u32::try_from(parallelism).map_err(|_| "Invalid parallelism")?;

  // Create an Argon2 instance
  let argon2 = Argon2::new(
    argon2::Algorithm::Argon2id,
    argon2::Version::V0x13,
    argon2::Params::new(memory_cost, time_cost, parallelism, None)
      .map_err(|_| "Invalid Argon2 parameters")?,
  );

  // Generate a random salt
  let salt = SaltString::generate(&mut OsRng);

  // Hash the password
  let password_hash = argon2
    .hash_password(password.as_bytes(), &salt)
    .map_err(|_| "Failed to hash password")?;

  // Convert the PasswordHash to a string
  let hash_string = password_hash.serialize();

  Ok(hash_string.to_string())
}

#[simple_shard("Argon2id.Verify", "Verifies a password against an Argon2id hash.")]
fn argon2id_verify(
  password: &str,
  #[param_var("Hash", "The Argon2id hash to verify against.")]
  hash: &str,
) -> Result<bool, &'static str> {
  let parsed_hash = PasswordHash::new(hash).map_err(|_| "Failed to parse the provided hash")?;

  let result = Argon2::default()
    .verify_password(password.as_bytes(), &parsed_hash)
    .is_ok();

  Ok(result)
}

pub fn register_shards() {
  register_shard::<Argon2idHashShard>();
  register_shard::<Argon2idVerifyShard>();
}
