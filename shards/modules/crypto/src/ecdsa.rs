/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2021 Fragcolor Pte. Ltd. */

use crate::CRYPTO_KEY_TYPES;
use shards::core::register_legacy_shard;
use shards::shard::LegacyShard;

use shards::types::common_type;
use shards::types::ClonedVar;
use shards::types::Context;

use shards::types::ParamVar;
use shards::types::Parameters;

use shards::types::OptionalString;
use shards::types::Type;
use shards::types::BOOL_TYPES_SLICE;
use shards::types::BYTES_TYPES;
use shards::types::STRING_TYPES;

use shards::types::Var;

use sp_core::crypto::Pair;
use sp_core::ecdsa;

use std::convert::TryInto;

static SIGNATURE_TYPES: &[Type] = &[common_type::bytes, common_type::bytes_var];

lazy_static! {
  static ref PARAMETERS: Parameters = vec![(
    cstr!("Key"),
    shccstr!("The private key used to sign the hashed message input."),
    CRYPTO_KEY_TYPES
  )
    .into()];
  static ref PK_TYPES: Vec<Type> = vec![common_type::bytes, common_type::string];
  static ref PK_PARAMETERS: Parameters = vec![(
    cstr!("Compressed"),
    shccstr!("Indicates if the output PublicKey should be in compressed format."),
    BOOL_TYPES_SLICE
  )
    .into()];
  static ref SIG_PARAMETERS: Parameters = vec![
    (
      cstr!("Signature"),
      shccstr!("The signature generated from signing the input message with the private key."),
      SIGNATURE_TYPES
    )
      .into(),
    (
      cstr!("Compressed"),
      shccstr!("Indicates if the output PublicKey should be in compressed format."),
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

impl LegacyShard for ECDSASign {
  fn registerName() -> &'static str {
    cstr!("ECDSA.Sign")
  }

  fn hash() -> u32 {
    compile_time_crc32::crc32!("ECDSA.Sign-rust-0x20200101")
  }

  fn name(&mut self) -> &str {
    "ECDSA.Sign"
  }

  fn help(&mut self) -> OptionalString {
    shccstr!("Signs a message with the private key using the ECDSA algorithm.").into()
  }

  fn inputTypes(&mut self) -> &std::vec::Vec<Type> {
    &BYTES_TYPES
  }

  fn inputHelp(&mut self) -> OptionalString {
    shccstr!("The message hash to sign with the private key, must be 32 bytes.").into()
  }

  fn outputTypes(&mut self) -> &std::vec::Vec<Type> {
    &BYTES_TYPES
  }

  fn outputHelp(&mut self) -> OptionalString {
    shccstr!("The signature generated from signing the input message with the private key.").into()
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
    let key = get_key(key)?;

    let msg = libsecp256k1::Message::parse_slice(bytes).map_err(|e| {
      shlog!("{}", e);
      "Failed to parse input message hash"
    })?;

    let x = libsecp256k1::sign(&msg, &key);
    let mut signature: [u8; 65] = [0; 65];
    signature[0..64].copy_from_slice(&x.0.serialize()[..]);
    signature[64] = x.1.serialize();
    self.output.assign(&signature[..].into());
    Ok(self.output.0)
  }
}

#[derive(Default)]
struct ECDSAPubKey {
  output: ClonedVar,
  compressed: bool,
}

impl LegacyShard for ECDSAPubKey {
  fn registerName() -> &'static str {
    cstr!("ECDSA.PublicKey")
  }

  fn hash() -> u32 {
    compile_time_crc32::crc32!("ECDSA.PublicKey-rust-0x20200101")
  }

  fn name(&mut self) -> &str {
    "ECDSA.PublicKey"
  }

  fn help(&mut self) -> OptionalString {
    shccstr!("Generates the public key from the private key using the ECDSA algorithm.").into()
  }

  fn inputTypes(&mut self) -> &std::vec::Vec<Type> {
    &PK_TYPES
  }

  fn inputHelp(&mut self) -> OptionalString {
    shccstr!("The private key to generate the public key from.").into()
  }

  fn outputTypes(&mut self) -> &std::vec::Vec<Type> {
    &BYTES_TYPES
  }

  fn outputHelp(&mut self) -> OptionalString {
    shccstr!("The public key generated from the private key.").into()
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
      self.output.assign(&key[..].into());
    } else {
      let key: [u8; 33] = key.serialize_compressed();
      self.output.assign(&key[..].into());
    }
    Ok(self.output.0)
  }
}

#[derive(Default)]
struct ECDSAPrivKey {
  output: ClonedVar,
  compressed: bool,
}

impl LegacyShard for ECDSAPrivKey {
  fn registerName() -> &'static str {
    cstr!("ECDSA.Seed")
  }

  fn hash() -> u32 {
    compile_time_crc32::crc32!("ECDSA.Seed-rust-0x20200101")
  }

  fn name(&mut self) -> &str {
    "ECDSA.Seed"
  }

  fn help(&mut self) -> OptionalString {
    shccstr!("Generates the private key from the seed using the ECDSA algorithm.").into()
  }

  fn inputTypes(&mut self) -> &std::vec::Vec<Type> {
    &STRING_TYPES
  }

  fn inputHelp(&mut self) -> OptionalString {
    shccstr!("The seed to generate the private key from.").into()
  }

  fn outputTypes(&mut self) -> &std::vec::Vec<Type> {
    &BYTES_TYPES
  }

  fn outputHelp(&mut self) -> OptionalString {
    shccstr!("The private key generated from the seed.").into()
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
    self.output.assign(&key[..].into());
    Ok(self.output.0)
  }
}

#[derive(Default)]
struct ECDSARecover {
  output: ClonedVar,
  compressed: bool,
  signature: ParamVar,
}

impl LegacyShard for ECDSARecover {
  fn registerName() -> &'static str {
    cstr!("ECDSA.Recover")
  }

  fn hash() -> u32 {
    compile_time_crc32::crc32!("ECDSA.Recover-rust-0x20200101")
  }

  fn name(&mut self) -> &str {
    "ECDSA.Recover"
  }

  fn help(&mut self) -> OptionalString {
    shccstr!("Recovers the public key from the signature and message using the ECDSA algorithm.")
      .into()
  }

  fn inputTypes(&mut self) -> &std::vec::Vec<Type> {
    &BYTES_TYPES
  }

  fn inputHelp(&mut self) -> OptionalString {
    shccstr!("The message hash to recover the public key from.").into()
  }

  fn outputTypes(&mut self) -> &std::vec::Vec<Type> {
    &BYTES_TYPES
  }

  fn outputHelp(&mut self) -> OptionalString {
    shccstr!("The public key recovered from the signature and message.").into()
  }

  fn parameters(&mut self) -> Option<&Parameters> {
    Some(&SIG_PARAMETERS)
  }

  fn setParam(&mut self, index: i32, value: &Var) -> Result<(), &str> {
    match index {
      0 => self.signature.set_param(value),
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

  fn cleanup(&mut self, ctx: Option<&Context>) -> Result<(), &str> {
    self.signature.cleanup(ctx);
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
      self.output.assign(&key[..].into());
    } else {
      let key: [u8; 65] = pub_key.serialize();
      self.output.assign(&key[..].into());
    }
    Ok(self.output.0)
  }
}

pub fn register_shards() {
  register_legacy_shard::<ECDSASign>();
  register_legacy_shard::<ECDSAPubKey>();
  register_legacy_shard::<ECDSAPrivKey>();
  register_legacy_shard::<ECDSARecover>();
}
