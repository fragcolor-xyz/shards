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

use chrono::{DateTime, LocalResult, NaiveDate, NaiveDateTime, TimeZone, Utc};
use shards::types::Var;

use core::convert::TryInto;

lazy_static! {
  static ref FORMAT_PARAMETERS: Parameters = vec![
    (
      cstr!("Format"),
      shccstr!("The actual formatting string, see full docs: https://docs.rs/chrono/0.4.19/chrono/format/strftime/index.html#specifiers"),
      STRING_TYPES_SLICE
    )
      .into(),
  ];
  static ref FORMAT_IN_HELP: OptionalString =
    OptionalString(shccstr!("An epoch timestamp (seconds after epoch)."));
  static ref FORMAT_OUT_HELP: OptionalString = OptionalString(shccstr!(
    "A formatted readable string."
  ));

  static ref PARSE_PARAMETERS: Parameters = vec![
    (
      cstr!("Format"),
      shccstr!("An optional strftime format string. If not set, the shard auto-detects common formats (RFC 3339, ISO 8601, and other common date/time formats)."),
      STRING_TYPES_SLICE
    )
      .into(),
  ];
  static ref PARSE_IN_HELP: OptionalString =
    OptionalString(shccstr!("A date/time string to parse."));
  static ref PARSE_OUT_HELP: OptionalString = OptionalString(shccstr!(
    "The epoch timestamp in seconds (UTC)."
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
    compile_time_crc32::crc32!("Date.Format-rust-0x20250822")
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
    *FORMAT_IN_HELP
  }

  fn outputHelp(&mut self) -> OptionalString {
    *FORMAT_OUT_HELP
  }

  fn inputTypes(&mut self) -> &std::vec::Vec<Type> {
    &INT_TYPES
  }

  fn outputTypes(&mut self) -> &std::vec::Vec<Type> {
    &STRING_TYPES
  }

  fn parameters(&mut self) -> Option<&Parameters> {
    Some(&FORMAT_PARAMETERS)
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

// Common date/time formats to try, ordered from most specific to least.
// Formats with timezone info first, then naive formats (assumed UTC), then date-only.
const AUTO_FORMATS: &[&str] = &[
  // ISO 8601 / RFC 3339 with timezone
  "%Y-%m-%dT%H:%M:%S%.f%:z", // 2024-03-17T10:30:00.123+00:00
  "%Y-%m-%dT%H:%M:%S%:z",    // 2024-03-17T10:30:00+00:00
  "%Y-%m-%d %H:%M:%S%:z",    // 2024-03-17 10:30:00+00:00
  "%Y-%m-%dT%H:%M:%S%.fZ",   // 2024-03-17T10:30:00.123Z
  "%Y-%m-%dT%H:%M:%SZ",      // 2024-03-17T10:30:00Z
  "%Y-%m-%d %H:%M:%SZ",      // 2024-03-17 10:30:00Z
];

const AUTO_FORMATS_NAIVE: &[&str] = &[
  // ISO 8601 without timezone (assumed UTC)
  "%Y-%m-%dT%H:%M:%S%.f", // 2024-03-17T10:30:00.123
  "%Y-%m-%dT%H:%M:%S",    // 2024-03-17T10:30:00
  "%Y-%m-%d %H:%M:%S%.f", // 2024-03-17 10:30:00.123
  "%Y-%m-%d %H:%M:%S",    // 2024-03-17 10:30:00
  // Common human-readable formats
  "%b %d, %Y %H:%M:%S",   // Mar 17, 2024 10:30:00
  "%B %d, %Y %H:%M:%S",   // March 17, 2024 10:30:00
  "%d %b %Y %H:%M:%S",    // 17 Mar 2024 10:30:00
  "%d %B %Y %H:%M:%S",    // 17 March 2024 10:30:00
  "%a %b %e %T %Y",       // Sun Mar 17 10:30:00 2024 (matches Date.Format default)
  "%Y/%m/%d %H:%M:%S",    // 2024/03/17 10:30:00
  "%m/%d/%Y %H:%M:%S",    // 03/17/2024 10:30:00
];

const AUTO_FORMATS_DATE_ONLY: &[&str] = &[
  "%Y-%m-%d",   // 2024-03-17
  "%Y/%m/%d",   // 2024/03/17
  "%b %d, %Y",  // Mar 17, 2024
  "%B %d, %Y",  // March 17, 2024
  "%d %b %Y",   // 17 Mar 2024
  "%d %B %Y",   // 17 March 2024
  "%m/%d/%Y",   // 03/17/2024
];

fn try_parse_auto(input: &str) -> Result<i64, &'static str> {
  // First try RFC 3339 via chrono's dedicated parser (handles fractional seconds, Z and offsets)
  if let Ok(dt) = DateTime::parse_from_rfc3339(input) {
    return Ok(dt.timestamp());
  }

  // Try formats with timezone info
  for fmt in AUTO_FORMATS {
    if let Ok(dt) = DateTime::parse_from_str(input, fmt) {
      return Ok(dt.timestamp());
    }
  }

  // Try naive formats (no timezone, assume UTC)
  for fmt in AUTO_FORMATS_NAIVE {
    if let Ok(dt) = NaiveDateTime::parse_from_str(input, fmt) {
      return Ok(dt.and_utc().timestamp());
    }
  }

  // Try date-only formats (midnight UTC)
  for fmt in AUTO_FORMATS_DATE_ONLY {
    if let Ok(d) = NaiveDate::parse_from_str(input, fmt) {
      if let Some(dt) = d.and_hms_opt(0, 0, 0) {
        return Ok(dt.and_utc().timestamp());
      }
    }
  }

  Err("Date.Parse: could not parse date string with any known format")
}

struct DateParse {
  output: ClonedVar,
  formatting: ClonedVar,
}

impl Default for DateParse {
  fn default() -> Self {
    Self {
      output: ClonedVar::default(),
      formatting: ClonedVar::default(), // empty = auto-detect
    }
  }
}

impl LegacyShard for DateParse {
  fn registerName() -> &'static str {
    cstr!("Date.Parse")
  }

  fn hash() -> u32 {
    compile_time_crc32::crc32!("Date.Parse-rust-0x20260317")
  }

  fn name(&mut self) -> &str {
    "Date.Parse"
  }

  fn help(&mut self) -> OptionalString {
    OptionalString(shccstr!(
      "Parses a date/time string into an epoch timestamp (seconds). Auto-detects RFC 3339, ISO 8601, and other common formats. An optional Format parameter can force a specific strftime format."
    ))
  }

  fn inputHelp(&mut self) -> OptionalString {
    *PARSE_IN_HELP
  }

  fn outputHelp(&mut self) -> OptionalString {
    *PARSE_OUT_HELP
  }

  fn inputTypes(&mut self) -> &std::vec::Vec<Type> {
    &STRING_TYPES
  }

  fn outputTypes(&mut self) -> &std::vec::Vec<Type> {
    &INT_TYPES
  }

  fn parameters(&mut self) -> Option<&Parameters> {
    Some(&PARSE_PARAMETERS)
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
    let input_str: &str = input.as_ref().try_into()?;

    let epoch = if self.formatting.0.is_none() {
      // Auto-detect mode
      try_parse_auto(input_str)?
    } else {
      // Explicit format mode
      let fmt: &str = self.formatting.0.as_ref().try_into()?;
      // Try with timezone first
      if let Ok(dt) = DateTime::parse_from_str(input_str, fmt) {
        dt.timestamp()
      } else if let Ok(dt) = NaiveDateTime::parse_from_str(input_str, fmt) {
        // No timezone in format, assume UTC
        dt.and_utc().timestamp()
      } else if let Ok(d) = NaiveDate::parse_from_str(input_str, fmt) {
        // Date-only format
        d.and_hms_opt(0, 0, 0)
          .ok_or("Date.Parse: invalid date")?
          .and_utc()
          .timestamp()
      } else {
        return Err("Date.Parse: could not parse date string with the given format");
      }
    };

    self.output = Var::from(epoch).into();
    Ok(Some(self.output.0))
  }
}

pub fn register_shards() {
  register_legacy_shard::<DateFormat>();
  register_legacy_shard::<DateParse>();
}
