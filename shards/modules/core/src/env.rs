use shards::core::register_shard;
use shards::shard::Shard;
use shards::types::{
  AutoSeqVar, Context, ExposedTypes, InstanceData, Type, Types, Var, SEQ_OF_STRINGS_TYPES,
};
use shards::types::{ClonedVar, NONE_TYPES, STRING_TYPES};

#[derive(shards::shard)]
#[shard_info("Env", "Get environment variables.")]
struct EnvShard {
  #[shard_required]
  required: ExposedTypes,
  output: ClonedVar,
}

impl Default for EnvShard {
  fn default() -> Self {
    Self {
      required: ExposedTypes::new(),
      output: ClonedVar::default(),
    }
  }
}

#[shards::shard_impl]
impl Shard for EnvShard {
  fn input_types(&mut self) -> &Types {
    &STRING_TYPES
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
    Ok(self.output_types()[0])
  }

  fn activate(&mut self, _context: &Context, input: &Var) -> Result<Option<Var>, &str> {
    let name: &str = input.try_into()?;
    let value = std::env::var(name).unwrap_or_default();
    self.output = value.into();
    Ok(Some(self.output.0))
  }
}

#[derive(shards::shard)]
#[shard_info(
  "Env.All",
  "Get all environment variables as a sequence of key-value pairs."
)]
struct EnvAllShard {
  #[shard_required]
  required: ExposedTypes,
  output: AutoSeqVar,
}

impl Default for EnvAllShard {
  fn default() -> Self {
    Self {
      required: ExposedTypes::new(),
      output: AutoSeqVar::new(),
    }
  }
}

#[shards::shard_impl]
impl Shard for EnvAllShard {
  fn input_types(&mut self) -> &Types {
    &NONE_TYPES
  }

  fn output_types(&mut self) -> &Types {
    &SEQ_OF_STRINGS_TYPES
  }

  fn warmup(&mut self, ctx: &Context) -> Result<(), &str> {
    self.warmup_helper(ctx)?;
    Ok(())
  }

  fn cleanup(&mut self, ctx: Option<&Context>) -> Result<(), &str> {
    self.cleanup_helper(ctx)?;
    self.output = AutoSeqVar::new();
    Ok(())
  }

  fn compose(&mut self, data: &InstanceData) -> Result<Type, &str> {
    self.compose_helper(data)?;
    Ok(self.output_types()[0])
  }

  fn activate(&mut self, _context: &Context, _input: &Var) -> Result<Option<Var>, &str> {
    self.output.0.clear();

    // Get all environment variables and push them as key-value pairs
    for (key, value) in std::env::vars() {
      let mut key_value_pair = AutoSeqVar::new();
      key_value_pair.0.push(&ClonedVar::from(key).0);
      key_value_pair.0.push(&ClonedVar::from(value).0);
      self.output.0.emplace_seq(key_value_pair);
    }

    Ok(Some(self.output.0 .0))
  }
}

pub(crate) fn register_shards() {
  register_shard::<EnvShard>();
  register_shard::<EnvAllShard>();
}
