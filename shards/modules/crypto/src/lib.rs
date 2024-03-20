/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2020 Fragcolor Pte. Ltd. */
use shards::core::register_shard;
use shards::shard::Shard;
use shards::types::common_type;
use shards::types::Context;
use shards::types::ExposedInfo;
use shards::types::ExposedTypes;
use shards::types::InstanceData;
use shards::types::ParamVar;
use shards::types::Type;
use shards::types::Types;
use shards::types::{Var, ClonedVar};
use std::convert::TryInto;

#[macro_use]
extern crate shards;

#[macro_use]
extern crate lazy_static;

pub mod chachapoly;
pub mod curve25519;
pub mod ecdsa;
pub mod eth;
pub mod hash;
pub mod substrate;

static CRYPTO_KEY_TYPES: &[Type] = &[
  common_type::bytes,
  common_type::bytes_var,
  common_type::string,
  common_type::string_var,
];

static PUB_KEY_TYPES: &[Type] = &[common_type::bytes, common_type::bytes_var];

use bip39::{Mnemonic, MnemonicType, Language, Seed};

lazy_static! {
  pub static ref INPUT_TYPES: Vec<Type> = vec![common_type::int];
  pub static ref OUTPUT_TYPES: Vec<Type> = vec![common_type::string];
}

#[derive(shards::shard)]
#[shard_info("Mnemonic.Generate", "Generates a BIP39 mnemonic")]
struct MnemonicGenerate {
  output: ClonedVar,
}

impl Default for MnemonicGenerate {
  fn default() -> Self {
    Self {
      output: ClonedVar::default(),
    }
  }
}

#[shards::shard_impl]
impl Shard for MnemonicGenerate {
  fn input_types(&mut self) -> &Types {
    &INPUT_TYPES
  }

  fn output_types(&mut self) -> &Types {
    &OUTPUT_TYPES
  }

  fn warmup(&mut self, ctx: &Context) -> Result<(), &str> {
    self.warmup_helper(ctx)?;
    Ok(())
  }

  fn cleanup(&mut self, ctx: Option<&Context>) -> Result<(), &str> {
    self.cleanup_helper(ctx)?;
    Ok(())
  }

  fn activate(&mut self, context: &Context, input: &Var) -> Result<Var, &str> {
    let size: i64 = input.try_into().unwrap();
    let mnemonic_type = match size {
      12  => MnemonicType::Words12,
      15  => MnemonicType::Words15,
      18  => MnemonicType::Words18,
      21  => MnemonicType::Words21,
      24  => MnemonicType::Words24,
      _ => return Err("Invalid mnemonic size"),
    };
    let mnemonic = Mnemonic::new(mnemonic_type, Language::English);
    self.output = Var::ephemeral_string(mnemonic.phrase()).into();
    Ok(self.output.0)
  }
}

#[no_mangle]
pub extern "C" fn shardsRegister_crypto_crypto(core: *mut shards::shardsc::SHCore) {
  unsafe {
    shards::core::Core = core;
  }

  ecdsa::register_shards();
  hash::register_shards();
  eth::register_shards();
  substrate::register_shards();
  curve25519::register_shards();
  chachapoly::register_shards();
  register_shard::<MnemonicGenerate>();
}
