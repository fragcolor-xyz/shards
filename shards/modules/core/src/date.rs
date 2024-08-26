/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2021 Fragcolor Pte. Ltd. */

use shards::core::register_legacy_shard;
use shards::shard::LegacyShard;

use shards::types::ClonedVar;
use shards::types::Context;
use shards::types::OptionalString;

use shards::types::Parameters;

use shards::types::Type;

use shards::types::INT_TYPES;

use shards::types::STRING_TYPES;
use shards::types::STRING_TYPES_SLICE;

use chrono::{LocalResult, TimeZone, Utc};
use shards::types::Var;

use core::convert::TryInto;

lazy_static! {
  static ref PARAMETERS: Parameters = vec![
    (
      cstr!("Format"),
      shccstr!("The actual formatting string, see full docs: https://docs.rs/chrono/0.4.19/chrono/format/strftime/index.html#specifiers"),
      STRING_TYPES_SLICE
    )
      .into(),
  ];
  static ref IN_HELP: OptionalString =
    OptionalString(shccstr!("An epoch timestamp (seconds after epoch)."));
  static ref OUT_HELP: OptionalString = OptionalString(shccstr!(
    "A formatted readable string."
  ));
}

struct DateFormat {
  output: ClonedVar,
  formatting: ClonedVar,
}

impl Default for DateFormat {
  fn default() -> Self {
    let default_formatting = Var::ephemeral_string("%a %b %e %T %Y");
    Self {
      output: ClonedVar::default(),
      formatting: default_formatting.into(),
    }
  }
}

impl LegacyShard for DateFormat {
  fn registerName() -> &'static str {
    cstr!("Date.Format")
  }

  fn hash() -> u32 {
    compile_time_crc32::crc32!("Date.Format-rust-0x20200101")
  }

  fn name(&mut self) -> &str {
    "Date.Format"
  }

  fn help(&mut self) -> OptionalString {
    OptionalString(shccstr!(
      "Reads an epoch timestamps and formats it into a readable string."
    ))
  }

  fn inputHelp(&mut self) -> OptionalString {
    *IN_HELP
  }

  fn outputHelp(&mut self) -> OptionalString {
    *OUT_HELP
  }

  fn inputTypes(&mut self) -> &std::vec::Vec<Type> {
    &INT_TYPES
  }

  fn outputTypes(&mut self) -> &std::vec::Vec<Type> {
    &STRING_TYPES
  }

  fn parameters(&mut self) -> Option<&Parameters> {
    Some(&PARAMETERS)
  }

  fn setParam(&mut self, index: i32, value: &Var) -> Result<(), &str> {
    match index {
      0 => Ok(self.formatting.assign(value)),
      _ => unreachable!(),
    }
  }

  fn getParam(&mut self, index: i32) -> Var {
    match index {
      0 => self.formatting.0,
      _ => unreachable!(),
    }
  }

  fn activate(&mut self, _: &Context, input: &Var) -> Result<Option<Var>, &str> {
    let time = Utc.timestamp_opt(input.try_into()?, 0);
    match time {
      LocalResult::Single(time) => {
        let formatter: &str = self.formatting.0.as_ref().try_into()?;
        let output = time.format(formatter).to_string();
        self.output.assign_string(&output.as_str());
        Ok(Some(self.output.0))
      }
      _ => Err("Date.Format: input must be a valid epoch seconds timestamp"),
    }
  }
}

pub fn register_shards() {
  register_legacy_shard::<DateFormat>();
}
