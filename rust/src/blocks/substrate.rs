/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2021 Fragcolor Pte. Ltd. */

use crate::block::Block;
use crate::chainblocksc::CBType_Bool;
use crate::chainblocksc::CBType_Bytes;
use crate::chainblocksc::CBType_Int;
use crate::chainblocksc::CBType_None;
use crate::chainblocksc::CBType_Seq;
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
use crate::types::ANYS_TYPES;
use crate::types::BYTES_TYPES;
use crate::types::STRINGS_TYPES;
use crate::types::STRING_TYPES;
use crate::types::TYPES_ENUMS;
use crate::CString;
use crate::Types;
use crate::Var;
use core::time::Duration;
use parity_scale_codec::{Compact, Decode, Encode, HasCompact};
use sp_core::crypto::{AccountId32, Pair, Ss58Codec};
use sp_core::storage::StorageKey;
use sp_core::{blake2_128, ed25519, sr25519, twox_128};
use std::convert::{TryFrom, TryInto};
use std::ffi::CStr;
use std::str::FromStr;

lazy_static! {
  static ref ID_PARAMETERS: Parameters = vec![(
    cstr!("Ed25519"),
    cbccstr!("If the input public key is an Ed25519 and not the default Sr25519."),
    vec![common_type::bool]
  )
    .into()];
  static ref STRINGS_OR_NONE: Vec<Type> = vec![common_type::string, common_type::none];
  static ref STRINGS_OR_NONE_TYPE: Type = Type::seq(&STRINGS_OR_NONE);
  static ref ENCODE_PARAMETERS: Parameters = vec![(
    cstr!("Types"),
    cbccstr!("The hints of types to encode, either \"i8\"/\"u8\", \"i16\"/\"u16\" etc... for int types, \"a\" for AccountId or nil for other types."),
    vec![*STRINGS_OR_NONE_TYPE]
  )
    .into()];
  static ref DECODE_PARAMETERS: Parameters = vec![(cstr!("Types"), cbccstr!("The list of types expected to decode."), vec![*TYPES_ENUMS]).into()];
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

    if strings.len() < 3 {
      return Err("Invalid input length");
    }

    self.v.clear();

    // first two strings are the map
    for i in 0..2 {
      let string: &str = strings[i].as_ref().try_into()?;
      let hash = twox_128(string.as_bytes());
      self.v.extend(&hash[..]);
    }

    // third is the key
    let string: &str = strings[2].as_ref().try_into()?;
    let bytes =
      hex::decode(string.trim_start_matches("0x").trim_start_matches("0X")).map_err(|e| {
        cblog!("{}", e);
        "Failed to parse input hex string"
      })?;
    let hash = blake2_128(&bytes);
    self.v.extend(&hash[..]);
    self.v.extend(bytes);

    // double map key
    if strings.len() == 4 {
      let string: &str = strings[3].as_ref().try_into()?;
      let bytes =
        hex::decode(string.trim_start_matches("0x").trim_start_matches("0X")).map_err(|e| {
          cblog!("{}", e);
          "Failed to parse input hex string"
        })?;
      let hash = blake2_128(&bytes);
      self.v.extend(&hash[..]);
      self.v.extend(bytes);
    }

    self.output = self.v.as_slice().into();
    Ok(self.output.0)
  }
}

#[derive(Default)]
struct CBEncode {
  output: ClonedVar,
  hints: ClonedVar,
  v: Vec<u8>,
}

fn encode_var(value: Var, hint: Var) -> Result<Vec<u8>, &'static str> {
  match value.valueType {
    CBType_String => {
      let hint: Result<&str, &str> = hint.as_ref().try_into();
      let account = if let Ok(hint) = hint {
        matches!(hint, "a")
      } else {
        false
      };
      let string: &str = value.as_ref().try_into()?;
      if account {
        let id = AccountId32::from_str(string).map_err(|_| "Invalid account id")?;
        Ok(id.encode())
      } else {
        Ok(string.encode())
      }
    }
    CBType_Int => {
      let hint: &str = hint.as_ref().try_into()?;
      let int: i64 = value.as_ref().try_into()?;
      match hint {
        "u8" => Ok(u8::encode(&int.try_into().map_err(|_| "Invalid u8")?)),
        "u16" => Ok(u16::encode(&int.try_into().map_err(|_| "Invalid u16")?)),
        "u32" => Ok(u32::encode(&int.try_into().map_err(|_| "Invalid u32")?)),
        "u64" => Ok(u64::encode(&int.try_into().map_err(|_| "Invalid u64")?)),
        "i8" => Ok(i8::encode(&int.try_into().map_err(|_| "Invalid i8")?)),
        "i16" => Ok(i16::encode(&int.try_into().map_err(|_| "Invalid i16")?)),
        "i32" => Ok(i32::encode(&int.try_into().map_err(|_| "Invalid i32")?)),
        "i64" => Ok(i64::encode(&int)),
        _ => Err("Invalid hint"),
      }
    }
    CBType_Bytes => {
      let bytes: &[u8] = value.as_ref().try_into()?;
      Ok(bytes.encode())
    }
    CBType_Bool => {
      let bool: bool = value.as_ref().try_into()?;
      Ok(bool.encode())
    }
    _ => Err("Invalid input value type"),
  }
}

