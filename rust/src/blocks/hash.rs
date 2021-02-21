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

struct Keccak256 {
  output: Vec<u8>,
}

impl Default for Keccak256 {
  fn default() -> Self {
    Keccak256 { output: Vec::new() }
  }
}

impl Block for Keccak256 {
  fn registerName() -> &'static str {
    cstr!("Hash.Keccak256")
  }
  fn hash() -> u32 {
    compile_time_crc32::crc32!("Hash.Keccak256-rust-0x20200101")
  }
  fn name(&mut self) -> &str {
    "Hash.Keccak256"
  }
  fn inputTypes(&mut self) -> &std::vec::Vec<Type> {
    &INPUT_TYPES
  }
  fn outputTypes(&mut self) -> &std::vec::Vec<Type> {
    &OUTPUT_TYPE
  }
  fn activate(&mut self, _: &Context, input: &Var) -> Result<Var, &str> {
    let mut k = Keccak::v256();
    self.output.resize(32, 0);

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

pub fn registerBlocks() {
  registerBlock::<Keccak256>();
}
