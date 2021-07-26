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
use ethabi::Contract;
use ethabi::ParamType;
use ethabi::Token;
use ethereum_types::U256;
use std::convert::TryInto;

lazy_static! {
  static ref INPUT_TYPES: Vec<Type> = vec![common_type::any];
  static ref OUTPUT_TYPES: Vec<Type> = vec![common_type::bytes];
  static ref PARAMETERS: Parameters = vec![
    (
      cstr!("ABI"),
      cbccstr!("The contract's json ABI."),
      vec![
        common_type::none,
        common_type::string,
        common_type::string_var
      ]
    )
      .into(),
    (
      cstr!("Name"),
      cbccstr!("The name of the method to call."),
      vec![common_type::none, common_type::string]
    )
      .into()
  ];
}

fn var_to_token(var: Var, param_type: &ParamType) -> Result<Token, &'static str> {
  match param_type {
    ParamType::Uint(size) => {
      if *size > 64 && *size <= 256 {
        let bytes: &[u8] = var.as_ref().try_into()?;
        let u: U256 = bytes.into();
        let u: [u8; 32] = u.into();
        Ok(Token::Uint(u.into()))
      } else if *size <= 64 {
        let uint: u64 = var.as_ref().try_into()?;
        Ok(Token::Uint(uint.into()))
      } else {
        Err("Invalid Uint size")
      }
    }
    ParamType::Address => {
      let s: String = var.as_ref().try_into()?;
      Ok(Token::Address(
        s.parse().map_err(|_| "Invalid address string")?,
      ))
    }
    ParamType::Bytes => {
      let bytes: &[u8] = var.as_ref().try_into()?;
      Ok(Token::Bytes(bytes.to_vec()))
    }
    ParamType::FixedBytes(size) => {
      let bytes: &[u8] = var.as_ref().try_into()?;
      let mut u = Vec::new();
      u.resize(*size, 0);
      u[..bytes.len()].clone_from_slice(&bytes[..bytes.len()]);
      Ok(Token::FixedBytes(u))
    }
    ParamType::String => {
      let s: String = var.as_ref().try_into()?;
      Ok(Token::String(s))
    }
    ParamType::Bool => {
      let b: bool = var.as_ref().try_into()?;
      Ok(Token::Bool(b))
    }
    ParamType::Array(inner_type) => {
      if !var.is_seq() {
        Err("Expected a sequence")
      } else {
        let mut tokens = Vec::new();
        let s: Seq = var.try_into()?;
        for v in s.iter() {
          tokens.push(var_to_token(v, inner_type)?);
        }
        Ok(Token::Array(tokens))
      }
    }
    _ => Err("Failed to convert Var into Token - matched case not implemented"),
  }
}

#[derive(Default)]
struct EncodeCall {
  abi: ParamVar,
  call_name: ParamVar,
  current_abi: Option<ClonedVar>,
  contract: Option<Contract>,
  output: Vec<u8>,
  input: Vec<Token>,
}

impl Block for EncodeCall {
  fn registerName() -> &'static str {
    cstr!("Eth.EncodeCall")
  }

  fn hash() -> u32 {
    compile_time_crc32::crc32!("Eth.EncodeCall-rust-0x20200101")
  }

  fn name(&mut self) -> &str {
    "Eth.EncodeCall"
  }

  fn inputTypes(&mut self) -> &std::vec::Vec<Type> {
    &INPUT_TYPES
  }

  fn outputTypes(&mut self) -> &std::vec::Vec<Type> {
    &OUTPUT_TYPES
  }

  fn parameters(&mut self) -> Option<&Parameters> {
    Some(&PARAMETERS)
  }

  fn setParam(&mut self, index: i32, value: &Var) {
    match index {
      0 => self.abi.set_param(value),
      1 => self.call_name.set_param(value),
      _ => unreachable!(),
    }
  }

  fn getParam(&mut self, index: i32) -> Var {
    match index {
      0 => self.abi.get_param(),
      1 => self.call_name.get_param(),
      _ => unreachable!(),
    }
  }

  fn warmup(&mut self, context: &Context) -> Result<(), &str> {
    self.abi.warmup(context);
    self.call_name.warmup(context);
    Ok(())
  }

  fn cleanup(&mut self) {
    self.abi.cleanup();
    self.call_name.cleanup();
  }

  fn activate(&mut self, _: &Context, input: &Var) -> Result<Var, &str> {
    let abi = self.abi.get();
    let call_name = self.call_name.get();

    // process abi, create contract if needed
    if let Some(current_abi) = &self.current_abi {
      // abi might change on the fly, so we need to check it
      if self.abi.is_variable() && abi != current_abi.0 {
        self.contract = serde_json::from_str(abi.as_ref().try_into()?).map_err(|e| {
          cblog!("{}", e);
          "Failed to parse abi json string"
        })?;
        self.current_abi = Some(abi.into());
      }
    } else {
      self.contract = serde_json::from_str(abi.as_ref().try_into()?).map_err(|e| {
        cblog!("{}", e);
        "Failed to parse abi json string"
      })?;
      self.current_abi = Some(abi.into());
    }

    // encode the call
    if let Some(contract) = &self.contract {
      let name: String = call_name.as_ref().try_into()?;
      let func = &contract.functions[&name][0];

      let inputs: Seq = input.try_into()?;
      if inputs.len() != func.inputs.len() {
        return Err("Invalid number of input parameters");
      }

      self.input.clear();

      for (idx, input_type) in func.inputs.iter().enumerate() {
        let value = inputs[idx];
        self.input.push(var_to_token(value, &input_type.kind)?);
      }

      self.output = func.encode_input(self.input.as_slice()).map_err(|e| {
        cblog!("{}", e);
        "Failed to parse abi json string"
      })?;
      Ok(self.output.as_slice().into())
    } else {
      Err("Contract is missing")
    }
  }
}

pub fn registerBlocks() {
  registerBlock::<EncodeCall>();
}
