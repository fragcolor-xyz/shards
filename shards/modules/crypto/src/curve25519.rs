/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2021 Fragcolor Pte. Ltd. */

use crate::{CRYPTO_KEY_TYPES, PUB_KEY_TYPES};
use shards::core::register_legacy_shard;
use shards::shard::LegacyShard;

use shards::types::common_type;
use shards::types::ClonedVar;
use shards::types::Context;

use shards::types::ParamVar;
use shards::types::Parameters;

use shards::types::Type;
use shards::types::BOOL_TYPES;
use shards::types::BYTES_TYPES;
use shards::types::STRING_TYPES;

use shards::types::Var;

use sp_core::crypto::Pair;
use sp_core::ed25519;
use sp_core::sr25519;

use std::convert::TryInto;

lazy_static! {
  static ref PK_TYPES: Vec<Type> = vec![common_type::bytes, common_type::string];
  static ref PARAMETERS: Parameters = vec![(
    cstr!("Key"),
    shccstr!("The private key to be used to sign the hashed message input."),
    CRYPTO_KEY_TYPES
  )
    .into()];
  static ref VER_PARAMETERS: Parameters = vec![
    (
      cstr!("Key"),
      shccstr!("The public key of the keypair that signed the message. This will be used to verify the signature."),
      PUB_KEY_TYPES
    )
      .into(),
    (
      cstr!("Message"),
      shccstr!("The message string that was signed to produce the signature. This is the original plaintext message that the signature was created for. When verifying the signature, this message will be hashed and the resulting digest compared to the signature to validate it was produced by signing this exact message."),
      CRYPTO_KEY_TYPES
    )
    .into()];
}

fn get_key<T: Pair>(input: &Var) -> Result<T, &'static str> {
  let key: Result<&[u8], &str> = input.try_into();
  if let Ok(key) = key {
    T::from_seed_slice(key).map_err(|e| {
      shlog!("{:?}", e);
      "Failed to parse secret key"
    })
  } else {
    let key: Result<&str, &str> = input.try_into();
    if let Ok(key) = key {
      T::from_string(key, None).map_err(|e| {
        shlog!("{:?}", e);
        "Failed to parse secret key"
      })
    } else {
      Err("Invalid key value")
    }
  }
}

macro_rules! add_signer {
  ($shard_name:ident, $name_str:literal, $hash:literal, $key_type:tt, $size:literal) => {
    #[derive(Default)]
    struct $shard_name {
      output: ClonedVar,
      key: ParamVar,
    }

    impl LegacyShard for $shard_name {
      fn registerName() -> &'static str {
        cstr!($name_str)
      }

      fn hash() -> u32 {
        compile_time_crc32::crc32!($hash)
      }

      fn name(&mut self) -> &str {
        $name_str
      }

      fn inputTypes(&mut self) -> &std::vec::Vec<Type> {
        &BYTES_TYPES
      }

      fn outputTypes(&mut self) -> &std::vec::Vec<Type> {
        &BYTES_TYPES
      }

      fn parameters(&mut self) -> Option<&Parameters> {
        Some(&PARAMETERS)
      }

      fn setParam(&mut self, index: i32, value: &Var) -> Result<(), &str> {
        match index {
          0 => self.key.set_param(value),
          _ => unreachable!(),
        }
      }

      fn getParam(&mut self, index: i32) -> Var {
        match index {
          0 => self.key.get_param(),
          _ => unreachable!(),
        }
      }

      fn warmup(&mut self, context: &Context) -> Result<(), &str> {
        self.key.warmup(context);
        Ok(())
      }

      fn cleanup(&mut self, ctx: Option<&Context>) -> Result<(), &str> {
        self.key.cleanup(ctx);
        Ok(())
      }

      fn activate(&mut self, _: &Context, input: &Var) -> Result<Var, &str> {
        let bytes: &[u8] = input.try_into()?;

        let key = self.key.get();
        let key: $key_type::Pair = get_key(key)?;

        let signature: $key_type::Signature = key.sign(bytes);
        let signature: [u8; $size] = signature.0;

        self.output.assign(&signature[..].into());
        Ok(self.output.0)
      }
    }
  };
}

add_signer!(
  Sr25519Sign,
  "Sr25519.Sign",
  "Sr25519.Sign-rust-0x20200101",
  sr25519,
  64
);

add_signer!(
  Ed25519Sign,
  "Ed25519.Sign",
  "Ed25519.Sign-rust-0x20200101",
  ed25519,
  64
);

macro_rules! add_pub_key {
  ($shard_name:ident, $name_str:literal, $hash:literal, $key_type:tt, $size:literal) => {
    #[derive(Default)]
    struct $shard_name {
      output: ClonedVar,
    }

    impl LegacyShard for $shard_name {
      fn registerName() -> &'static str {
        cstr!($name_str)
      }

      fn hash() -> u32 {
        compile_time_crc32::crc32!($hash)
      }

      fn name(&mut self) -> &str {
        $name_str
      }

      fn inputTypes(&mut self) -> &std::vec::Vec<Type> {
        &PK_TYPES
      }

      fn outputTypes(&mut self) -> &std::vec::Vec<Type> {
        &BYTES_TYPES
      }

      fn activate(&mut self, _: &Context, input: &Var) -> Result<Var, &str> {
        let key: $key_type::Pair = get_key(input)?;
        let key = key.public();
        let key: &[u8; $size] = key.as_array_ref();
        self.output.assign(&key[..].into());
        Ok(self.output.0)
      }
    }
  };
}

