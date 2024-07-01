/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2021 Fragcolor Pte. Ltd. */

use shards::core::register_shard;

use shards::shard::Shard;
use shards::types::common_type;
use shards::types::ClonedVar;
use shards::types::Context;

use shards::types::ExposedTypes;
use shards::types::InstanceData;
use shards::types::ParamVar;

use shards::types::Type;
use shards::types::Types;
use shards::types::BOOL_TYPES;
use shards::types::BYTES_TYPES;

use shards::types::Var;

use std::convert::TryInto;

// signing context, substrate for legacy reasons...
const SIGNING_CTX: &[u8] = b"substrate";

#[derive(shards::shard)]
#[shard_info(
  "Sr25519.Sign",
  "Signs a message using the Schnorr signature on Ristretto compressed Ed25519 points."
)]
struct Sr25519SignShard {
  #[shard_required]
  required: ExposedTypes,

  #[shard_param("Key", "The private key to be used to sign the hashed message input.", [common_type::bytes, common_type::bytes_var])]
  key: ParamVar,

  output: ClonedVar,
}

impl Default for Sr25519SignShard {
  fn default() -> Self {
    Self {
      required: ExposedTypes::new(),
      key: ParamVar::default(),
      output: ClonedVar::default(),
    }
  }
}

#[shards::shard_impl]
impl Shard for Sr25519SignShard {
  fn input_types(&mut self) -> &Types {
    &BYTES_TYPES
  }

  fn output_types(&mut self) -> &Types {
    &BYTES_TYPES
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

    if self.key.is_none() {
      return Err("Key parameter is required");
    }

    Ok(self.output_types()[0])
  }

  fn activate(&mut self, _context: &Context, input: &Var) -> Result<Var, &str> {
    let bytes: &[u8] = input.try_into()?;

    let key = self.key.get();
    let key: &[u8] = key.try_into()?;
    let key = schnorrkel::MiniSecretKey::from_bytes(key).map_err(|e| {
      shlog!("{:?}", e);
      "Failed to parse secret key"
    })?;
    let key = key.expand_to_keypair(schnorrkel::ExpansionMode::Ed25519);

    let sign_context = schnorrkel::signing_context(SIGNING_CTX);
    let signature = key.sign(sign_context.bytes(bytes));
    let signature = signature.to_bytes();

    self.output = Var::ephemeral_slice(&signature).into();
    Ok(self.output.0)
  }
}

#[derive(shards::shard)]
#[shard_info("Sr25519.PublicKey", "Extracts the public key from a Sr25519 keypair")]
struct Sr25519PublicKeyShard {
  output: ClonedVar,
}

impl Default for Sr25519PublicKeyShard {
  fn default() -> Self {
    Self {
      output: ClonedVar::default(),
    }
  }
}

#[shards::shard_impl]
impl Shard for Sr25519PublicKeyShard {
  fn input_types(&mut self) -> &Types {
    &BYTES_TYPES
  }

  fn output_types(&mut self) -> &Types {
    &BYTES_TYPES
  }

  fn warmup(&mut self, ctx: &Context) -> Result<(), &str> {
    self.warmup_helper(ctx)?;

    Ok(())
  }

  fn cleanup(&mut self, ctx: Option<&Context>) -> Result<(), &str> {
    self.cleanup_helper(ctx)?;

    Ok(())
  }

  fn compose(&mut self, _data: &InstanceData) -> Result<Type, &str> {
    Ok(self.output_types()[0])
  }

  fn activate(&mut self, _context: &Context, input: &Var) -> Result<Var, &str> {
    let key: &[u8] = input.try_into()?;
    let key = schnorrkel::MiniSecretKey::from_bytes(key).map_err(|e| {
      shlog!("{:?}", e);
      "Failed to parse secret key"
    })?;
    let key = key.expand_to_keypair(schnorrkel::ExpansionMode::Ed25519);
    let key = key.public.to_bytes();
    self.output = Var::ephemeral_slice(&key).into();
    Ok(self.output.0)
  }
}

#[derive(shards::shard)]
#[shard_info("Sr25519.Verify", "Verifies a Sr25519 signature")]
struct Sr25519VerifyShard {
  #[shard_required]
  required: ExposedTypes,

  #[shard_param("Key", "The public key of the keypair that signed the message. This will be used to verify the signature.", [common_type::bytes, common_type::bytes_var])]
  key: ParamVar,

  #[shard_param("Message", "The message that was signed to produce the signature. This is the original plain bytes message that the signature was created for.", [common_type::bytes, common_type::bytes_var])]
  message: ParamVar,
}

impl Default for Sr25519VerifyShard {
  fn default() -> Self {
    Self {
      required: ExposedTypes::new(),
      key: ParamVar::default(),
      message: ParamVar::default(),
    }
  }
}

#[shards::shard_impl]
impl Shard for Sr25519VerifyShard {
  fn input_types(&mut self) -> &Types {
    &BYTES_TYPES
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

    if self.key.is_none() {
      return Err("Key parameter is required");
    }

    if self.message.is_none() {
      return Err("Message parameter is required");
    }

    Ok(self.output_types()[0])
  }

  fn activate(&mut self, _context: &Context, input: &Var) -> Result<Var, &str> {
    let key = self.key.get();
    let key: &[u8] = key.try_into()?;
    let key = schnorrkel::PublicKey::from_bytes(key).map_err(|e| {
      shlog!("{:?}", e);
      "Failed to parse public key"
    })?;

    let message = self.message.get();
    let message: &[u8] = message.try_into()?;

    let signature = input.try_into()?;
    let signature = schnorrkel::Signature::from_bytes(signature).map_err(|e| {
      shlog!("{:?}", e);
      "Failed to parse signature"
    })?;

    let sign_context = schnorrkel::signing_context(SIGNING_CTX);
    let ok = key.verify(sign_context.bytes(message), &signature).is_ok();

    Ok(ok.into())
  }
}

pub fn register_shards() {
  register_shard::<Sr25519SignShard>();
  register_shard::<Sr25519PublicKeyShard>();
  register_shard::<Sr25519VerifyShard>();
}
