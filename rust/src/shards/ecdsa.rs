/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2021 Fragcolor Pte. Ltd. */

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
use crate::types::BOOL_TYPES_SLICE;
use crate::types::BYTES_TYPES;
use crate::types::STRING_TYPES;
use crate::CString;
use crate::Types;
use crate::Var;
use core::time::Duration;
use sp_core::crypto::Pair;
use sp_core::ecdsa;
use std::convert::TryFrom;
use std::convert::TryInto;
use std::ffi::CStr;

static SIGNATURE_TYPES: &[Type] = &[common_type::bytes, common_type::bytes_var];

lazy_static! {
  static ref PARAMETERS: Parameters = vec![(
    cstr!("Key"),
    shccstr!("The private key to be used to sign the hashed message input."),
    CRYPTO_KEY_TYPES
  )
    .into()];
  static ref PK_TYPES: Vec<Type> = vec![common_type::bytes, common_type::string];
  static ref PK_PARAMETERS: Parameters = vec![(
    cstr!("Compressed"),
    shccstr!("If the output PublicKey should use the compressed format."),
    BOOL_TYPES_SLICE
  )
    .into()];
  static ref SIG_PARAMETERS: Parameters = vec![
    (
      cstr!("Signature"),
      shccstr!("The signature generated signing the input message with the private key."),
      SIGNATURE_TYPES
    )
      .into(),
    (
      cstr!("Compressed"),
      shccstr!("If the output PublicKey should use the compressed format."),
      BOOL_TYPES_SLICE
    )
      .into()
  ];
}

fn get_key(input: &Var) -> Result<libsecp256k1::SecretKey, &'static str> {
  let key: Result<&[u8], &str> = input.try_into();
  if let Ok(key) = key {
    libsecp256k1::SecretKey::parse_slice(key).map_err(|e| {
      shlog!("{}", e);
      "Failed to parse secret key"
    })
  } else {
    let key: Result<&str, &str> = input.try_into();
    if let Ok(key) = key {
      // use this to allow even mnemonics to work!
      let pair = ecdsa::Pair::from_string(key, None).map_err(|e| {
        shlog!("{:?}", e);
        "Failed to parse secret key"
      })?;
      let key = pair.seed();
      libsecp256k1::SecretKey::parse(&key).map_err(|e| {
        shlog!("{}", e);
        "Failed to parse secret key"
      })
    } else {
      Err("Invalid key value")
    }
  }
}

#[derive(Default)]
struct ECDSASign {
  output: ClonedVar,
  key: ParamVar,
}

impl Shard for ECDSASign {
  fn registerName() -> &'static str {
    cstr!("ECDSA.Sign")
  }

  fn hash() -> u32 {
    compile_time_crc32::crc32!("ECDSA.Sign-rust-0x20200101")
  }

  fn name(&mut self) -> &str {
    "ECDSA.Sign"
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
    let key = get_key(key)?;

    let msg = libsecp256k1::Message::parse_slice(bytes).map_err(|e| {
      shlog!("{}", e);
      "Failed to parse input message hash"
    })?;

    let x = libsecp256k1::sign(&msg, &key);
    let mut signature: [u8; 65] = [0; 65];
    signature[0..64].copy_from_slice(&x.0.serialize()[..]);
    signature[64] = x.1.serialize();
    self.output = signature[..].into();
    Ok(self.output.0)
  }
}

#[derive(Default)]
struct ECDSAPubKey {
  output: ClonedVar,
  compressed: bool,
}

impl Shard for ECDSAPubKey {
  fn registerName() -> &'static str {
    cstr!("ECDSA.PublicKey")
  }

  fn hash() -> u32 {
    compile_time_crc32::crc32!("ECDSA.PublicKey-rust-0x20200101")
  }

  fn name(&mut self) -> &str {
    "ECDSA.PublicKey"
  }

  fn inputTypes(&mut self) -> &std::vec::Vec<Type> {
    &PK_TYPES
  }

  fn outputTypes(&mut self) -> &std::vec::Vec<Type> {
    &BYTES_TYPES
  }

  fn parameters(&mut self) -> Option<&Parameters> {
    Some(&PK_PARAMETERS)
  }

  fn setParam(&mut self, index: i32, value: &Var) -> Result<(), &str> {
    match index {
      0 => Ok(self.compressed = value.try_into()?),
      _ => unreachable!(),
    }
  }

  fn getParam(&mut self, index: i32) -> Var {
    match index {
      0 => self.compressed.into(),
      _ => unreachable!(),
    }
  }

  fn activate(&mut self, _: &Context, input: &Var) -> Result<Var, &str> {
    let key = get_key(input)?;
    let key = libsecp256k1::PublicKey::from_secret_key(&key);
    if !self.compressed {
      let key: [u8; 65] = key.serialize();
      self.output = key[..].into();
    } else {
      let key: [u8; 33] = key.serialize_compressed();
      self.output = key[..].into();
    }
    Ok(self.output.0)
  }
}

