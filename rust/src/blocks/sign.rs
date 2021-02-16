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

lazy_static! {
  static ref INPUT_TYPES: Vec<Type> = vec![common_type::bytes];
  static ref OUTPUT_TYPE: Vec<Type> = vec![common_type::bytes];
  static ref PARAMETERS: Parameters = vec![(
    "Key",
    "The private key to be used to sign the hashed message input.",
    vec![common_type::bytes, common_type::bytes_var, common_type::string, common_type::string_var]
  )
    .into()];
}

struct ECDSA {
  output: Option<[u8; 64]>,
  key: ParamVar,
}

impl Default for ECDSA {
  fn default() -> Self {
    ECDSA {
      output: None,
      key: ParamVar::new(().into()),
    }
  }
}

impl Block for ECDSA {
  fn registerName() -> &'static str {
    cstr!("Sign.ECDSA")
  }
  fn hash() -> u32 {
    compile_time_crc32::crc32!("Sign.ECDSA-rust-0x20200101")
  }
  fn name(&mut self) -> &str {
    "Sign.ECDSA"
  }
  fn inputTypes(&mut self) -> &std::vec::Vec<Type> {
    &INPUT_TYPES
  }
  fn outputTypes(&mut self) -> &std::vec::Vec<Type> {
    &OUTPUT_TYPE
  }
  fn parameters(&mut self) -> Option<&Parameters> {
    Some(&PARAMETERS)
  }
  fn setParam(&mut self, index: i32, value: &Var) {
    match index {
      0 => self.key.setParam(value),
      _ => unreachable!(),
    }
  }
  fn getParam(&mut self, index: i32) -> Var {
    match index {
      0 => self.key.getParam(),
      _ => unreachable!(),
    }
  }
  fn warmup(&mut self, context: &Context) -> Result<(), &str> {
    self.key.warmup(context);
    Ok(())
  }
  fn cleanup(&mut self) {
    self.key.cleanup();
  }
  fn activate(&mut self, _: &Context, input: &Var) -> Result<Var, &str> {
    let bytes: &[u8] = input.as_ref().try_into()?;
    let key_var = self.key.get();
    let key = {
      let key: Result<&[u8], &str> = key_var.as_ref().try_into();
      if let Ok(key) = key {
        secp256k1::SecretKey::parse_slice(key).map_err(|e| {
          cblog!("{}", e);
          "Failed to parse secret key"
        })
      } else {
        let key: Result<&str, &str> = key_var.as_ref().try_into();
        if let Ok(key) = key {
          let key = hex::decode(key).map_err(|e| {
            cblog!("{}", e);
            "Failed to parse key's hex string"
          })?;
          secp256k1::SecretKey::parse_slice(key.as_slice()).map_err(|e| {
            cblog!("{}", e);
            "Failed to parse secret key"
          })
        } else {
          Err("Invalid key value")
        }
      }
    }?;
    let msg = secp256k1::Message::parse_slice(bytes).map_err(|e| {
      cblog!("{}", e);
      "Failed to parse input message hash"
    })?;
    let signed = secp256k1::sign(&msg, &key);
    self.output = Some(signed.0.serialize());
    let output: &[u8] = &self.output.unwrap();
    Ok(output.into())
  }
}

pub fn registerBlocks() {
  registerBlock::<ECDSA>();
}
