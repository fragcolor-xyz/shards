use shards::core::register_shard;
use shards::shard::Shard;
use shards::types::{ClonedVar, STRING_TYPES};
use shards::types::{Context, ExposedTypes, InstanceData, Type, Types, Var};

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

pub(crate) fn register_shards() {
  register_shard::<EnvShard>();
}