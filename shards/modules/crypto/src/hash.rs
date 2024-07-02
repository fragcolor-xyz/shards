/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2021 Fragcolor Pte. Ltd. */

use shards::core::hash_var;
use shards::core::register_legacy_shard;
use shards::shard::LegacyShard;
use shards::types::common_type;

use shards::types::Context;

use shards::types::Seq;

use shards::types::Type;
use shards::types::BYTES_TYPES;

use shards::types::Var;

use sha2::{Digest as Sha2Digest, Sha256, Sha512};

use std::convert::TryInto;
use std::ffi::c_void;
use std::mem::transmute;

use byteorder::{ByteOrder, LittleEndian};
use std::hash::Hasher;
use tiny_keccak::{Hasher as KeccakHasher, Keccak, Sha3};

#[inline(always)]
fn blake2<const N: usize>(data: &[u8]) -> [u8; N] {
  blake2b_simd::Params::new()
    .hash_length(N)
    .hash(data)
    .as_bytes()
    .try_into()
    .expect("slice is always the necessary length")
}

/// Do a Blake2 512-bit hash and place result in `dest`.
pub fn blake2_512_into(data: &[u8], dest: &mut [u8; 64]) {
  *dest = blake2(data);
}

/// Do a Blake2 512-bit hash and return result.
pub fn blake2_512(data: &[u8]) -> [u8; 64] {
  blake2(data)
}

/// Do a Blake2 256-bit hash and return result.
pub fn blake2_256(data: &[u8]) -> [u8; 32] {
  blake2(data)
}

/// Do a Blake2 128-bit hash and return result.
pub fn blake2_128(data: &[u8]) -> [u8; 16] {
  blake2(data)
}

extern "C" {
  pub fn hash_bytes_xx128(data: *const u8, len: usize, seed: u64) -> Var;
  pub fn hash_bytes_xx64(data: *const u8, len: usize, seed: u64) -> Var;
  pub fn hash_bytes_xx64_legacy(data: *const u8, len: usize, seed: u64) -> Var;
}

/// Do a XX 64-bit hash and place result in `dest`.
pub fn twox_64_into(data: &[u8], dest: &mut [u8; 8]) {
  let hash = unsafe { hash_bytes_xx64_legacy(data.as_ptr(), data.len(), 0) };
  let value = unsafe { transmute(hash.payload.__bindgen_anon_1.intValue) };
  LittleEndian::write_u64(&mut dest[0..8], value);
}

/// Do a XX 64-bit hash and return result.
pub fn twox_64(data: &[u8]) -> [u8; 8] {
  let mut r: [u8; 8] = [0; 8];
  twox_64_into(data, &mut r);
  r
}

/// Do a XX 128-bit hash and place result in `dest`.
pub fn twox_128_into(data: &[u8], dest: &mut [u8; 16]) {
  // run legacy twice with seed at 0 and 1
  let hash0 = unsafe { hash_bytes_xx64_legacy(data.as_ptr(), data.len(), 0) };
  let hash1 = unsafe { hash_bytes_xx64_legacy(data.as_ptr(), data.len(), 1) };
  let first = unsafe { transmute(hash0.payload.__bindgen_anon_1.intValue) };
  let second = unsafe { transmute(hash1.payload.__bindgen_anon_1.intValue) };
  LittleEndian::write_u64(&mut dest[0..8], first);
  LittleEndian::write_u64(&mut dest[8..16], second);
}

/// Do a XX 128-bit hash and return result.
pub fn twox_128(data: &[u8]) -> [u8; 16] {
  let mut r: [u8; 16] = [0; 16];
  twox_128_into(data, &mut r);
  r
}

/// Do a XX 128-bit hash using XXH3 and place result in `dest`.
pub fn twox_128_3_into(data: &[u8], dest: &mut [u8; 16]) {
  let hash = unsafe { hash_bytes_xx128(data.as_ptr(), data.len(), 0) };
  let low = unsafe { transmute(hash.payload.__bindgen_anon_1.int2Value[0]) };
  let high = unsafe { transmute(hash.payload.__bindgen_anon_1.int2Value[1]) };
  LittleEndian::write_u64(&mut dest[0..8], low);
  LittleEndian::write_u64(&mut dest[8..16], high);
}

/// Do a XX 128-bit hash using XXH3 and return result.
pub fn twox_128_3(data: &[u8]) -> [u8; 16] {
  let mut r: [u8; 16] = [0; 16];
  twox_128_3_into(data, &mut r);
  r
}

/// Do a XX 64-bit hash using XXH3 and place result in `dest`.
pub fn twox_64_3_into(data: &[u8], dest: &mut [u8; 8]) {
  let hash = unsafe { hash_bytes_xx64(data.as_ptr(), data.len(), 0) };
  let value = unsafe { transmute(hash.payload.__bindgen_anon_1.intValue) };
  LittleEndian::write_u64(&mut dest[0..8], value);
}

/// Do a XX 64-bit hash using XXH3 and return result.
pub fn twox_64_3(data: &[u8]) -> [u8; 8] {
  let mut r: [u8; 8] = [0; 8];
  twox_64_3_into(data, &mut r);
  r
}

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
        self.output.clear();
        self.output.extend_from_slice(k.finalize().as_slice());
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
        self.output.clear();
        self.output.extend_from_slice(&output);
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

add_hasher3!(
  SHTwoX_64,
  "Hash.XXH-64",
  "Hash.XXH-64-rust-0x20200101",
  twox_64,
  8
);

add_hasher3!(
  SHTwoX_128,
  "Hash.XXH-128",
  "Hash.XXH-128-rust-0x20200101",
  twox_128,
  16
);

add_hasher3!(
  SHTwoX_64_New,
  "Hash.XXH3-64",
  "Hash.XXH3-64-rust-0x20200101",
  twox_64_3,
  8
);

add_hasher3!(
  SHTwoX_128_New,
  "Hash.XXH3-128",
  "Hash.XXH3-128-rust-0x20200101",
  twox_128_3,
  16
);

pub fn register_shards() {
  register_legacy_shard::<Keccak_256>();
  register_legacy_shard::<Keccak_512>();
  register_legacy_shard::<SHSha3_256>();
  register_legacy_shard::<SHSha3_512>();
  register_legacy_shard::<SHSha2_256>();
  register_legacy_shard::<SHSha2_512>();
  register_legacy_shard::<SHBlake_128>();
  register_legacy_shard::<SHBlake_256>();
  register_legacy_shard::<SHTwoX_64>();
  register_legacy_shard::<SHTwoX_128>();
  register_legacy_shard::<SHTwoX_64_New>();
  register_legacy_shard::<SHTwoX_128_New>();
}
