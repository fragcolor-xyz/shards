/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2021 Fragcolor Pte. Ltd. */

use crate::core::do_blocking;
use crate::core::log;
use crate::core::registerShard;
use crate::shard::Shard;
use crate::types::common_type;
use crate::types::ClonedVar;
use crate::types::Context;
use crate::types::ParamVar;
use crate::types::Parameters;
use crate::types::Seq;
use crate::types::Table;
use crate::types::Type;
use crate::types::BYTES_TYPES;
use crate::CString;
use crate::Types;
use crate::Var;
use core::time::Duration;
use sha2::{Digest, Sha256, Sha512};
use sp_core::{blake2_128, blake2_256};
use std::convert::TryFrom;
use std::convert::TryInto;
use std::ffi::CStr;
use tiny_keccak::{Hasher, Keccak, Sha3};

lazy_static! {
  pub static ref INPUT_TYPES: Vec<Type> = vec![
    common_type::bytes,
    common_type::bytezs,
    common_type::string,
    common_type::strings
  ];
}

macro_rules! add_hasher {
  ($shard_name:ident, $name_str:literal, $hash:literal, $algo:expr, $size:literal) => {
    struct $shard_name {
      output: Vec<u8>,
    }
    impl Default for $shard_name {
      fn default() -> Self {
        $shard_name { output: Vec::new() }
      }
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
        &INPUT_TYPES
      }
      fn outputTypes(&mut self) -> &std::vec::Vec<Type> {
        &BYTES_TYPES
      }
      fn activate(&mut self, _: &Context, input: &Var) -> Result<Var, &str> {
        let mut k = $algo();
        self.output.resize($size, 0);
        if input.is_seq() {
          let s: Seq = input.try_into().unwrap();
          for val in s.iter() {
            let bytes: Result<&[u8], &str> = val.as_ref().try_into();
            if let Ok(bytes) = bytes {
              k.update(bytes);
            } else {
              let string: Result<&str, &str> = val.as_ref().try_into();
              if let Ok(string) = string {
                let bytes = string.as_bytes();
                k.update(bytes);
              }
            }
          }
        } else {
          let bytes: Result<&[u8], &str> = input.try_into();
          if let Ok(bytes) = bytes {
            k.update(bytes);
          } else {
            let string: Result<&str, &str> = input.try_into();
            if let Ok(string) = string {
              let bytes = string.as_bytes();
              k.update(bytes);
            }
          }
        }
        k.finalize(&mut self.output);
        Ok(self.output.as_slice().into())
      }
    }
  };
}

add_hasher!(
  Keccak_256,
  "Hash.Keccak-256",
  "Hash.Keccak-256-rust-0x20200101",
  Keccak::v256,
  32
);
add_hasher!(
  Keccak_512,
  "Hash.Keccak-512",
  "Hash.Keccak-512-rust-0x20200101",
  Keccak::v512,
  64
);
add_hasher!(
  SHSha3_256,
  "Hash.Sha3-256",
  "Hash.Sha3-256-rust-0x20200101",
  Sha3::v256,
  32
);
add_hasher!(
  SHSha3_512,
  "Hash.Sha3-512",
  "Hash.Sha3-512-rust-0x20200101",
  Sha3::v512,
  64
);

macro_rules! add_hasher2 {
  ($shard_name:ident, $name_str:literal, $hash:literal, $algo:expr) => {
    struct $shard_name {
      output: Vec<u8>,
    }
    impl Default for $shard_name {
      fn default() -> Self {
        $shard_name { output: Vec::new() }
      }
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
        &INPUT_TYPES
      }
      fn outputTypes(&mut self) -> &std::vec::Vec<Type> {
        &BYTES_TYPES
      }
      fn activate(&mut self, _: &Context, input: &Var) -> Result<Var, &str> {
        let mut k = $algo();
        if input.is_seq() {
          let s: Seq = input.try_into().unwrap();
          for val in s.iter() {
            let bytes: Result<&[u8], &str> = val.as_ref().try_into();
            if let Ok(bytes) = bytes {
              k.update(bytes);
            } else {
              let string: Result<&str, &str> = val.as_ref().try_into();
              if let Ok(string) = string {
                let bytes = string.as_bytes();
                k.update(bytes);
              }
            }
          }
        } else {
          let bytes: Result<&[u8], &str> = input.try_into();
          if let Ok(bytes) = bytes {
            k.update(bytes);
          } else {
            let string: Result<&str, &str> = input.try_into();
            if let Ok(string) = string {
              let bytes = string.as_bytes();
              k.update(bytes);
            }
          }
        }
        self.output = k.finalize().as_slice().into();
        Ok(self.output.as_slice().into())
      }
    }
  };
}

add_hasher2!(
  SHSha2_256,
  "Hash.Sha2-256",
  "Hash.Sha2-256-rust-0x20200101",
  Sha256::new
);
add_hasher2!(
  SHSha2_512,
  "Hash.Sha2-512",
  "Hash.Sha2-512-rust-0x20200101",
  Sha512::new
);

macro_rules! add_hasher3 {
  ($shard_name:ident, $name_str:literal, $hash:literal, $algo:expr, $size:literal) => {
    struct $shard_name {
      output: Vec<u8>,
      scratch: Vec<u8>,
    }
    impl Default for $shard_name {
      fn default() -> Self {
        $shard_name {
          output: Vec::new(),
          scratch: Vec::new(),
        }
      }
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
        &INPUT_TYPES
      }
      fn outputTypes(&mut self) -> &std::vec::Vec<Type> {
        &BYTES_TYPES
      }
      fn activate(&mut self, _: &Context, input: &Var) -> Result<Var, &str> {
        self.scratch.clear();
        if input.is_seq() {
          let s: Seq = input.try_into().unwrap();
          for val in s.iter() {
            let bytes: Result<&[u8], &str> = val.as_ref().try_into();
            if let Ok(bytes) = bytes {
              self.scratch.extend(bytes);
            } else {
              let string: Result<&str, &str> = val.as_ref().try_into();
              if let Ok(string) = string {
                let bytes = string.as_bytes();
                self.scratch.extend(bytes);
              }
            }
          }
        } else {
          let bytes: Result<&[u8], &str> = input.try_into();
          if let Ok(bytes) = bytes {
            self.scratch.extend(bytes);
          } else {
            let string: Result<&str, &str> = input.try_into();
            if let Ok(string) = string {
              let bytes = string.as_bytes();
              self.scratch.extend(bytes);
            }
          }
        }
        let output: [u8; $size] = $algo(&self.scratch);
        self.output = output[..].into();
        Ok(self.output.as_slice().into())
      }
    }
  };
}

add_hasher3!(
  SHBlake_128,
  "Hash.Blake2-128",
  "Hash.Blake2-128-rust-0x20200101",
  blake2_128,
  16
);

add_hasher3!(
  SHBlake_256,
  "Hash.Blake2-256",
  "Hash.Blake2-256-rust-0x20200101",
  blake2_256,
  32
);

pub fn registerShards() {
  registerShard::<Keccak_256>();
  registerShard::<Keccak_512>();
  registerShard::<SHSha3_256>();
  registerShard::<SHSha3_512>();
  registerShard::<SHSha2_256>();
  registerShard::<SHSha2_512>();
  registerShard::<SHBlake_128>();
  registerShard::<SHBlake_256>();
}
