use crate::block::Block;
use crate::core::do_blocking;
use crate::core::log;
use crate::core::registerBlock;
use crate::types::common_type;
use crate::types::ClonedVar;
use crate::types::Context;
use crate::types::ParamVar;
use crate::types::Parameters;
use crate::types::Seq;
use crate::types::Table;
use crate::types::Type;
use crate::CString;
use crate::Types;
use crate::Var;
use core::time::Duration;
use std::convert::TryFrom;
use std::convert::TryInto;
use std::ffi::CStr;
// use tiny_keccak::{Hasher, Keccak, Sha3};
use tiny_keccak::{Hasher, Keccak};

lazy_static! {
  static ref INPUT_TYPES: Vec<Type> = vec![
    common_type::bytes,
    common_type::bytezs,
    common_type::string,
    common_type::strings
  ];
  static ref OUTPUT_TYPE: Vec<Type> = vec![common_type::bytes];
}

macro_rules! add_hasher {
  ($block_name:ident, $name_str:literal, $hash:literal, $algo:expr, $size:literal) => {
    struct $block_name {
      output: Vec<u8>,
    }
    impl Default for $block_name {
      fn default() -> Self {
        $block_name { output: Vec::new() }
      }
    }
    impl Block for $block_name {
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
        &OUTPUT_TYPE
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
          let bytes: Result<&[u8], &str> = input.as_ref().try_into();
          if let Ok(bytes) = bytes {
            k.update(bytes);
          } else {
            let string: Result<&str, &str> = input.as_ref().try_into();
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

add_hasher!(Keccak256, "Hash.Keccak256", "Hash.Keccak256-rust-0x20200101", Keccak::v256, 32);
// add_hasher!(CBSha3, "Hash.Sha3", "Hash.Sha3-rust-0x20200101", Sha3::v256, 32);

pub fn registerBlocks() {
  registerBlock::<Keccak256>();
  // registerBlock::<CBSha3>();
}
