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
use sp_core::crypto::Pair;
use sp_core::ed25519;
use sp_core::sr25519;
use std::convert::TryFrom;
use std::convert::TryInto;
use std::ffi::CStr;

lazy_static! {
  static ref PK_TYPES: Vec<Type> = vec![common_type::bytes, common_type::string];
  static ref PARAMETERS: Parameters = vec![(
    cstr!("Key"),
    cbccstr!("The private key to be used to sign the hashed message input."),
    vec![
      common_type::bytes,
      common_type::bytes_var,
      common_type::string,
      common_type::string_var
    ]
  )
    .into()];
  static ref SIG_PARAMETERS: Parameters = vec![(
    cstr!("Signature"),
    cbccstr!(
      "The signature and recovery id generated signing the input message with the private key."
    ),
    vec![common_type::bytezs, common_type::bytess_var]
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

macro_rules! add_signer {
  ($block_name:ident, $name_str:literal, $hash:literal, $key_type:tt, $size:literal) => {
    #[derive(Default)]
    struct $block_name {
      output: ClonedVar,
      key: ParamVar,
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
        &BYTES_TYPES
      }

      fn outputTypes(&mut self) -> &std::vec::Vec<Type> {
        &BYTES_TYPES
      }

      fn parameters(&mut self) -> Option<&Parameters> {
        Some(&PARAMETERS)
      }

      fn setParam(&mut self, index: i32, value: &Var) {
        match index {
          0 => self.key.set_param(value),
          _ => unreachable!(),
        }
      }

      fn getParam(&mut self, index: i32) -> Var {
        match index {
          0 => self.key.get_param(),
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

        let key = self.key.get();
        let key: $key_type::Pair = get_key(key)?;

        let signature: $key_type::Signature = key.sign(bytes);
        let signature: [u8; $size] = signature.0;

        self.output = signature[..].into();
        Ok(self.output.0)
      }
    }
  };
}

add_signer!(
  Sr25519Sign,
  "Sr25519.Sign",
  "Sr25519.Sign-rust-0x20200101",
  sr25519,
  64
);

add_signer!(
  Ed25519Sign,
  "Ed25519.Sign",
  "Ed25519.Sign-rust-0x20200101",
  ed25519,
  64
);

macro_rules! add_pub_key {
  ($block_name:ident, $name_str:literal, $hash:literal, $key_type:tt, $size:literal) => {
    #[derive(Default)]
    struct $block_name {
      output: ClonedVar,
      is_string: bool,
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
        &PK_TYPES
      }

      fn outputTypes(&mut self) -> &std::vec::Vec<Type> {
        &BYTES_TYPES
      }

      fn activate(&mut self, _: &Context, input: &Var) -> Result<Var, &str> {
        let key: $key_type::Pair = get_key(*input)?;
        let key = key.public();
        let key: &[u8; $size] = key.as_array_ref();
        self.output = key[..].into();
        Ok(self.output.0)
      }
    }
  };
}

add_pub_key!(
  Sr25519PublicKey,
  "Sr25519.PublicKey",
  "Sr25519.PublicKey-rust-0x20200101",
  sr25519,
  32
);

add_pub_key!(
  Ed25519PublicKey,
  "Ed25519.PublicKey",
  "Ed25519.PublicKey-rust-0x20200101",
  ed25519,
  32
);

pub fn registerBlocks() {
  registerBlock::<Sr25519Sign>();
  registerBlock::<Ed25519Sign>();
  registerBlock::<Sr25519PublicKey>();
  registerBlock::<Ed25519PublicKey>();
}
