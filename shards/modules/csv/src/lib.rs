/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2021 Fragcolor Pte. Ltd. */

#[macro_use]
extern crate shards;

#[macro_use]
extern crate lazy_static;

extern crate compile_time_crc32;

use shards::core::register_legacy_shard;
use shards::shard::LegacyShard;

use shards::types::ClonedVar;
use shards::types::Context;
use shards::types::OptionalString;

use shards::types::Parameters;
use shards::types::Seq;

use shards::types::Type;
use shards::types::BOOL_TYPES_SLICE;
use shards::types::SEQ_OF_STRINGS_TYPES;
use shards::types::STRING_TYPES;
use shards::types::STRING_TYPES_SLICE;
use std::ffi::CString;

use shards::types::Var;

use core::convert::TryInto;

use ext_csv::ReaderBuilder;
use ext_csv::WriterBuilder;

lazy_static! {
  static ref PARAMETERS: Parameters = vec![
    (
      cstr!("NoHeader"),
      shccstr!("Whether the shard should parse the first row as data, instead of header."),
      BOOL_TYPES_SLICE
    )
      .into(),
    (
      cstr!("Separator"),
      shccstr!("The character to use as fields separator."),
      STRING_TYPES_SLICE
    )
      .into()
  ];
  static ref STR_HELP: OptionalString =
    OptionalString(shccstr!("A multiline string in CSV format."));
  static ref SEQ_HELP: OptionalString = OptionalString(shccstr!(
    "A sequence of rows, with each row being a sequence of strings."
  ));
}

struct CSVRead {
  output: Seq,
  no_header: bool,
  separator: ClonedVar,
}

impl Default for CSVRead {
  fn default() -> Self {
    CSVRead {
      output: Seq::new(),
      no_header: false,
      separator: {
        let sep = Var::ephemeral_string(",");
        sep.into()
      },
    }
  }
}

impl LegacyShard for CSVRead {
  fn registerName() -> &'static str {
    cstr!("CSV.Read")
  }

  fn hash() -> u32 {
    compile_time_crc32::crc32!("CSV.Read-rust-0x20200101")
  }

  fn name(&mut self) -> &str {
    "CSV.Read"
  }

  fn help(&mut self) -> OptionalString {
    OptionalString(shccstr!(
      "Reads a CSV string and outputs the data as a sequence of strings in a sequence of rows."
    ))
  }

  fn inputHelp(&mut self) -> OptionalString {
    *STR_HELP
  }

  fn outputHelp(&mut self) -> OptionalString {
    *SEQ_HELP
  }

  fn inputTypes(&mut self) -> &std::vec::Vec<Type> {
    &STRING_TYPES
  }

  fn outputTypes(&mut self) -> &std::vec::Vec<Type> {
    &SEQ_OF_STRINGS_TYPES
  }

  fn parameters(&mut self) -> Option<&Parameters> {
    Some(&PARAMETERS)
  }

  fn setParam(&mut self, index: i32, value: &Var) -> Result<(), &str> {
    match index {
      0 => Ok(self.no_header = value.try_into()?),
      1 => Ok(self.separator = value.into()),
      _ => unreachable!(),
    }
  }

  fn getParam(&mut self, index: i32) -> Var {
    match index {
      0 => self.no_header.into(),
      1 => self.separator.0,
      _ => unreachable!(),
    }
  }

  fn activate(&mut self, _: &Context, input: &Var) -> Result<Option<Var>, &str> {
    let text: &str = input.try_into()?;
    let sep: &str = self.separator.as_ref().try_into()?;
    let mut reader = ReaderBuilder::new()
      .has_headers(!self.no_header)
      .delimiter(sep.as_bytes()[0] as u8)
      .from_reader(text.as_bytes());
    for row in reader.records() {
      let record = row.map_err(|e| {
        shlog!("{}", e);
        "Failed to parse CSV"
      })?;
      let mut output = Seq::new();
      for field in record.iter() {
        let field = Var::ephemeral_string(field);
        output.push(&field);
      }
      self.output.push(&output.as_ref().into());
    }
    Ok(Some(self.output.as_ref().into()))
  }
}

struct CSVWrite {
  output: ClonedVar,
  no_header: bool,
  separator: ClonedVar,
}

impl Default for CSVWrite {
  fn default() -> Self {
    CSVWrite {
      output: ().into(),
      no_header: false,
      separator: {
        let sep = Var::ephemeral_string(",");
        sep.into()
      },
    }
  }
}

impl LegacyShard for CSVWrite {
  fn registerName() -> &'static str {
    cstr!("CSV.Write")
  }

  fn hash() -> u32 {
    compile_time_crc32::crc32!("CSV.Write-rust-0x20200101")
  }

  fn name(&mut self) -> &str {
    "CSV.Write"
  }

  fn help(&mut self) -> OptionalString {
    OptionalString(shccstr!(
      "Reads a sequence of strings in a sequence of rows and outputs the data as a CSV string."
    ))
  }

  fn inputHelp(&mut self) -> OptionalString {
    *SEQ_HELP
  }

  fn outputHelp(&mut self) -> OptionalString {
    *STR_HELP
  }

  fn inputTypes(&mut self) -> &Vec<Type> {
    &SEQ_OF_STRINGS_TYPES
  }

  fn outputTypes(&mut self) -> &Vec<Type> {
    &STRING_TYPES
  }

  fn parameters(&mut self) -> Option<&Parameters> {
    Some(&PARAMETERS)
  }

  fn setParam(&mut self, index: i32, value: &Var) -> Result<(), &str> {
    match index {
      0 => Ok(self.no_header = value.try_into()?),
      1 => Ok(self.separator = value.into()),
      _ => unreachable!(),
    }
  }

  fn getParam(&mut self, index: i32) -> Var {
    match index {
      0 => self.no_header.into(),
      1 => self.separator.0,
      _ => unreachable!(),
    }
  }

  fn activate(&mut self, _: &Context, input: &Var) -> Result<Option<Var>, &str> {
    let sep: &str = self.separator.as_ref().try_into()?;
    let mut writer = WriterBuilder::new()
      .has_headers(!self.no_header)
      .delimiter(sep.as_bytes()[0] as u8)
      .from_writer(Vec::new());
    let input: Seq = input.try_into()?;
    for row in input.iter() {
      let row: Seq = row.as_ref().try_into()?;
      let mut record = Vec::new();
      for field in row.iter() {
        let field: &str = field.as_ref().try_into()?;
        record.push(field);
      }
      writer.write_record(&record).map_err(|e| {
        shlog!("{}", e);
        "Failed to write to CSV"
      })?;
    }
    let output = writer.into_inner().map_err(|e| {
      shlog!("{}", e);
      "Failed to write to CSV"
    })?;
    let output = CString::new(output).map_err(|e| {
      shlog!("{}", e);
      "Failed to write to CSV"
    })?;
    let output: Var = output.as_ref().into();
    self.output.assign(&output);
    Ok(Some(self.output.0))
  }
}

#[no_mangle]
pub extern "C" fn shardsRegister_csv_rust(core: *mut shards::shardsc::SHCore) {
  unsafe {
    shards::core::Core = core;
  }
  register_legacy_shard::<CSVRead>();
  register_legacy_shard::<CSVWrite>();
}
