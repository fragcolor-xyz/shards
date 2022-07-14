/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2021 Fragcolor Pte. Ltd. */

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
use crate::CString;
use crate::Types;
use crate::Var;
use std::convert::TryInto;

lazy_static! {
  static ref INPUT_TYPES: Vec<Type> = vec![common_type::string,];
}

#[derive(Default)]
struct Browse {}

impl Shard for Browse {
  fn registerName() -> &'static str {
    cstr!("Browse")
  }
  fn hash() -> u32 {
    compile_time_crc32::crc32!("Browse-rust-0x20200101")
  }
  fn name(&mut self) -> &str {
    "Browse"
  }
  fn inputTypes(&mut self) -> &std::vec::Vec<Type> {
    &INPUT_TYPES
  }
  fn outputTypes(&mut self) -> &std::vec::Vec<Type> {
    &INPUT_TYPES
  }
  fn activate(&mut self, _: &Context, input: &Var) -> Result<Var, &str> {
    webbrowser::open(input.try_into()?).map_err(|e| {
      shlog!("{}", e);
      "Failed to browse."
    })?;
    Ok(*input)
  }
}

pub fn registerShards() {
  registerShard::<Browse>();
}
