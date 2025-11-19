use shards::core::register_shard;
use shards::simple_shard;

#[simple_shard("Yaml.ToJson", "A shard that converts YAML to JSON.")]
fn yaml_to_json(yaml: &str) -> Result<String, &'static str> {
  // Deserialize YAML into a serde_json::Value
  let data: serde_json::Value = serde_yml::from_str(yaml).map_err(|_| "Failed to parse YAML")?;

  // Serialize the data to a JSON string
  let json_string = serde_json::to_string(&data).map_err(|_| "Failed to serialize to JSON")?;

  Ok(json_string)
}

#[simple_shard("Yaml.FromJson", "A shard that converts JSON to YAML.")]
fn json_to_yaml(json: &str) -> Result<String, &'static str> {
  // Deserialize JSON into a serde_json::Value
  let data: serde_json::Value = serde_json::from_str(json).map_err(|_| "Failed to parse JSON")?;

  // Serialize the data to a YAML string
  let yaml_string = serde_yml::to_string(&data).map_err(|_| "Failed to serialize to YAML")?;

  Ok(yaml_string)
}

pub fn register_shards() {
  register_shard::<YamlToJsonShard>();
  register_shard::<YamlFromJsonShard>();
}