#[derive(Default)]
struct ECDSAPrivKey {
  output: ClonedVar,
  compressed: bool,
}

impl Shard for ECDSAPrivKey {
  fn registerName() -> &'static str {
    cstr!("ECDSA.Seed")
  }

  fn hash() -> u32 {
    compile_time_crc32::crc32!("ECDSA.Seed-rust-0x20200101")
  }

  fn name(&mut self) -> &str {
    "ECDSA.Seed"
  }

  fn inputTypes(&mut self) -> &std::vec::Vec<Type> {
    &STRING_TYPES
  }

  fn outputTypes(&mut self) -> &std::vec::Vec<Type> {
    &BYTES_TYPES
  }

  fn parameters(&mut self) -> Option<&Parameters> {
    Some(&PK_PARAMETERS)
  }

  fn setParam(&mut self, index: i32, value: &Var) -> Result<(), &str> {
    match index {
      0 => Ok(self.compressed = value.try_into()?),
      _ => unreachable!(),
    }
  }

  fn getParam(&mut self, index: i32) -> Var {
    match index {
      0 => self.compressed.into(),
      _ => unreachable!(),
    }
  }

  fn activate(&mut self, _: &Context, input: &Var) -> Result<Var, &str> {
    let key = get_key(input)?;
    let key: [u8; 32] = key.serialize();
    self.output = key[..].into();
    Ok(self.output.0)
  }
}

#[derive(Default)]
struct ECDSARecover {
  output: ClonedVar,
  compressed: bool,
  signature: ParamVar,
}

impl Shard for ECDSARecover {
  fn registerName() -> &'static str {
    cstr!("ECDSA.Recover")
  }

  fn hash() -> u32 {
    compile_time_crc32::crc32!("ECDSA.Recover-rust-0x20200101")
  }

  fn name(&mut self) -> &str {
    "ECDSA.Recover"
  }

  fn inputTypes(&mut self) -> &std::vec::Vec<Type> {
    &BYTES_TYPES
  }

  fn outputTypes(&mut self) -> &std::vec::Vec<Type> {
    &BYTES_TYPES
  }

  fn parameters(&mut self) -> Option<&Parameters> {
    Some(&SIG_PARAMETERS)
  }

  fn setParam(&mut self, index: i32, value: &Var) -> Result<(), &str> {
    match index {
      0 => Ok(self.signature.set_param(value)),
      1 => Ok(self.compressed = value.try_into()?),
      _ => unreachable!(),
    }
  }

  fn getParam(&mut self, index: i32) -> Var {
    match index {
      0 => self.signature.get_param(),
      1 => self.compressed.into(),
      _ => unreachable!(),
    }
  }

  fn warmup(&mut self, context: &Context) -> Result<(), &str> {
    self.signature.warmup(context);
    Ok(())
  }

  fn cleanup(&mut self) -> Result<(), &str> {
    self.signature.cleanup();
    Ok(())
  }

  fn activate(&mut self, _: &Context, input: &Var) -> Result<Var, &str> {
    let bytes: &[u8] = input.try_into()?;

    let signature = self.signature.get();
    let x: &[u8] = signature.try_into()?;
    let sig = libsecp256k1::Signature::parse_overflowing_slice(&x[..64]).map_err(|e| {
      shlog!("{}", e);
      "Failed to parse signature"
    })?;
    let ri = libsecp256k1::RecoveryId::parse(x[64]).map_err(|e| {
      shlog!("{}", e);
      "Failed to parse signature"
    })?;

    let msg = libsecp256k1::Message::parse_slice(bytes).map_err(|e| {
      shlog!("{}", e);
      "Failed to parse input message hash"
    })?;

    let pub_key = libsecp256k1::recover(&msg, &sig, &ri).map_err(|e| {
      shlog!("{}", e);
      "Failed to recover public key"
    })?;

    if self.compressed {
      let key: [u8; 33] = pub_key.serialize_compressed();
      self.output = key[..].into();
    } else {
      let key: [u8; 65] = pub_key.serialize();
      self.output = key[..].into();
    }
    Ok(self.output.0)
  }
}

pub fn registerShards() {
  registerShard::<ECDSASign>();
  registerShard::<ECDSAPubKey>();
  registerShard::<ECDSAPrivKey>();
  registerShard::<ECDSARecover>();
}