add_pub_key!(
  Sr25519PublicKey,
  "Sr25519.PublicKey",
  "Sr25519.PublicKey-rust-0x20200101",
  sr25519,
  32
);

add_pub_key!(
  Ed25519PublicKey,
  "Ed25519.PublicKey",
  "Ed25519.PublicKey-rust-0x20200101",
  ed25519,
  32
);

macro_rules! add_priv_key {
  ($shard_name:ident, $name_str:literal, $hash:literal, $key_type:tt, $size:literal) => {
    #[derive(Default)]
    struct $shard_name {
      output: ClonedVar,
    }

    impl LegacyShard for $shard_name {
      fn registerName() -> &'static str {
        cstr!($name_str)
      }

      fn hash() -> u32 {
        compile_time_crc32::crc32!($hash)
      }

      fn name(&mut self) -> &str {
        $name_str
      }

      fn inputTypes(&mut self) -> &std::vec::Vec<Type> {
        &STRING_TYPES
      }

      fn outputTypes(&mut self) -> &std::vec::Vec<Type> {
        &BYTES_TYPES
      }

      fn activate(&mut self, _: &Context, input: &Var) -> Result<Var, &str> {
        let key: &str = input.try_into()?;
        let (_, seed) = $key_type::Pair::from_string_with_seed(key, None).map_err(|e| {
          shlog!("{:?}", e);
          "Failed to parse secret key mnemonic or string"
        })?;
        if let Some(seed) = seed {
          self.output.assign(&seed[..].into());
          Ok(self.output.0)
        } else {
          Err("No seed found")
        }
      }
    }
  };
}

add_priv_key!(
  Sr25519Seed,
  "Sr25519.Seed",
  "Sr25519.Seed-rust-0x20200101",
  sr25519,
  32
);

add_priv_key!(
  Ed25519Seed,
  "Ed25519.Seed",
  "Ed25519.Seed-rust-0x20200101",
  ed25519,
  32
);

macro_rules! add_verifier {
  ($shard_name:ident, $name_str:literal, $hash:literal, $key_type:tt, $size:literal) => {
    #[derive(Default)]
    struct $shard_name {
      key: ParamVar,
      msg: ParamVar,
    }

    impl LegacyShard for $shard_name {
      fn registerName() -> &'static str {
        cstr!($name_str)
      }

      fn hash() -> u32 {
        compile_time_crc32::crc32!($hash)
      }

      fn name(&mut self) -> &str {
        $name_str
      }

      fn inputTypes(&mut self) -> &std::vec::Vec<Type> {
        &BYTES_TYPES
      }

      fn outputTypes(&mut self) -> &std::vec::Vec<Type> {
        &BOOL_TYPES
      }

      fn parameters(&mut self) -> Option<&Parameters> {
        Some(&VER_PARAMETERS)
      }

      fn setParam(&mut self, index: i32, value: &Var) -> Result<(), &str> {
        match index {
          0 => self.key.set_param(value),
          1 => self.msg.set_param(value),
          _ => unreachable!(),
        }
      }

      fn getParam(&mut self, index: i32) -> Var {
        match index {
          0 => self.key.get_param(),
          1 => self.msg.get_param(),
          _ => unreachable!(),
        }
      }

      fn warmup(&mut self, context: &Context) -> Result<(), &str> {
        self.key.warmup(context);
        self.msg.warmup(context);
        Ok(())
      }

      fn cleanup(&mut self, ctx: Option<&Context>) -> Result<(), &str> {
        self.key.cleanup(ctx);
        self.msg.cleanup(ctx);
        Ok(())
      }

      fn activate(&mut self, _: &Context, input: &Var) -> Result<Var, &str> {
        let bytes: &[u8] = input.try_into()?;
        if bytes.len() < $size {
          return Err("Invalid signature length");
        }
        let mut sig: [u8; $size] = [0; $size];
        sig.copy_from_slice(&bytes[..$size]);
        let sig = $key_type::Signature(sig);

        let bytes = self.key.get();
        let bytes: &[u8] = bytes.try_into()?;
        if bytes.len() < 32 {
          return Err("Invalid public key length");
        }
        let mut pub_key: [u8; 32] = [0; 32];
        pub_key.copy_from_slice(&bytes[..32]);
        let pub_key = $key_type::Public::from_raw(pub_key);

        let bytes = self.msg.get();
        let msg: &[u8] = bytes.try_into()?;

        let ok = $key_type::Pair::verify(&sig, msg, &pub_key);

        Ok(ok.into())
      }
    }
  };
}

add_verifier!(
  Sr25519Verify,
  "Sr25519.Verify",
  "Sr25519.Verify-rust-0x20200101",
  sr25519,
  64
);

add_verifier!(
  Ed25519Verify,
  "Ed25519.Verify",
  "Ed25519.Verify-rust-0x20200101",
  ed25519,
  64
);

pub fn register_shards() {
  register_legacy_shard::<Sr25519Sign>();
  register_legacy_shard::<Ed25519Sign>();
  register_legacy_shard::<Sr25519PublicKey>();
  register_legacy_shard::<Ed25519PublicKey>();
  register_legacy_shard::<Sr25519Seed>();
  register_legacy_shard::<Ed25519Seed>();
  register_legacy_shard::<Sr25519Verify>();
  register_legacy_shard::<Ed25519Verify>();
}
