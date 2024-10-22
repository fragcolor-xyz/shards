use shards::core::register_shard;
use shards::shard::Shard;
use shards::types::{ClonedVar, STRING_TYPES};
use shards::types::{Context, ExposedTypes, InstanceData, Type, Types, Var};

#[derive(shards::shard)]
#[shard_info("Yaml.ToJson", "A shard that converts YAML to JSON.")]
struct YamlToJsonShard {
  #[shard_required]
  required: ExposedTypes,

  output: ClonedVar,
}

impl Default for YamlToJsonShard {
  fn default() -> Self {
    Self {
      required: ExposedTypes::new(),
      output: ClonedVar::default(),
    }
  }
}

#[shards::shard_impl]
impl Shard for YamlToJsonShard {
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
    let yaml: &str = input.try_into()?;
    // simply convert to json  using serde

    // Deserialize YAML into a serde_json::Value
    let data: serde_json::Value = serde_yml::from_str(yaml).map_err(|_| "Failed to parse YAML")?;

    // Serialize the data to a JSON string
    let json_string = serde_json::to_string(&data).map_err(|_| "Failed to serialize to JSON")?;

    self.output = json_string.into();

    Ok(Some(self.output.0))
  }
}

#[derive(shards::shard)]
#[shard_info("Yaml.FromJson", "A shard that converts JSON to YAML.")]
struct JsonToYamlShard {
  #[shard_required]
  required: ExposedTypes,

  output: ClonedVar,
}

impl Default for JsonToYamlShard {
  fn default() -> Self {
    Self {
      required: ExposedTypes::new(),
      output: ClonedVar::default(),
    }
  }
}

#[shards::shard_impl]
impl Shard for JsonToYamlShard {
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
    let json: &str = input.try_into()?;

    // Deserialize JSON into a serde_json::Value
    let data: serde_json::Value = serde_json::from_str(json).map_err(|_| "Failed to parse JSON")?;

    // Serialize the data to a YAML string
    let yaml_string = serde_yml::to_string(&data).map_err(|_| "Failed to serialize to YAML")?;

    self.output = yaml_string.into();

    Ok(Some(self.output.0))
  }
}

pub fn register_shards() {
  register_shard::<YamlToJsonShard>();
  register_shard::<JsonToYamlShard>();
}
