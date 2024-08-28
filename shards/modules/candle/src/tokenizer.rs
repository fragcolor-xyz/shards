use shards::shard::Shard;
use shards::types::InstanceData;
use shards::types::{common_type, ClonedVar, Context, Type, Types, Var, FRAG_CC};
use shards::types::{
  AutoSeqVar, ExposedTypes, ParamVar, NONE_TYPES, SEQ_OF_INT_TYPES, STRING_TYPES,
};
use shards::{fourCharacterCode, ref_counted_object_type_impl, shlog_error};

use std::str::FromStr;
use tokenizers::Tokenizer;

struct TokenizerWrapper(Tokenizer);
ref_counted_object_type_impl!(TokenizerWrapper);

lazy_static! {
  pub static ref TOKENIZER_TYPE: Type = Type::object(FRAG_CC, fourCharacterCode(*b"TOKn")); // last letter used as version
  pub static ref TOKENIZER_TYPE_VEC: Vec<Type> = vec![*TOKENIZER_TYPE];
  pub static ref TOKENIZER_VAR_TYPE: Type = Type::context_variable(&TOKENIZER_TYPE_VEC);
}

#[derive(shards::shard)]
#[shard_info(
  "Candle.Tokenizer",
  "Loads a tokenizer from an input json string, ready to be used for tokenizing text."
)]
pub(crate) struct CandleTokenizer {
  #[shard_required]
  required: ExposedTypes,

  output: ClonedVar,
}

impl Default for CandleTokenizer {
  fn default() -> Self {
    Self {
      required: ExposedTypes::new(),
      output: ClonedVar::default(),
    }
  }
}

#[shards::shard_impl]
impl Shard for CandleTokenizer {
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
#[shard_info("Candle.Tokens", "Tokenizes text using a tokenizer.")]
pub(crate) struct TokensShard {
  #[shard_required]
  required: ExposedTypes,

  #[shard_param("Tokenizer", "The tokenizer to use.", [*TOKENIZER_VAR_TYPE])]
  tokenizer: ParamVar,

  #[shard_param("AddSpecialTokens", "If true, add special tokens.", [common_type::bool])]
  add_special_tokens: ClonedVar,

  output: AutoSeqVar,
}

impl Default for TokensShard {
  fn default() -> Self {
    Self {
      required: ExposedTypes::new(),
      tokenizer: ParamVar::new(Var::default()),
      add_special_tokens: true.into(),
      output: AutoSeqVar::new(),
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

    Ok(self.output_types()[0])
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

    self.output.0.clear();
    for token in tokens {
      let token: Var = token.into();
      self.output.0.push(&token);
    }

    Ok(Some(self.output.0 .0))
  }
}
