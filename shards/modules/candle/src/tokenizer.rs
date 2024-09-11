use candle_core::{Device, Tensor as CandleTensor};
use shards::shard::Shard;
use shards::types::{common_type, ClonedVar, Context, Type, Types, Var, FRAG_CC};
use shards::types::{AutoSeqVar, ExposedTypes, ParamVar, SEQ_OF_INT_TYPES, STRING_TYPES};
use shards::types::{InstanceData, SeqVar};
use shards::{fourCharacterCode, ref_counted_object_type_impl, shlog_error};

use std::str::FromStr;
use tokenizers::Tokenizer as TokenizerPure;

use crate::{get_global_device, Tensor, TensorType, TENSORTYPE_TYPE, TENSOR_TYPE};

pub(crate) struct Tokenizer(TokenizerPure);
ref_counted_object_type_impl!(Tokenizer);

lazy_static! {
  pub static ref TOKENIZER_TYPE: Type = Type::object(FRAG_CC, fourCharacterCode(*b"TOKn")); // last letter used as version
  pub static ref TOKENIZER_TYPE_VEC: Vec<Type> = vec![*TOKENIZER_TYPE];
  pub static ref TOKENIZER_VAR_TYPE: Type = Type::context_variable(&TOKENIZER_TYPE_VEC);
  pub static ref INTS_OR_TENSOR_TYPES: Vec<Type> = vec![SEQ_OF_INT_TYPES[0], *TENSOR_TYPE];
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
    &TOKENIZER_TYPE_VEC
  }

  fn warmup(&mut self, ctx: &Context) -> Result<(), &str> {
    self.warmup_helper(ctx)?;
    Ok(())
  }

  fn cleanup(&mut self, ctx: Option<&Context>) -> Result<(), &str> {
    self.cleanup_helper(ctx)?;
    self.output = ClonedVar::default();
    Ok(())
  }

  fn compose(&mut self, data: &InstanceData) -> Result<Type, &str> {
    self.compose_helper(data)?;
    Ok(self.output_types()[0])
  }

  fn activate(&mut self, _context: &Context, input: &Var) -> Result<Option<Var>, &str> {
    let input_str: &str = input.try_into()?;
    let tokenizer = TokenizerPure::from_str(input_str).map_err(|e| {
      shlog_error!("Failed to create tokenizer: {:?}", e);
      "Failed to create tokenizer"
    })?;
    self.output = Var::new_ref_counted(Tokenizer(tokenizer), &*TOKENIZER_TYPE).into();
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

  #[shard_param("GPU", "If true, the output tensor will be on the GPU (if ).", [common_type::bool])]
  gpu: ClonedVar,

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
      gpu: false.into(),
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
    self.output_seq = AutoSeqVar::new();
    self.output_tensor = ClonedVar::default();
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
      &mut *Var::from_ref_counted_object::<Tokenizer>(&self.tokenizer.get(), &*TOKENIZER_TYPE)?
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
      let device = if self.gpu.as_ref().try_into()? {
        get_global_device()
      } else {
        &Device::Cpu
      };
      let token_ids = match format {
        TensorType::U32 => CandleTensor::new(&tokens[..], device)
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
          CandleTensor::new(&tokens[..], device)
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
      self.output_tensor = Var::new_ref_counted(Tensor(token_ids), &*TENSOR_TYPE).into();
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

#[derive(shards::shard)]
#[shard_info(
  "ML.Detokenize",
  "Converts token IDs or tensors back into text using a tokenizer."
)]
pub(crate) struct MLDetokenizer {
  #[shard_required]
  required: ExposedTypes,

  #[shard_param("Tokenizer", "The tokenizer to use for detokenization.", [*TOKENIZER_VAR_TYPE])]
  tokenizer: ParamVar,

  #[shard_param("SkipSpecialTokens", "If true, skip special tokens during detokenization.", [common_type::bool])]
  skip_special_tokens: ClonedVar,

  output: ClonedVar,
}

impl Default for MLDetokenizer {
  fn default() -> Self {
    Self {
      required: ExposedTypes::new(),
      tokenizer: ParamVar::new(Var::default()),
      skip_special_tokens: true.into(),
      output: ClonedVar::default(),
    }
  }
}

#[shards::shard_impl]
impl Shard for MLDetokenizer {
  fn input_types(&mut self) -> &Types {
    &INTS_OR_TENSOR_TYPES
  }

  fn output_types(&mut self) -> &Types {
    &STRING_TYPES
  }

  fn warmup(&mut self, ctx: &Context) -> Result<(), &str> {
    self.warmup_helper(ctx)?;
    Ok(())
  }

  fn cleanup(&mut self, ctx: Option<&Context>) -> Result<(), &str> {
    self.cleanup_helper(ctx)?;
    self.output = ClonedVar::default();
    Ok(())
  }

  fn compose(&mut self, data: &InstanceData) -> Result<Type, &str> {
    self.compose_helper(data)?;

    if self.tokenizer.is_none() {
      return Err("Tokenizer is not set");
    }

    Ok(STRING_TYPES[0])
  }

  fn activate(&mut self, _context: &Context, input: &Var) -> Result<Option<Var>, &str> {
    let tokenizer = unsafe {
      &mut *Var::from_ref_counted_object::<Tokenizer>(&self.tokenizer.get(), &*TOKENIZER_TYPE)?
    };

    let skip_special_tokens: bool = self.skip_special_tokens.as_ref().try_into()?;

    let token_ids: Vec<u32> = match input.is_seq() {
      true => {
        let input_seq: SeqVar = input.try_into()?;
        input_seq
          .iter()
          .map(|token| u32::try_from(&token).unwrap())
          .collect()
      }
      false => {
        let tensor = unsafe { &*Var::from_ref_counted_object::<Tensor>(input, &*TENSOR_TYPE)? };
        tensor
          .0
          .to_dtype(candle_core::DType::U32)
          .and_then(|tensor| tensor.flatten_all()?.to_vec1())
          .map_err(|e| {
            shlog_error!("Failed to convert tensor to vector: {:?}", e);
            "Failed to convert tensor to vector"
          })?
      }
      _ => return Err("Invalid input type. Must be 'Sequence' or 'Tensor'"),
    };

    let decoded_text = tokenizer
      .0
      .decode(&token_ids, skip_special_tokens)
      .map_err(|e| {
        shlog_error!("Failed to detokenize text: {:?}", e);
        "Failed to detokenize text"
      })?;

    self.output = decoded_text.into();
    Ok(Some(self.output.0))
  }
}
