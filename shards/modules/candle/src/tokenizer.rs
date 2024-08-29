use candle_core::{Device, Tensor};
use shards::shard::Shard;
use shards::types::InstanceData;
use shards::types::{common_type, ClonedVar, Context, Type, Types, Var, FRAG_CC};
use shards::types::{
  AutoSeqVar, ExposedTypes, ParamVar, NONE_TYPES, SEQ_OF_INT_TYPES, STRING_TYPES,
};
use shards::{fourCharacterCode, ref_counted_object_type_impl, shlog_error};

use std::str::FromStr;
use tokenizers::Tokenizer;

use crate::{TensorType, TensorWrapper, TENSORTYPE_TYPE, TENSOR_TYPE};

struct TokenizerWrapper(Tokenizer);
ref_counted_object_type_impl!(TokenizerWrapper);

lazy_static! {
  pub static ref TOKENIZER_TYPE: Type = Type::object(FRAG_CC, fourCharacterCode(*b"TOKn")); // last letter used as version
  pub static ref TOKENIZER_TYPE_VEC: Vec<Type> = vec![*TOKENIZER_TYPE];
  pub static ref TOKENIZER_VAR_TYPE: Type = Type::context_variable(&TOKENIZER_TYPE_VEC);
}

#[derive(shards::shard)]
#[shard_info(
  "ML.Tokenizer",
  "Loads a tokenizer from an input json string, ready to be used for tokenizing text."
)]
pub(crate) struct MLTokenizer {
  #[shard_required]
  required: ExposedTypes,

  output: ClonedVar,
}

impl Default for MLTokenizer {
  fn default() -> Self {
    Self {
      required: ExposedTypes::new(),
      output: ClonedVar::default(),
    }
  }
}

#[shards::shard_impl]
impl Shard for MLTokenizer {
  fn input_types(&mut self) -> &Types {
    &STRING_TYPES
  }

  fn output_types(&mut self) -> &Types {
    &NONE_TYPES
  }

  fn warmup(&mut self, ctx: &Context) -> Result<(), &str> {
    self.warmup_helper(ctx)?;

    Ok(())
  }

  fn cleanup(&mut self, ctx: Option<&Context>) -> Result<(), &str> {
    self.cleanup_helper(ctx)?;

    Ok(())
  }

  fn compose(&mut self, data: &InstanceData) -> Result<Type, &str> {
    self.compose_helper(data)?;
    Ok(self.output_types()[0])
  }

  fn activate(&mut self, _context: &Context, input: &Var) -> Result<Option<Var>, &str> {
    let input_str: &str = input.try_into()?;
    let tokenizer = Tokenizer::from_str(input_str).map_err(|e| {
      shlog_error!("Failed to create tokenizer: {:?}", e);
      "Failed to create tokenizer"
    })?;
    self.output = Var::new_ref_counted(TokenizerWrapper(tokenizer), &*TOKENIZER_TYPE).into();
    Ok(Some(self.output.0))
  }
}

#[derive(shards::shard)]
#[shard_info("ML.Tokens", "Tokenizes text using a tokenizer.")]
pub(crate) struct TokensShard {
  #[shard_required]
  required: ExposedTypes,

  #[shard_param("Tokenizer", "The tokenizer to use.", [*TOKENIZER_VAR_TYPE])]
  tokenizer: ParamVar,

  #[shard_param("AddSpecialTokens", "If true, add special tokens.", [common_type::bool])]
  add_special_tokens: ClonedVar,

  #[shard_param("AsTensor", "Outputs a tensor object instead of an int sequence.", [common_type::bool])]
  as_tensor: ClonedVar,

  #[shard_param("Format", "The format of the output tensor. If As Tensor is true.", [*TENSORTYPE_TYPE])]
  format: ClonedVar,

  output_seq: AutoSeqVar,
  output_tensor: ClonedVar,
}

impl Default for TokensShard {
  fn default() -> Self {
    Self {
      required: ExposedTypes::new(),
      tokenizer: ParamVar::new(Var::default()),
      add_special_tokens: true.into(),
      as_tensor: false.into(),
      output_seq: AutoSeqVar::new(),
      output_tensor: ClonedVar::default(),
      format: TensorType::U32.into(),
    }
  }
}

#[shards::shard_impl]
impl Shard for TokensShard {
  fn input_types(&mut self) -> &Types {
    &STRING_TYPES
  }

  fn output_types(&mut self) -> &Types {
    &SEQ_OF_INT_TYPES
  }

  fn warmup(&mut self, ctx: &Context) -> Result<(), &str> {
    self.warmup_helper(ctx)?;

    Ok(())
  }

  fn cleanup(&mut self, ctx: Option<&Context>) -> Result<(), &str> {
    self.cleanup_helper(ctx)?;

    Ok(())
  }

  fn compose(&mut self, data: &InstanceData) -> Result<Type, &str> {
    self.compose_helper(data)?;

    if self.tokenizer.is_none() {
      return Err("Tokenizer is not set");
    }

    let as_tensor = self.as_tensor.as_ref().try_into()?;
    if as_tensor {
      Ok(*TENSOR_TYPE)
    } else {
      Ok(SEQ_OF_INT_TYPES[0])
    }
  }

  fn activate(&mut self, _context: &Context, input: &Var) -> Result<Option<Var>, &str> {
    let input_str: &str = input.try_into()?;

    let tokenizer = unsafe {
      &mut *Var::from_ref_counted_object::<TokenizerWrapper>(
        &self.tokenizer.get(),
        &*TOKENIZER_TYPE,
      )?
    };

    let add_special_tokens: bool = self.add_special_tokens.as_ref().try_into()?;
    let tokens = tokenizer
      .0
      .with_padding(None)
      .with_truncation(None)
      .map_err(|e| {
        shlog_error!("Failed to tokenize text: {:?}", e);
        "Failed to tokenize text"
      })?
      .encode(input_str, add_special_tokens)
      .map_err(|e| {
        shlog_error!("Failed to tokenize text: {:?}", e);
        "Failed to tokenize text"
      })?
      .get_ids()
      .to_vec();

    let as_tensor: bool = self.as_tensor.as_ref().try_into()?;
    if as_tensor {
      let format: TensorType = self.format.0.as_ref().try_into()?;
      let device = Device::Cpu; // todo add gpu support
      let token_ids = match format {
        TensorType::U32 => Tensor::new(&tokens[..], &device)
          .map_err(|e| {
            shlog_error!("Failed to create tensor: {:?}", e);
            "Failed to create tensor"
          })?
          .unsqueeze(0)
          .map_err(|e| {
            shlog_error!("Failed to unsqueeze tensor: {:?}", e);
            "Failed to unsqueeze tensor"
          }),
        TensorType::I64 => {
          let tokens: Vec<i64> = tokens.into_iter().map(|token| token as i64).collect();
          Tensor::new(&tokens[..], &device)
            .map_err(|e| {
              shlog_error!("Failed to create tensor: {:?}", e);
              "Failed to create tensor"
            })?
            .unsqueeze(0)
            .map_err(|e| {
              shlog_error!("Failed to unsqueeze tensor: {:?}", e);
              "Failed to unsqueeze tensor"
            })
        }
        _ => Err("Invalid format"),
      }?;
      self.output_tensor = Var::new_ref_counted(TensorWrapper(token_ids), &*TENSOR_TYPE).into();
      Ok(Some(self.output_tensor.0))
    } else {
      self.output_seq.0.clear();
      for token in tokens {
        let token: Var = token.into();
        self.output_seq.0.push(&token);
      }
      Ok(Some(self.output_seq.0 .0))
    }
  }
}
