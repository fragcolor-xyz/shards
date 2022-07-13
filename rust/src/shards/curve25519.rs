/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2021 Fragcolor Pte. Ltd. */

use crate::core::do_blocking;
use crate::core::log;
use crate::core::registerShard;
use crate::shard::Shard;
use crate::shards::CRYPTO_KEY_TYPES;
use crate::shardsc::SHType_String;
use crate::types::common_type;
use crate::types::ClonedVar;
use crate::types::Context;
use crate::types::InstanceData;
use crate::types::ParamVar;
use crate::types::Parameters;
use crate::types::Seq;
use crate::types::Table;
use crate::types::Type;
use crate::types::BYTES_TYPES;
use crate::types::STRING_TYPES;
use crate::CString;
use crate::Types;
use crate::Var;
use core::time::Duration;
use sp_core::crypto::Pair;
use sp_core::ed25519;
use sp_core::sr25519;
use std::convert::TryFrom;
use std::convert::TryInto;
use std::ffi::CStr;

static SIGNATURE_TYPES: &[Type] = &[common_type::bytezs, common_type::bytess_var];

lazy_static! {
  static ref PK_TYPES: Vec<Type> = vec![common_type::bytes, common_type::string];
  static ref PARAMETERS: Parameters = vec![(
    cstr!("Key"),
    shccstr!("The private key to be used to sign the hashed message input."),
    CRYPTO_KEY_TYPES
  )
    .into()];
  static ref SIG_PARAMETERS: Parameters = vec![(
    cstr!("Signature"),
    shccstr!(
      "The signature and recovery id generated signing the input message with the private key."
    ),
    SIGNATURE_TYPES
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

    impl Shard for $shard_name {
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
          0 => Ok(self.key.set_param(value)),
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

      fn cleanup(&mut self) -> Result<(), &str> {
        self.key.cleanup();
        Ok(())
      }

      fn activate(&mut self, _: &Context, input: &Var) -> Result<Var, &str> {
        let bytes: &[u8] = input.try_into()?;

        let key = self.key.get();
        let key: $key_type::Pair = get_key(key)?;

        let signature: $key_type::Signature = key.sign(bytes);
        let signature: [u8; $size] = signature.0;

        self.output = signature[..].into();
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
      is_string: bool,
    }

    impl Shard for $shard_name {
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
        self.output = key[..].into();
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
      is_string: bool,
    }

    impl Shard for $shard_name {
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
          self.output = seed[..].into();
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

pub fn registerShards() {
  registerShard::<Sr25519Sign>();
  registerShard::<Ed25519Sign>();
  registerShard::<Sr25519PublicKey>();
  registerShard::<Ed25519PublicKey>();
  registerShard::<Sr25519Seed>();
  registerShard::<Ed25519Seed>();
}
