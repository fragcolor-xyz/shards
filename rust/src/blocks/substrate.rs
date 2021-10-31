/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2021 Fragcolor Pte. Ltd. */

use crate::block::Block;
use crate::chainblocksc::CBType_String;
use crate::core::do_blocking;
use crate::core::log;
use crate::core::registerBlock;
use crate::types::common_type;
use crate::types::ClonedVar;
use crate::types::Context;
use crate::types::InstanceData;
use crate::types::OptionalString;
use crate::types::ParamVar;
use crate::types::Parameters;
use crate::types::Seq;
use crate::types::Table;
use crate::types::Type;
use crate::types::BYTES_TYPES;
use crate::types::STRINGS_TYPES;
use crate::types::STRING_TYPES;
use crate::CString;
use crate::Types;
use crate::Var;
use core::time::Duration;
use sp_core::crypto::{AccountId32, Pair, Ss58Codec};
use sp_core::storage::StorageKey;
use sp_core::{blake2_128, ed25519, sr25519, twox_128};
use std::convert::{TryFrom, TryInto};
use std::ffi::CStr;
// use substrate_api_client::extrinsic::codec::Compact;
// use substrate_api_client::extrinsic::xt_primitives::{
//   GenericAddress, GenericExtra, UncheckedExtrinsicV4,
// };

lazy_static! {
  static ref ID_PARAMETERS: Parameters = vec![(
    cstr!("Ed25519"),
    cbccstr!("If the input public key is an Ed25519 and not the default Sr25519."),
    vec![common_type::bool]
  )
    .into()];
}

fn get_key<T: Pair>(input: Var) -> Result<T, &'static str> {
  let key: Result<&[u8], &str> = input.as_ref().try_into();
  if let Ok(key) = key {
    T::from_seed_slice(key).map_err(|e| {
      cblog!("{:?}", e);
      "Failed to parse secret key"
    })
  } else {
    let key: Result<&str, &str> = input.as_ref().try_into();
    if let Ok(key) = key {
      T::from_string(key, None).map_err(|e| {
        cblog!("{:?}", e);
        "Failed to parse secret key"
      })
    } else {
      Err("Invalid key value")
    }
  }
}

#[derive(Default)]
struct AccountId {
  output: ClonedVar,
  is_ed: bool,
}

impl Block for AccountId {
  fn registerName() -> &'static str {
    cstr!("Substrate.AccountId")
  }

  fn hash() -> u32 {
    compile_time_crc32::crc32!("Substrate.AccountId-rust-0x20200101")
  }

  fn name(&mut self) -> &str {
    "Substrate.AccountId"
  }

  fn inputTypes(&mut self) -> &Vec<Type> {
    &BYTES_TYPES
  }

  fn inputHelp(&mut self) -> OptionalString {
    OptionalString(cbccstr!(
      "The public key bytes, must be a valid Sr25519 or Ed25519 public key."
    ))
  }

  fn outputTypes(&mut self) -> &Vec<Type> {
    &STRING_TYPES
  }

  fn outputHelp(&mut self) -> OptionalString {
    OptionalString(cbccstr!("The Substrate account id in SS58 format."))
  }

  fn parameters(&mut self) -> Option<&Parameters> {
    Some(&ID_PARAMETERS)
  }

  fn setParam(&mut self, index: i32, value: &Var) {
    match index {
      0 => self.is_ed = value.try_into().unwrap(),
      _ => unreachable!(),
    }
  }

  fn getParam(&mut self, index: i32) -> Var {
    match index {
      0 => self.is_ed.into(),
      _ => unreachable!(),
    }
  }

  fn activate(&mut self, _: &Context, input: &Var) -> Result<Var, &str> {
    let bytes: &[u8] = input.as_ref().try_into()?;
    let raw: [u8; 32] = bytes.try_into().map_err(|_| "Invalid key length")?;
    let id: String = if self.is_ed {
      let key = ed25519::Public::from_raw(raw);
      key.to_ss58check()
    } else {
      let key = sr25519::Public::from_raw(raw);
      key.to_ss58check()
    };
    self.output = id.into();
    Ok(self.output.0)
  }
}

#[derive(Default)]
struct CBStorageKey {
  output: ClonedVar,
  v: Vec<u8>,
}

impl Block for CBStorageKey {
  fn registerName() -> &'static str {
    cstr!("Substrate.StorageKey")
  }

  fn hash() -> u32 {
    compile_time_crc32::crc32!("Substrate.StorageKey-rust-0x20200101")
  }

  fn name(&mut self) -> &str {
    "Substrate.StorageKey"
  }

  fn inputTypes(&mut self) -> &Vec<Type> {
    &STRINGS_TYPES
  }

  fn inputHelp(&mut self) -> OptionalString {
    OptionalString(cbccstr!(
      "A sequence of two strings representing the key we want to encode."
    ))
  }

  fn outputTypes(&mut self) -> &Vec<Type> {
    &BYTES_TYPES
  }

  fn outputHelp(&mut self) -> OptionalString {
    OptionalString(cbccstr!("The encoded storage key."))
  }

  fn activate(&mut self, _: &Context, input: &Var) -> Result<Var, &str> {
    let strings: Seq = input.try_into()?;
    if strings.len() != 2 {
      return Err("Invalid input length");
    }
    self.v.clear();
    for string in strings {
      let string: &str = string.as_ref().try_into()?;
      let hash = twox_128(string.as_bytes());
      self.v.extend(&hash[..]);
    }
    self.output = self.v.as_slice().into();
    Ok(self.output.0)
  }
}

#[derive(Default)]
struct CBStorageMap {
  output: ClonedVar,
  v: Vec<u8>,
}

impl Block for CBStorageMap {
  fn registerName() -> &'static str {
    cstr!("Substrate.StorageMap")
  }

  fn hash() -> u32 {
    compile_time_crc32::crc32!("Substrate.StorageMap-rust-0x20200101")
  }

  fn name(&mut self) -> &str {
    "Substrate.StorageMap"
  }

  fn inputTypes(&mut self) -> &Vec<Type> {
    &STRINGS_TYPES
  }

  fn inputHelp(&mut self) -> OptionalString {
    OptionalString(cbccstr!(
      "A sequence of three strings representing the map we want to encode, the last one being SCALE encoded data of the key."
    ))
  }

  fn outputTypes(&mut self) -> &Vec<Type> {
    &BYTES_TYPES
  }

  fn outputHelp(&mut self) -> OptionalString {
    OptionalString(cbccstr!("The encoded storage map."))
  }

  fn activate(&mut self, _: &Context, input: &Var) -> Result<Var, &str> {
    let strings: Seq = input.try_into()?;
    if strings.len() != 3 {
      return Err("Invalid input length");
    }
    self.v.clear();
    for i in 0..2 {
      let string: &str = strings[i].as_ref().try_into()?;
      let hash = twox_128(string.as_bytes());
      self.v.extend(&hash[..]);
    }
    let string: &str = strings[2].as_ref().try_into()?;
    let bytes =
      hex::decode(string.trim_start_matches("0x").trim_start_matches("0X")).map_err(|e| {
        cblog!("{}", e);
        "Failed to parse input hex string"
      })?;
    let hash = blake2_128(&bytes);
    self.v.extend(&hash[..]);
    self.v.extend(bytes);
    self.output = self.v.as_slice().into();
    Ok(self.output.0)
  }
}

pub fn registerBlocks() {
  registerBlock::<AccountId>();
  registerBlock::<CBStorageKey>();
  registerBlock::<CBStorageMap>();
}
