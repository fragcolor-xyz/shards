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
use crate::types::BYTES_TYPES;
use crate::types::STRING_TYPES;
use crate::CString;
use crate::Types;
use crate::Var;
use core::time::Duration;
use std::convert::TryFrom;
use std::convert::TryInto;
use std::ffi::CStr;

lazy_static! {
  pub static ref INPUT_TYPES: Vec<Type> = vec![common_type::bytes, common_type::string,];
}

struct ToBase58 {
  output: String,
}
impl Default for ToBase58 {
  fn default() -> Self {
    ToBase58 {
      output: String::new(),
    }
  }
}
impl Block for ToBase58 {
  fn registerName() -> &'static str {
    cstr!("ToBase58")
  }

  fn hash() -> u32 {
    compile_time_crc32::crc32!("ToBase58-rust-0x20200101")
  }

  fn name(&mut self) -> &str {
    "ToBase58"
  }

  fn inputTypes(&mut self) -> &std::vec::Vec<Type> {
    &INPUT_TYPES
  }

  fn outputTypes(&mut self) -> &std::vec::Vec<Type> {
    &STRING_TYPES
  }

  fn activate(&mut self, _: &Context, input: &Var) -> Result<Var, &str> {
    let bytes: Result<&[u8], &str> = input.as_ref().try_into();
    if let Ok(bytes) = bytes {
      self.output = bs58::encode(bytes).into_string();
    } else {
      let string: Result<&str, &str> = input.as_ref().try_into();
      if let Ok(string) = string {
        self.output = bs58::encode(string).into_string();
      }
    }
    self.output.push('\0');
    Ok(self.output.as_str().into())
  }
}

struct FromBase58 {
  output: Vec<u8>,
}
impl Default for FromBase58 {
  fn default() -> Self {
    FromBase58 { output: Vec::new() }
  }
}
impl Block for FromBase58 {
  fn registerName() -> &'static str {
    cstr!("FromBase58")
  }

  fn hash() -> u32 {
    compile_time_crc32::crc32!("FromBase58-rust-0x20200101")
  }

  fn name(&mut self) -> &str {
    "FromBase58"
  }

  fn inputTypes(&mut self) -> &std::vec::Vec<Type> {
    &STRING_TYPES
  }

  fn outputTypes(&mut self) -> &std::vec::Vec<Type> {
    &BYTES_TYPES
  }

  fn activate(&mut self, _: &Context, input: &Var) -> Result<Var, &str> {
    let str_input: &str = input.as_ref().try_into()?;
    self.output = bs58::decode(str_input).into_vec().map_err(|e| {
      cblog!("{}", e);
      "Failed to decode base58"
    })?;
    Ok(self.output.as_slice().into())
  }
}

pub fn registerBlocks() {
  registerBlock::<ToBase58>();
  registerBlock::<FromBase58>();
}
