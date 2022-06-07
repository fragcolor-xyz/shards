use crate::shard::Shard;
use crate::core::do_blocking;
use crate::core::log;
use crate::core::registerShard;
use crate::types::common_type;
use crate::types::ClonedVar;
use crate::types::Context;
use crate::types::OptionalString;
use crate::types::ParamVar;
use crate::types::Parameters;
use crate::types::Seq;
use crate::types::Table;
use crate::types::Type;
use crate::types::SEQ_OF_STRINGS_TYPES;
use crate::types::STRING_TYPES;
use crate::CString;
use crate::Types;
use crate::Var;
use core::convert::TryFrom;
use core::convert::TryInto;

lazy_static! {
  static ref PARAMETERS: Parameters = vec![
    (
      cstr!("Grains"),
      shccstr!("of salt."),
      vec![common_type::int]
    )
      .into()
  ];
  static ref STR_HELP: OptionalString =
    OptionalString(shccstr!("A multiline string in CSV format."));
  static ref SEQ_HELP: OptionalString = OptionalString(shccstr!(
    "A sequence of rows, with each row being a sequence of strings."
  ));
}

struct Salt {
  grains: i32,
}

// Default CTOR
impl Default for Salt {
  fn default() -> Self {
    Salt {
      grains: 0,
    }
  }
}

// Shard CTOR
impl Shard for Salt {
  fn registerName() -> &'static str {
    cstr!("Sandbox.Salt")
  }

  fn hash() -> u32 {
    compile_time_crc32::crc32!("Sandbox.Salt-rust-0x20200101")
  }

  fn name(&mut self) -> &str {
    "Sandbox.Salt"
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

  fn setParam(&mut self, index: i32, value: &Var) {
    match index {
      0 => self.grains = value.try_into().unwrap(),
      _ => unreachable!(),
    }
  }

  fn getParam(&mut self, index: i32) -> Var {
    match index {
      0 => self.grains.into(),
      _ => unreachable!(),
    }
  }

  fn warmup(&mut self, _context: &Context) -> Result<(), &str> {
    shlog!("Pouring some salt in the sandbox (WILD!)");
    Ok(())
  }

  fn cleanup(&mut self) {
  }

  fn activate(&mut self, _: &Context, _input: &Var) -> Result<Var, &str> {
    shlog!("Salt activate()-d");
    Ok(().into()) // impl From<()> for Var
  }
}

pub fn registerShards() {
  registerShard::<Salt>();
}
