/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2020 Fragcolor Pte. Ltd. */
use shards::types::common_type;
use shards::types::Type;

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

#[no_mangle]
pub extern "C" fn shardsRegister_crypto_crypto(core: *mut shards::shardsc::SHCore) {
  unsafe {
    shards::core::Core = core;
  }

  ecdsa::registerShards();
  hash::registerShards();
  eth::registerShards();
  substrate::registerShards();
  curve25519::registerShards();
  chachapoly::registerShards();
}