impl Block for CBEncode {
  fn registerName() -> &'static str {
    cstr!("Substrate.Encode")
  }

  fn hash() -> u32 {
    compile_time_crc32::crc32!("Substrate.Encode-rust-0x20200101")
  }

  fn name(&mut self) -> &str {
    "Substrate.Encode"
  }

  fn inputTypes(&mut self) -> &Vec<Type> {
    &ANYS_TYPES
  }

  fn inputHelp(&mut self) -> OptionalString {
    OptionalString(cbccstr!("A sequence of values to SCALE encode."))
  }

  fn outputTypes(&mut self) -> &Vec<Type> {
    &BYTES_TYPES
  }

  fn outputHelp(&mut self) -> OptionalString {
    OptionalString(cbccstr!("The encoded SCALE bytes."))
  }

  fn parameters(&mut self) -> Option<&Parameters> {
    Some(&ENCODE_PARAMETERS)
  }

  fn setParam(&mut self, index: i32, value: &Var) {
    match index {
      0 => self.hints = ClonedVar(*value),
      _ => unreachable!(),
    }
  }

  fn getParam(&mut self, index: i32) -> Var {
    match index {
      0 => self.hints.0,
      _ => unreachable!(),
    }
  }

  fn activate(&mut self, _: &Context, input: &Var) -> Result<Var, &str> {
    let values: Seq = input.try_into()?;
    let hints: Seq = self.hints.0.try_into()?;

    if values.len() != hints.len() {
      return Err("Invalid input length");
    }

    self.v.clear();
    for (value, hint) in values.iter().zip(hints.iter()) {
      let encoded = encode_var(value, hint)?;
      self.v.extend(encoded);
    }

    self.output = self.v.as_slice().into();
    Ok(self.output.0)
  }
}

struct CBDecode {
  output: Seq,
  hints: ClonedVar,
}

impl Default for CBDecode {
  fn default() -> Self {
    Self {
      output: Seq::new(),
      hints: ClonedVar(Var::default()),
    }
  }
}

impl Block for CBDecode {
  fn registerName() -> &'static str {
    cstr!("Substrate.Decode")
  }

  fn hash() -> u32 {
    compile_time_crc32::crc32!("Substrate.Decode-rust-0x20200101")
  }

  fn name(&mut self) -> &str {
    "Substrate.Decode"
  }

  fn inputTypes(&mut self) -> &Vec<Type> {
    &BYTES_TYPES
  }

  fn inputHelp(&mut self) -> OptionalString {
    OptionalString(cbccstr!("The encoded SCALE bytes."))
  }

  fn outputTypes(&mut self) -> &Vec<Type> {
    &ANYS_TYPES
  }

  fn outputHelp(&mut self) -> OptionalString {
    OptionalString(cbccstr!("The decoded values."))
  }

  fn parameters(&mut self) -> Option<&Parameters> {
    Some(&DECODE_PARAMETERS)
  }

  fn setParam(&mut self, index: i32, value: &Var) {
    match index {
      0 => self.hints = ClonedVar(*value),
      _ => unreachable!(),
    }
  }

  fn getParam(&mut self, index: i32) -> Var {
    match index {
      0 => self.hints.0,
      _ => unreachable!(),
    }
  }

  fn activate(&mut self, _: &Context, input: &Var) -> Result<Var, &str> {
    let bytes: &[u8] = input.as_ref().try_into()?;
    let hints: Seq = self.hints.0.try_into()?;
    let mut offset = 0;
    self.output.clear();
    for hint in hints {
      let hint = hint.enum_value()?;
      match hint {
        1 => {
          // Bool
          let mut bytes = &bytes[offset..];
          let value = bool::decode(&mut bytes).map_err(|_| "Invalid bool")?;
          offset += 1;
          self.output.push(value.into());
        }
        2 => {
          // Int
          let mut bytes = &bytes[offset..];
          let value = i64::decode(&mut bytes).map_err(|_| "Invalid i64")?;
          offset += 8;
          self.output.push(value.into());
        }
        15 => {
          // Bytes
          let mut bytes = &bytes[offset..];
          let value = Vec::<u8>::decode(&mut bytes).map_err(|_| "Invalid bytes")?;
          offset += value.len();
          self.output.push(value.as_slice().into());
        }
        16 => {
          // String
          let mut bytes = &bytes[offset..];
          let value = String::decode(&mut bytes).map_err(|_| "Invalid string")?;
          offset += value.len();
          self.output.push(value.as_str().into());
        }
        _ => return Err("Invalid value type"),
      }
    }
    Ok(self.output.as_ref().into())
  }
}

pub fn registerBlocks() {
  registerBlock::<AccountId>();
  registerBlock::<CBStorageKey>();
  registerBlock::<CBStorageMap>();
  registerBlock::<CBEncode>();
  registerBlock::<CBDecode>();
}
