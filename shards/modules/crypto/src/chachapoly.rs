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

use shards::types::Type;
use shards::types::BYTES_TYPES;
use chacha20poly1305::aead::Aead;
use chacha20poly1305::{ChaCha20Poly1305, Key, Nonce, KeyInit};
use shards::types::Var;

lazy_static! {
  static ref INPUT_TYPES: Vec<Type> = vec![common_type::string, common_type::bytes,];
  static ref PARAMETERS: Parameters = vec![(
    cstr!("Key"),
    shccstr!("The private key to be used to encrypt/decrypt the input payload."),
    CRYPTO_KEY_TYPES
  )
    .into()];
}

#[derive(Default)]
struct Encrypt {
  key: ParamVar,
  output: ClonedVar,
  nonce: u64,
}

impl LegacyShard for Encrypt {
  fn registerName() -> &'static str {
    cstr!("ChaChaPoly.Encrypt")
  }

  fn hash() -> u32 {
    compile_time_crc32::crc32!("ChaChaPoly.Encrypt-rust-0x20200101")
  }

  fn name(&mut self) -> &str {
    "ChaChaPoly.Encrypt"
  }

  fn inputTypes(&mut self) -> &std::vec::Vec<Type> {
    &INPUT_TYPES
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
    self.nonce = 0;
    Ok(())
  }

  fn cleanup(&mut self, ctx: Option<&Context>) -> Result<(), &str> {
    self.key.cleanup(ctx);
    Ok(())
  }

  fn activate(&mut self, _: &Context, input: &Var) -> Result<Option<Var>, &str> {
    let input = if let Ok(bytes) = <&[u8]>::try_from(input) {
      bytes
    } else if let Ok(s) = <&str>::try_from(input) {
      s.as_bytes()
    } else {
      return Err("Invalid input type");
    };

    let key = self.key.get();

    // TODO cache me?
    let key = if let Ok(key) = <&[u8]>::try_from(key) {
      if key.len() != 32 {
        return Err("Invalid key length");
      }
      Key::from_slice(key)
    } else if let Ok(key) = <&str>::try_from(key) {
      if key.len() != 32 {
        return Err("Invalid key length");
      }
      Key::from_slice(key.as_bytes())
    } else {
      return Err("Invalid key type");
    };

    let cipher = ChaCha20Poly1305::new(key);

    self.nonce += 1;

    let nonce = self.nonce.to_be_bytes();
    let mut nonce_bytes = Nonce::default();
    nonce_bytes[..8].copy_from_slice(&nonce);

    let ciphertext = cipher
      .encrypt(&nonce_bytes, input)
      .map_err(|_| "Encryption failed")?;

    self.output.assign(&ciphertext.as_slice().into());

    Ok(Some(self.output.0))
  }
}

#[derive(Default)]
struct Decrypt {
  key: ParamVar,
  output: ClonedVar,
  nonce: u64,
}

impl LegacyShard for Decrypt {
  fn registerName() -> &'static str {
    cstr!("ChaChaPoly.Decrypt")
  }

  fn hash() -> u32 {
    compile_time_crc32::crc32!("ChaChaPoly.Decrypt-rust-0x20200101")
  }

  fn name(&mut self) -> &str {
    "ChaChaPoly.Decrypt"
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
    self.nonce = 0;
    Ok(())
  }

  fn cleanup(&mut self, ctx: Option<&Context>) -> Result<(), &str> {
    self.key.cleanup(ctx);
    Ok(())
  }

  fn activate(&mut self, _: &Context, input: &Var) -> Result<Option<Var>, &str> {
    let input = if let Ok(bytes) = <&[u8]>::try_from(input) {
      bytes
    } else {
      return Err("Invalid input type");
    };

    let key = self.key.get();

    // TODO cache me?
    let key = if let Ok(key) = <&[u8]>::try_from(key) {
      if key.len() != 32 {
        return Err("Invalid key length");
      }
      Key::from_slice(key)
    } else if let Ok(key) = <&str>::try_from(key) {
      if key.len() != 32 {
        return Err("Invalid key length");
      }
      Key::from_slice(key.as_bytes())
    } else {
      return Err("Invalid key type");
    };

    let cipher = ChaCha20Poly1305::new(key);

    self.nonce += 1;

    let nonce = self.nonce.to_be_bytes();
    let mut nonce_bytes = Nonce::default();
    nonce_bytes[..8].copy_from_slice(&nonce);

    let plaintext = cipher
      .decrypt(&nonce_bytes, input)
      .map_err(|_| "Decryption failed")?;

    self.output.assign(&plaintext.as_slice().into());

    Ok(Some(self.output.0))
  }
}

pub fn register_shards() {
  register_legacy_shard::<Encrypt>();
  register_legacy_shard::<Decrypt>();
}
