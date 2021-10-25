use crate::blocks::Type;
use crate::core::log;
use crate::core::logLevel;
use crate::types::common_type;
use crate::types::Context;
use crate::types::ParamVar;
use crate::types::Parameters;
use crate::types::RawString;
use crate::types::Seq;
use crate::types::BOOL_TYPES;
use crate::types::BYTES_TYPES;
use crate::Block;
use crate::Var;
use winterfell::{math::fields::f128::BaseElement, StarkProof};

lazy_static! {
  static ref VERIFY_PARAMETERS: Parameters = vec![(
    cstr!("Inputs"),
    cbccstr!("The public inputs of the proof to verify."),
    vec![common_type::bytezs, common_type::bytess_var]
  )
    .into()];
}

#[derive(Default)]
pub struct Verify {
  inputs: ParamVar,
}

impl Block for Verify {
  fn registerName() -> &'static str {
    cstr!("Stark.Verify")
  }

  fn hash() -> u32 {
    compile_time_crc32::crc32!("Stark.Verify-rust-0x20200101")
  }

  fn name(&mut self) -> &str {
    "Stark.Verify"
  }

  fn inputTypes(&mut self) -> &std::vec::Vec<Type> {
    &BYTES_TYPES
  }

  fn outputTypes(&mut self) -> &std::vec::Vec<Type> {
    &BOOL_TYPES
  }

  fn parameters(&mut self) -> Option<&Parameters> {
    Some(&VERIFY_PARAMETERS)
  }

  fn setParam(&mut self, index: i32, value: &Var) {
    match index {
      0 => self.inputs.set_param(value),
      _ => unreachable!(),
    }
  }

  fn getParam(&mut self, index: i32) -> Var {
    match index {
      0 => self.inputs.get_param(),
      _ => unreachable!(),
    }
  }

  fn warmup(&mut self, context: &Context) -> Result<(), &str> {
    self.inputs.warmup(context);
    Ok(())
  }

  fn cleanup(&mut self) {
    self.inputs.cleanup();
  }

  fn activate(&mut self, _: &Context, input: &Var) -> Result<Var, &str> {
    let proof: &[u8] = input.try_into()?;
    let proof = StarkProof::from_bytes(proof).map_err(|e| {
      cblog!("{}", e);
      "Invalid proof"
    })?;

    let inputs = self.inputs.get();
    let inputs: Seq = inputs.try_into()?;
    let elements: Vec<BaseElement> = Vec::new();

    // match winterfell::verify(proof, elements) {
    //   Ok(()) => Ok(true.into()),
    //   Err(e) => {
    //     cblog_debug!("Proof failed validation {}", e);
    //     Ok(false.into())
    //   }
    // }
    Ok(false.into())
  }
}
