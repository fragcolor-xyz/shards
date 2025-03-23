use candle_transformers::models::bert::BertModel;
use candle_transformers::models::bert::DTYPE;
use candle_transformers::models::moondream::{self};
use candle_transformers::models::quantized_moondream;
use candle_transformers::models::whisper::{self as Whisper, Config as WhisperConfigType};
use candle_transformers::quantized_var_builder;
use shards::fourCharacterCode;
use shards::ref_counted_object_type_impl;
use shards::shard::Shard;
use shards::shlog_error;
use shards::types::common_type;
use shards::types::AutoSeqVar;
use shards::types::ExposedTypes;
use shards::types::InstanceData;
use shards::types::ParamVar;
use shards::types::SeqVar;
use shards::types::TableVar;
use shards::types::FRAG_CC;
use shards::types::IMAGE_TYPES;
use shards::types::STRING_TYPES;
use shards::types::{ClonedVar, Context, Type, Types, Var};
use std::collections::HashMap;

use candle_core::{Device, Tensor as CandleTensor};

use crate::get_global_device;
use crate::tokenizer::Tokenizer;
use crate::tokenizer::TOKENIZER_TYPE;
use crate::tokenizer::TOKENIZER_VAR_TYPE;
use crate::Tensor;
use crate::TENSORS_TYPE_VEC;
use crate::TENSOR_TYPE;
use crate::TENSOR_TYPE_VEC;

pub enum Model {
  Bert(BertModel),
  Whisper(Whisper::model::Whisper),
  WhisperQuantized(Whisper::quantized_model::Whisper),
  Moondream2(candle_transformers::models::moondream::Model),
  MoondreamQuantized(candle_transformers::models::quantized_moondream::Model),
}

ref_counted_object_type_impl!(Model);

lazy_static! {
  pub static ref MODEL_TYPE: Type = Type::object(FRAG_CC, fourCharacterCode(*b"cMOD")); // last letter used as version
  pub static ref MODEL_TYPE_VEC: Vec<Type> = vec![*MODEL_TYPE];
  pub static ref MODEL_VAR_TYPE: Type = Type::context_variable(&MODEL_TYPE_VEC);
}

#[derive(shards::shards_enum)]
#[enum_info(b"mMDL", "MLModels", "A machine learning model type and architecture.")]
pub enum ModelType {
  #[enum_value("A BERT model.")]
  Bert = 0x1,
  #[enum_value("A Whisper speech recognition model.")]
  Whisper = 0x2,
  #[enum_value("Moondream2 vision-language model.")]
  Moondream2 = 0x3,
}

#[derive(shards::shards_enum)]
#[enum_info(b"mFMT", "MLFormats", "The format of the machine learning model.")]
pub enum Formats {
  #[enum_value("GGUF")]
  GGUF = 0x1,
  #[enum_value("SafeTensor")]
  SafeTensor = 0x2,
}

#[derive(shards::shard)]
#[shard_info("ML.Model", "This shard allows you to load a machine learning model and specify its format and configuration.")]
pub(crate) struct ModelShard {
  #[shard_required]
  required: ExposedTypes,

  #[shard_param("Model", "The model to use.", MODELTYPE_TYPES)]
  model: ClonedVar,

  #[shard_param("Format", "The format of the model.", FORMATS_TYPES)]
  format: ClonedVar,

  #[shard_param(
    "Configuration",
    "The configuration of the model.",
    [common_type::any_table, common_type::any_table_var]
  )]
  configuration: ParamVar,

  #[shard_param("GPU", "Whether to use the GPU (if available).", [common_type::bool])]
  gpu: ClonedVar,

  output: ClonedVar,
}

impl Default for ModelShard {
  fn default() -> Self {
    Self {
      required: ExposedTypes::new(),
      model: ClonedVar::default(),
      format: ClonedVar::default(),
      configuration: ParamVar::default(),
      gpu: false.into(),
      output: ClonedVar::default(),
    }
  }
}

#[shards::shard_impl]
impl Shard for ModelShard {
  fn input_types(&mut self) -> &Types {
    &STRING_TYPES
  }

  fn output_types(&mut self) -> &Types {
    &MODEL_TYPE_VEC
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

    if self.model.0.is_none() {
      return Err("Model is required");
    }

    if self.format.0.is_none() {
      return Err("Format is required");
    }

    Ok(self.output_types()[0])
  }

  fn activate(&mut self, _context: &Context, input: &Var) -> Result<Option<Var>, &str> {
    let model: ModelType = self.model.0.as_ref().try_into().unwrap();
    let format: Formats = self.format.0.as_ref().try_into().unwrap();

    let model_path: &str = input.try_into()?;

    let model = match (model, format) {
      (ModelType::Bert, Formats::SafeTensor) => {
        if self.configuration.is_none() {
          return Err("Configuration is required");
        }

        let device = if self.gpu.as_ref().try_into()? {
          get_global_device()
        } else {
          &Device::Cpu
        };

        let vb = unsafe {
          candle_nn::VarBuilder::from_mmaped_safetensors(
            &[std::path::Path::new(model_path)],
            DTYPE,
            device,
          )
        }
        .map_err(|e| {
          shlog_error!("Failed to load model: {}", e);
          "Failed to load model"
        })?;
        let config: TableVar = self.configuration.get().as_ref().try_into()?;
        let config = BertConfig::try_from(&config)?;
        let model = BertModel::load(vb, &config.0).map_err(|e| {
          shlog_error!("Failed to load model: {}", e);
          "Failed to load model"
        })?;
        Model::Bert(model)
      }
      (ModelType::Whisper, Formats::SafeTensor) => {
        if self.configuration.is_none() {
          return Err("Configuration is required");
        }

        let device = if self.gpu.as_ref().try_into()? {
          get_global_device()
        } else {
          &Device::Cpu
        };

        let vb = unsafe {
          candle_nn::VarBuilder::from_mmaped_safetensors(
            &[std::path::Path::new(model_path)],
            DTYPE,
            device,
          )
        }
        .map_err(|e| {
          shlog_error!("Failed to load model: {}", e);
          "Failed to load model"
        })?;
        let config: TableVar = self.configuration.get().as_ref().try_into()?;
        let config = WhisperConfig::try_from(&config)?;
        let model = Whisper::model::Whisper::load(&vb, config.0).map_err(|e| {
          shlog_error!("Failed to load model: {}", e);
          "Failed to load model"
        })?;
        Model::Whisper(model)
      }
      (ModelType::Whisper, Formats::GGUF) => {
        if self.configuration.is_none() {
          return Err("Configuration is required");
        }

        let device = if self.gpu.as_ref().try_into()? {
          get_global_device()
        } else {
          &Device::Cpu
        };

        let vb =
          quantized_var_builder::VarBuilder::from_gguf(std::path::Path::new(model_path), device)
            .map_err(|e| {
              shlog_error!("Failed to load model: {}", e);
              "Failed to load model"
            })?;

        let config: TableVar = self.configuration.get().as_ref().try_into()?;
        let config = WhisperConfig::try_from(&config)?;
        let model = Whisper::quantized_model::Whisper::load(&vb, config.0).map_err(|e| {
          shlog_error!("Failed to load model: {}", e);
          "Failed to load model"
        })?;
        Model::WhisperQuantized(model)
      }
      (ModelType::Moondream2, Formats::SafeTensor) => {
        let device = if self.gpu.as_ref().try_into()? {
          get_global_device()
        } else {
          &Device::Cpu
        };

        let vb = unsafe {
          candle_nn::VarBuilder::from_mmaped_safetensors(
            &[std::path::Path::new(model_path)],
            DTYPE,
            device,
          )
        }
        .map_err(|e| {
          shlog_error!("Failed to load model: {}", e);
          "Failed to load model"
        })?;
        let config = MoondreamConfig::default();
        let model = moondream::Model::new(&config.0, vb).map_err(|e| {
          shlog_error!("Failed to load model: {}", e);
          "Failed to load model"
        })?;
        Model::Moondream2(model)
      }
      (ModelType::Moondream2, Formats::GGUF) => {
        let device = if self.gpu.as_ref().try_into()? {
          get_global_device()
        } else {
          &Device::Cpu
        };

        let vb =
          quantized_var_builder::VarBuilder::from_gguf(std::path::Path::new(model_path), device)
            .map_err(|e| {
              shlog_error!("Failed to load model: {}", e);
              "Failed to load model"
            })?;

        let config = MoondreamConfig::default();
        let model = quantized_moondream::Model::new(&config.0, vb).map_err(|e| {
          shlog_error!("Failed to load model: {}", e);
          "Failed to load model"
        })?;
        Model::MoondreamQuantized(model)
      }
      _ => return Err("Unsupported model/format combination"),
    };

    self.output = Var::new_ref_counted(model, &*MODEL_TYPE).into();

    Ok(Some(self.output.0))
  }
}

struct BertConfig(candle_transformers::models::bert::Config);
impl TryFrom<&TableVar> for BertConfig {
  type Error = &'static str;

  fn try_from(value: &TableVar) -> Result<Self, Self::Error> {
    let mut config_map = HashMap::new();
    for (ref key, ref value) in value.iter() {
      let key: &str = key.try_into()?;
      match key {
        "vocab_size" => {
          let value: usize = value.try_into()?;
          let value =
            serde_json::to_value(value).map_err(|_| "Failed to convert value to usize")?;
          config_map.insert("vocab_size", value);
        }
        "hidden_size" => {
          let value: usize = value.try_into()?;
          let value =
            serde_json::to_value(value).map_err(|_| "Failed to convert value to usize")?;
          config_map.insert("hidden_size", value);
        }
        "num_hidden_layers" => {
          let value: usize = value.try_into()?;
          let value =
            serde_json::to_value(value).map_err(|_| "Failed to convert value to usize")?;
          config_map.insert("num_hidden_layers", value);
        }
        "num_attention_heads" => {
          let value: usize = value.try_into()?;
          let value =
            serde_json::to_value(value).map_err(|_| "Failed to convert value to usize")?;
          config_map.insert("num_attention_heads", value);
        }
        "intermediate_size" => {
          let value: usize = value.try_into()?;
          let value =
            serde_json::to_value(value).map_err(|_| "Failed to convert value to usize")?;
          config_map.insert("intermediate_size", value);
        }
        "hidden_act" => {
          let value: &str = value.try_into()?;
          let value =
            serde_json::to_value(value).map_err(|_| "Failed to convert value to usize")?;
          config_map.insert("hidden_act", value);
        }
        "hidden_dropout_prob" => {
          let value: f64 = value.try_into()?;
          let value = serde_json::to_value(value).map_err(|_| "Failed to convert value to f64")?;
          config_map.insert("hidden_dropout_prob", value);
        }
        "max_position_embeddings" => {
          let value: usize = value.try_into()?;
          let value =
            serde_json::to_value(value).map_err(|_| "Failed to convert value to usize")?;
          config_map.insert("max_position_embeddings", value);
        }
        "type_vocab_size" => {
          let value: usize = value.try_into()?;
          let value =
            serde_json::to_value(value).map_err(|_| "Failed to convert value to usize")?;
          config_map.insert("type_vocab_size", value);
        }
        "initializer_range" => {
          let value: f64 = value.try_into()?;
          let value = serde_json::to_value(value).map_err(|_| "Failed to convert value to f64")?;
          config_map.insert("initializer_range", value);
        }
        "layer_norm_eps" => {
          let value: f64 = value.try_into()?;
          let value = serde_json::to_value(value).map_err(|_| "Failed to convert value to f64")?;
          config_map.insert("layer_norm_eps", value);
        }
        "pad_token_id" => {
          let value: usize = value.try_into()?;
          let value =
            serde_json::to_value(value).map_err(|_| "Failed to convert value to usize")?;
          config_map.insert("pad_token_id", value);
        }
        "position_embedding_type" => {
          let value: &str = value.try_into()?;
          let value =
            serde_json::to_value(value).map_err(|_| "Failed to convert value to usize")?;
          config_map.insert("position_embedding_type", value);
        }
        "use_cache" => {
          let value: bool = value.try_into()?;
          let value = serde_json::to_value(value).map_err(|_| "Failed to convert value to bool")?;
          config_map.insert("use_cache", value);
        }
        "classifier_dropout" => {
          if value.is_none() {
            config_map.insert("classifier_dropout", serde_json::Value::Null);
          } else {
            let value: f64 = value.try_into()?;
            let value =
              serde_json::to_value(value).map_err(|_| "Failed to convert value to f64")?;
            config_map.insert("classifier_dropout", value);
          }
        }
        "model_type" => {
          let value: String = value.try_into()?;
          let value =
            serde_json::to_value(value).map_err(|_| "Failed to convert value to String")?;
          config_map.insert("model_type", value);
        }
        _ => {} // just ignore it
      }
    }
    let json =
      serde_json::to_string(&config_map).map_err(|_| "Failed to convert config to JSON")?;
    let config: candle_transformers::models::bert::Config =
      serde_json::from_str(&json).map_err(|_| "Failed to convert JSON to config")?;
    Ok(BertConfig(config))
  }
}

struct WhisperConfig(WhisperConfigType);

impl TryFrom<&TableVar> for WhisperConfig {
  type Error = &'static str;

  fn try_from(value: &TableVar) -> Result<Self, Self::Error> {
    let mut config_map = HashMap::new();
    for (ref key, ref value) in value.iter() {
      let key: &str = key.try_into()?;
      match key {
        "vocab_size" => {
          let value: usize = value.try_into()?;
          let value =
            serde_json::to_value(value).map_err(|_| "Failed to convert value to usize")?;
          config_map.insert("vocab_size", value);
        }
        "num_mel_bins" => {
          let value: usize = value.try_into()?;
          let value =
            serde_json::to_value(value).map_err(|_| "Failed to convert value to usize")?;
          config_map.insert("num_mel_bins", value);
        }
        "max_source_positions" => {
          let value: usize = value.try_into()?;
          let value =
            serde_json::to_value(value).map_err(|_| "Failed to convert value to usize")?;
          config_map.insert("max_source_positions", value);
        }
        "max_target_positions" => {
          let value: usize = value.try_into()?;
          let value =
            serde_json::to_value(value).map_err(|_| "Failed to convert value to usize")?;
          config_map.insert("max_target_positions", value);
        }
        "encoder_attention_heads" => {
          let value: usize = value.try_into()?;
          let value =
            serde_json::to_value(value).map_err(|_| "Failed to convert value to usize")?;
          config_map.insert("encoder_attention_heads", value);
        }
        "encoder_layers" => {
          let value: usize = value.try_into()?;
          let value =
            serde_json::to_value(value).map_err(|_| "Failed to convert value to usize")?;
          config_map.insert("encoder_layers", value);
        }
        "decoder_attention_heads" => {
          let value: usize = value.try_into()?;
          let value =
            serde_json::to_value(value).map_err(|_| "Failed to convert value to usize")?;
          config_map.insert("decoder_attention_heads", value);
        }
        "decoder_layers" => {
          let value: usize = value.try_into()?;
          let value =
            serde_json::to_value(value).map_err(|_| "Failed to convert value to usize")?;
          config_map.insert("decoder_layers", value);
        }
        "encoder_ffn_dim" => {
          let value: usize = value.try_into()?;
          let value =
            serde_json::to_value(value).map_err(|_| "Failed to convert value to usize")?;
          config_map.insert("encoder_ffn_dim", value);
        }
        "decoder_ffn_dim" => {
          let value: usize = value.try_into()?;
          let value =
            serde_json::to_value(value).map_err(|_| "Failed to convert value to usize")?;
          config_map.insert("decoder_ffn_dim", value);
        }
        "d_model" => {
          let value: usize = value.try_into()?;
          let value =
            serde_json::to_value(value).map_err(|_| "Failed to convert value to usize")?;
          config_map.insert("d_model", value);
        }
        _ => {} // ignore unknown keys
      }
    }
    let json =
      serde_json::to_string(&config_map).map_err(|_| "Failed to convert config to JSON")?;
    let config: WhisperConfigType =
      serde_json::from_str(&json).map_err(|_| "Failed to convert JSON to config")?;
    Ok(WhisperConfig(config))
  }
}

struct MoondreamConfig(moondream::Config);

impl Default for MoondreamConfig {
  fn default() -> Self {
    Self(moondream::Config::v2())
  }
}

impl TryFrom<&TableVar> for MoondreamConfig {
  type Error = &'static str;

  fn try_from(value: &TableVar) -> Result<Self, Self::Error> {
    // Create default config
    let config = moondream::Config::v2();

    // For now, we'll just use the default config since Moondream has a fixed architecture
    // In the future, we could add support for custom configurations if needed

    Ok(MoondreamConfig(config))
  }
}

#[derive(shards::shard)]
#[shard_info("ML.Forward", "Forward a tensor through a model.")]
pub(crate) struct ForwardShard {
  #[shard_required]
  required: ExposedTypes,

  #[shard_param("Model", "The model to use.", [*MODEL_VAR_TYPE])]
  model: ParamVar,

  outputs: AutoSeqVar,
}

impl Default for ForwardShard {
  fn default() -> Self {
    Self {
      required: ExposedTypes::new(),
      model: ParamVar::default(),
      outputs: AutoSeqVar::new(),
    }
  }
}

#[shards::shard_impl]
impl Shard for ForwardShard {
  fn input_types(&mut self) -> &Types {
    &TENSORS_TYPE_VEC
  }

  fn output_types(&mut self) -> &Types {
    &TENSORS_TYPE_VEC
  }

  fn warmup(&mut self, ctx: &Context) -> Result<(), &str> {
    self.warmup_helper(ctx)?;
    Ok(())
  }

  fn cleanup(&mut self, ctx: Option<&Context>) -> Result<(), &str> {
    self.cleanup_helper(ctx)?;
    self.outputs = AutoSeqVar::new();
    Ok(())
  }

  fn compose(&mut self, data: &InstanceData) -> Result<Type, &str> {
    self.compose_helper(data)?;

    if self.model.is_none() {
      return Err("Model is required");
    }

    Ok(self.output_types()[0])
  }

  fn activate(&mut self, _context: &Context, input: &Var) -> Result<Option<Var>, &str> {
    let tensors: SeqVar = input.try_into()?;
    let model =
      unsafe { &mut *Var::from_ref_counted_object::<Model>(&self.model.get(), &*MODEL_TYPE)? };

    self.outputs.0.clear();

    match model {
      Model::Bert(model) => {
        if tensors.len() == 2 {
          let input_ids =
            unsafe { &mut *Var::from_ref_counted_object::<Tensor>(&tensors[0], &*TENSOR_TYPE)? };
          let input_type_ids =
            unsafe { &mut *Var::from_ref_counted_object::<Tensor>(&tensors[1], &*TENSOR_TYPE)? };
          let output = model
            .forward(&input_ids.0, &input_type_ids.0, None)
            .map_err(|e| {
              shlog_error!("Failed to forward: {}", e);
              "Failed to forward"
            })?;
          let output = Var::new_ref_counted(Tensor(output), &*TENSOR_TYPE);
          self.outputs.0.push(&output);
        } else {
          return Err("Invalid number of tensors");
        }
      }
      Model::Whisper(model) => {
        if tensors.len() != 1 {
          return Err("Whisper expects a single mel spectrogram tensor");
        }

        let mel =
          unsafe { &mut *Var::from_ref_counted_object::<Tensor>(&tensors[0], &*TENSOR_TYPE)? };

        let encoder_output = model.encoder.forward(&mel.0, true).map_err(|e| {
          shlog_error!("Failed to encode: {}", e);
          "Failed to encode audio"
        })?;

        let output = Var::new_ref_counted(Tensor(encoder_output), &*TENSOR_TYPE);
        self.outputs.0.push(&output);
      }
      Model::WhisperQuantized(model) => {
        if tensors.len() != 1 {
          return Err("Whisper expects a single mel spectrogram tensor");
        }

        let mel =
          unsafe { &mut *Var::from_ref_counted_object::<Tensor>(&tensors[0], &*TENSOR_TYPE)? };

        let encoder_output = model.encoder.forward(&mel.0, true).map_err(|e| {
          shlog_error!("Failed to encode: {}", e);
          "Failed to encode audio"
        })?;

        let output = Var::new_ref_counted(Tensor(encoder_output), &*TENSOR_TYPE);
        self.outputs.0.push(&output);
      }
      Model::Moondream2(model) => {
        if tensors.len() != 1 {
          return Err("Moondream expects a single image tensor");
        }

        let image =
          unsafe { &mut *Var::from_ref_counted_object::<Tensor>(&tensors[0], &*TENSOR_TYPE)? };

        let image_embeddings = image.0.apply(model.vision_encoder()).map_err(|e| {
          shlog_error!("Failed to encode image: {}", e);
          "Failed to encode image"
        })?;

        let output = Var::new_ref_counted(Tensor(image_embeddings), &*TENSOR_TYPE);
        self.outputs.0.push(&output);
      }
      Model::MoondreamQuantized(model) => {
        if tensors.len() != 1 {
          return Err("Moondream expects a single image tensor");
        }

        let image =
          unsafe { &mut *Var::from_ref_counted_object::<Tensor>(&tensors[0], &*TENSOR_TYPE)? };

        let image_embeddings = image.0.apply(model.vision_encoder()).map_err(|e| {
          shlog_error!("Failed to encode image: {}", e);
          "Failed to encode image"
        })?;

        let output = Var::new_ref_counted(Tensor(image_embeddings), &*TENSOR_TYPE);
        self.outputs.0.push(&output);
      }
    }

    Ok(Some(self.outputs.0 .0))
  }
}

#[derive(shards::shard)]
#[shard_info(
    "ML.VisionToText",
    "Complete vision-to-text pipeline using Moondream2 model. Takes an image tensor as input and outputs text based on a prompt."
)]
pub(crate) struct VisionToTextShard {
  #[shard_required]
  required: ExposedTypes,

  #[shard_param("Model", "The Moondream2 model to use.", [*MODEL_VAR_TYPE])]
  model: ParamVar,

  #[shard_param("Tokenizer", "The tokenizer to use.", [*TOKENIZER_VAR_TYPE])]
  tokenizer: ParamVar,

  #[shard_param("Prompt", "The prompt to use for the vision-to-text generation.", [common_type::string])]
  prompt: ParamVar,

  #[shard_param("Temperature", "Temperature for text generation (0.0 for deterministic output).", [common_type::float])]
  temperature: ClonedVar,

  #[shard_param("TopP", "Top-p sampling value (0.0-1.0, 0.0 to disable).", [common_type::float])]
  top_p: ClonedVar,

  #[shard_param("RepeatPenalty", "Penalty for repeating tokens (1.0 means no penalty).", [common_type::float])]
  repeat_penalty: ClonedVar,

  #[shard_param("MaxTokens", "Maximum number of tokens to generate.", [common_type::int])]
  max_tokens: ClonedVar,

  #[shard_param("GPU", "Whether to use the GPU (if available).", [common_type::bool])]
  gpu: ClonedVar,

  #[shard_param("Seed", "The seed to use for the generation.", [common_type::int, common_type::int_var])]
  seed: ParamVar,

  output: ClonedVar,
}

impl Default for VisionToTextShard {
  fn default() -> Self {
    Self {
      required: ExposedTypes::new(),
      model: ParamVar::default(),
      tokenizer: ParamVar::default(),
      prompt: ParamVar::default(),
      temperature: 0.5f32.into(),
      top_p: 0.9f32.into(),
      repeat_penalty: 1.0f32.into(),
      max_tokens: 512i64.into(),
      gpu: false.into(),
      seed: ParamVar::new(42i64.into()),
      output: ClonedVar::default(),
    }
  }
}

/// Loads an image from raw bytes using the image crate, this returns a tensor with shape
/// (3, 378, 378) suitable for Moondream model.
pub fn load_image_from_bytes(
  data: &shards::SHImage,
  device: &Device,
) -> candle_core::Result<CandleTensor> {
  let size = (data.width as usize) * (data.height as usize) * (data.channels as usize);
  let data = data.data;
  let slice = unsafe { std::slice::from_raw_parts(data, size) };
  let data = CandleTensor::from_vec(slice.to_vec(), (378, 378, 3), device)?.permute((2, 0, 1))?;
  let mean = CandleTensor::new(&[0.5f32, 0.5, 0.5], device)?.reshape((3, 1, 1))?;
  let std = CandleTensor::new(&[0.5f32, 0.5, 0.5], device)?.reshape((3, 1, 1))?;
  (data.to_dtype(candle_core::DType::F32)? / 255.)?
    .broadcast_sub(&mean)?
    .broadcast_div(&std)?
    .unsqueeze(0)
}

#[shards::shard_impl]
impl Shard for VisionToTextShard {
  fn input_types(&mut self) -> &Types {
    &IMAGE_TYPES
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

    if self.model.is_none() {
      return Err("Model is required");
    }
    if self.tokenizer.is_none() {
      return Err("Tokenizer is required");
    }
    if self.prompt.is_none() {
      return Err("Prompt is required");
    }

    Ok(STRING_TYPES[0])
  }

  fn activate(&mut self, _context: &Context, input: &Var) -> Result<Option<Var>, &str> {
    let model_var = self.model.get();
    let model = unsafe { &mut *Var::from_ref_counted_object::<Model>(&model_var, &*MODEL_TYPE)? };

    let tokenizer_var = self.tokenizer.get();
    let tokenizer = unsafe {
      &mut *Var::from_ref_counted_object::<crate::tokenizer::Tokenizer>(
        &tokenizer_var,
        &*TOKENIZER_TYPE,
      )?
    };

    let sh_image: &shards::SHImage = input.try_into()?;
    if sh_image.channels != 3 {
      return Err("Image must have 3 channels");
    }
    if sh_image.width != 378 || sh_image.height != 378 {
      return Err("Image must be 378x378");
    }

    let device = if bool::try_from(self.gpu.as_ref())? {
      get_global_device()
    } else {
      &Device::Cpu
    };

    let image_tensor = load_image_from_bytes(&sh_image, &device).map_err(|e| {
      shlog_error!("Failed to load image: {}", e);
      "Failed to load image"
    })?;

    // Get prompt
    let prompt: &str = self.prompt.get().as_ref().try_into()?;
    let formatted_prompt = format!("\n\nQuestion: {}\n\nAnswer:", prompt);

    // Get generation parameters
    let temperature: f32 = self.temperature.as_ref().try_into()?;
    let top_p: f32 = self.top_p.as_ref().try_into()?;
    let repeat_penalty: f32 = self.repeat_penalty.as_ref().try_into()?;
    let max_tokens: i64 = self.max_tokens.as_ref().try_into()?;

    // Process image through vision encoder
    let image_embeddings = match model {
      Model::Moondream2(model) => image_tensor.apply(model.vision_encoder()).map_err(|e| {
        shlog_error!("Failed to encode image: {}", e);
        "Failed to encode image"
      })?,
      Model::MoondreamQuantized(model) => {
        image_tensor.apply(model.vision_encoder()).map_err(|e| {
          shlog_error!("Failed to encode image: {}", e);
          "Failed to encode image"
        })?
      }
      _ => return Err("Model must be a Moondream model"),
    };

    // Tokenize the prompt
    let tokens = tokenizer.encode(&formatted_prompt, true).map_err(|e| {
      shlog_error!("Failed to tokenize prompt: {}", e);
      "Failed to tokenize prompt"
    })?;

    if tokens.is_empty() {
      return Err("Empty prompt after tokenization");
    }

    // Get the special token (BOS/EOS token)
    let special_token = match tokenizer.get_token("<|endoftext|>") {
      Ok(token) => token,
      Err(_) => return Err("BOS token not found in the tokenizer"),
    };

    // Create tensor for BOS token
    let device = image_tensor.device();
    let bos_token = CandleTensor::new(&[special_token], device)
      .map_err(|e| {
        shlog_error!("Failed to create BOS token tensor: {}", e);
        "Failed to create BOS token tensor"
      })?
      .unsqueeze(0)
      .map_err(|e| {
        shlog_error!("Failed to unsqueeze BOS token tensor: {}", e);
        "Failed to unsqueeze BOS token tensor"
      })?;

    // Create tensor for input tokens
    let input_tensor = CandleTensor::new(&*tokens, device)
      .map_err(|e| {
        shlog_error!("Failed to create input tensor: {}", e);
        "Failed to create input tensor"
      })?
      .unsqueeze(0)
      .map_err(|e| {
        shlog_error!("Failed to unsqueeze input tensor: {}", e);
        "Failed to unsqueeze input tensor"
      })?;

    // Set up logits processor for sampling
    let temp_option = if temperature <= 0.0 {
      None
    } else {
      Some(temperature as f64)
    };
    let top_p_option = if top_p <= 0.0 || top_p >= 1.0 {
      None
    } else {
      Some(top_p as f64)
    };
    let mut logits_processor = candle_transformers::generation::LogitsProcessor::new(
      self.seed.get().as_ref().try_into()?, // Fixed seed
      temp_option,
      top_p_option,
    );

    // Generate text
    let mut generated_tokens = Vec::new();
    let mut _current_token = None;

    // First forward pass with image embeddings
    let mut logits = match model {
      Model::Moondream2(model) => model
        .text_model
        .forward_with_img(&bos_token, &input_tensor, &image_embeddings)
        .map_err(|e| {
          shlog_error!("Failed in text generation: {}", e);
          "Failed in text generation"
        })?,
      Model::MoondreamQuantized(model) => model
        .text_model
        .forward_with_img(&bos_token, &input_tensor, &image_embeddings)
        .map_err(|e| {
          shlog_error!("Failed in text generation: {}", e);
          "Failed in text generation"
        })?,
      _ => return Err("Model must be a Moondream model"),
    };

    // Extract the last token's logits
    logits = logits
      .squeeze(0)
      .map_err(|e| {
        shlog_error!("Failed to squeeze logits: {}", e);
        "Failed to squeeze logits"
      })?
      .to_dtype(candle_core::DType::F32)
      .map_err(|e| {
        shlog_error!("Failed to convert logits to F32: {}", e);
        "Failed to convert logits to F32"
      })?;

    // Sample the first token
    let next_token = logits_processor.sample(&logits).map_err(|e| {
      shlog_error!("Failed to sample token: {}", e);
      "Failed to sample token"
    })?;
    generated_tokens.push(next_token);
    _current_token = Some(next_token);

    // Continue generating tokens
    for _ in 0..(max_tokens as usize - 1) {
      if let Some(token) = _current_token {
        // Check for EOS token
        if token == special_token {
          break;
        }

        // Create input tensor for the next token
        let next_input = CandleTensor::new(&[token], device)
          .map_err(|e| {
            shlog_error!("Failed to create next input tensor: {}", e);
            "Failed to create next input tensor"
          })?
          .unsqueeze(0)
          .map_err(|e| {
            shlog_error!("Failed to unsqueeze next input tensor: {}", e);
            "Failed to unsqueeze next input tensor"
          })?;

        // Forward pass for next token
        let mut logits = match model {
          Model::Moondream2(model) => model.text_model.forward(&next_input).map_err(|e| {
            shlog_error!("Failed in text generation: {}", e);
            "Failed in text generation"
          })?,
          Model::MoondreamQuantized(model) => {
            model.text_model.forward(&next_input).map_err(|e| {
              shlog_error!("Failed in text generation: {}", e);
              "Failed in text generation"
            })?
          }
          _ => return Err("Model must be a Moondream model"),
        };

        // Extract logits
        logits = logits
          .squeeze(0)
          .map_err(|e| {
            shlog_error!("Failed to squeeze logits: {}", e);
            "Failed to squeeze logits"
          })?
          .to_dtype(candle_core::DType::F32)
          .map_err(|e| {
            shlog_error!("Failed to convert logits to F32: {}", e);
            "Failed to convert logits to F32"
          })?;

        // Apply repeat penalty if needed
        let logits = if repeat_penalty == 1.0 {
          logits
        } else {
          let start_at = generated_tokens.len().saturating_sub(64); // Use last 64 tokens for penalty
          candle_transformers::utils::apply_repeat_penalty(
            &logits,
            repeat_penalty,
            &generated_tokens[start_at..],
          )
          .map_err(|e| {
            shlog_error!("Failed to apply repeat penalty: {}", e);
            "Failed to apply repeat penalty"
          })?
        };

        // Sample next token
        let next_token = logits_processor.sample(&logits).map_err(|e| {
          shlog_error!("Failed to sample token: {}", e);
          "Failed to sample token"
        })?;
        generated_tokens.push(next_token);
        _current_token = Some(next_token);
      }
    }

    // Decode the generated tokens
    let generated_text = tokenizer.decode(&generated_tokens, true).map_err(|e| {
      shlog_error!("Failed to decode tokens: {}", e);
      "Failed to decode tokens"
    })?;

    self.output = generated_text.into();
    Ok(Some(self.output.0))
  }
}

trait WhisperExt {
  fn detect_language(
    &mut self,
    mel: &CandleTensor,
    tokenizer: &mut Tokenizer,
  ) -> Result<u32, candle_core::Error>;
  fn decode(
    &mut self,
    mel: &CandleTensor,
    tokenizer: &mut Tokenizer,
    initial_tokens: &[u32],
    _beam_size: usize,
  ) -> Result<WhisperDecodeOutput, candle_core::Error>;
}

struct WhisperDecodeOutput {
  text: String,
  tokens: Vec<u32>,
}

impl WhisperExt for Whisper::model::Whisper {
  fn detect_language(
    &mut self,
    mel: &CandleTensor,
    tokenizer: &mut Tokenizer,
  ) -> Result<u32, candle_core::Error> {
    // Ensure mel is f32
    let mel = mel.to_dtype(candle_core::DType::F32)?;

    // Get encoder output first
    let audio_features = self.encoder.forward(&mel, true)?;

    // Create token tensor directly as i64
    let sot_token = tokenizer
      .get_sot_token()
      .map_err(|e| candle_core::Error::Msg(e.to_string()))?;
    let tokens_i64 = vec![sot_token as i64];
    let tokens_t = CandleTensor::from_slice(&tokens_i64, (1, 1), mel.device())?;

    // Run decoder with first iteration flag
    let ys = self.decoder.forward(&tokens_t, &audio_features, true)?;
    let logits = self.decoder.final_linear(&ys)?.squeeze(0)?;

    logits.argmax(0)?.to_scalar::<u32>()
  }

  fn decode(
    &mut self,
    mel: &CandleTensor,
    tokenizer: &mut Tokenizer,
    initial_tokens: &[u32],
    _beam_size: usize,
  ) -> Result<WhisperDecodeOutput, candle_core::Error> {
    // Ensure mel is f32
    let mel = mel.to_dtype(candle_core::DType::F32)?;

    // Get encoder output first
    let audio_features = self.encoder.forward(&mel, true)?;

    let mut tokens = initial_tokens.to_vec();
    let sample_len = self.config.max_target_positions / 2;

    for i in 0..sample_len {
      // Create tokens tensor directly
      let tokens_i64: Vec<i64> = tokens.iter().map(|&x| x as i64).collect();
      let tokens_t = CandleTensor::from_slice(&tokens_i64, (1, tokens_i64.len()), mel.device())?;

      // Forward pass with first iteration flag
      let ys = self.decoder.forward(&tokens_t, &audio_features, i == 0)?;

      // Get logits for next token prediction
      let (_, seq_len, _) = ys.dims3()?;
      let logits = self
        .decoder
        .final_linear(&ys.narrow(1, seq_len - 1, 1)?)?
        .squeeze(1)?;

      // Get next token using argmax
      let next_token = logits.argmax(1)?.squeeze(0)?.to_scalar::<u32>()?;

      if next_token
        == tokenizer
          .get_token("<|endoftext|>")
          .map_err(|e| candle_core::Error::Msg(e.to_string()))?
      {
        break;
      }
      tokens.push(next_token);
      if tokens.len() > 448 {
        // Max length
        break;
      }
    }

    let text = tokenizer.decode(&tokens, true).unwrap_or_default();
    Ok(WhisperDecodeOutput { text, tokens })
  }
}

impl WhisperExt for Whisper::quantized_model::Whisper {
  fn detect_language(
    &mut self,
    mel: &CandleTensor,
    tokenizer: &mut Tokenizer,
  ) -> Result<u32, candle_core::Error> {
    // Ensure mel is f32
    let mel = mel.to_dtype(candle_core::DType::F32)?;

    // Get encoder output first
    let audio_features = self.encoder.forward(&mel, true)?;

    // Create token tensor directly as i64
    let sot_token = tokenizer
      .get_sot_token()
      .map_err(|e| candle_core::Error::Msg(e.to_string()))?;
    let tokens_i64 = vec![sot_token as i64];
    let tokens_t = CandleTensor::from_slice(&tokens_i64, (1, 1), mel.device())?;

    // Run decoder with first iteration flag
    let ys = self.decoder.forward(&tokens_t, &audio_features, true)?;
    let logits = self.decoder.final_linear(&ys)?.squeeze(0)?;

    logits.argmax(0)?.to_scalar::<u32>()
  }

  fn decode(
    &mut self,
    mel: &CandleTensor,
    tokenizer: &mut Tokenizer,
    initial_tokens: &[u32],
    _beam_size: usize,
  ) -> Result<WhisperDecodeOutput, candle_core::Error> {
    // Ensure mel is f32
    let mel = mel.to_dtype(candle_core::DType::F32)?;

    // Get encoder output first
    let audio_features = self.encoder.forward(&mel, true)?;

    let mut tokens = initial_tokens.to_vec();
    let sample_len = self.config.max_target_positions / 2;

    for i in 0..sample_len {
      // Create tokens tensor directly
      let tokens_i64: Vec<i64> = tokens.iter().map(|&x| x as i64).collect();
      let tokens_t = CandleTensor::from_slice(&tokens_i64, (1, tokens_i64.len()), mel.device())?;

      // Forward pass with first iteration flag
      let ys = self.decoder.forward(&tokens_t, &audio_features, i == 0)?;

      // Get logits for next token prediction
      let (_, seq_len, _) = ys.dims3()?;
      let logits = self
        .decoder
        .final_linear(&ys.narrow(1, seq_len - 1, 1)?)?
        .squeeze(1)?;

      // Get next token using argmax
      let next_token = logits.argmax(1)?.squeeze(0)?.to_scalar::<u32>()?;

      if next_token
        == tokenizer
          .get_token("<|endoftext|>")
          .map_err(|e| candle_core::Error::Msg(e.to_string()))?
      {
        break;
      }
      tokens.push(next_token);
      if tokens.len() > 448 {
        // Max length
        break;
      }
    }

    let text = tokenizer.decode(&tokens, true).unwrap_or_default();
    Ok(WhisperDecodeOutput { text, tokens })
  }
}

#[derive(shards::shard)]
#[shard_info(
    "ML.SpeechToText",
    "Complete speech-to-text pipeline using Whisper model. Takes a MEL spectrogram tensor as input and outputs transcribed text."
)]
pub(crate) struct SpeechToTextShard {
  #[shard_required]
  required: ExposedTypes,

  #[shard_param("Model", "The Whisper model to use.", [*MODEL_VAR_TYPE])]
  model: ParamVar,

  #[shard_param("Tokenizer", "The tokenizer to use.", [*TOKENIZER_VAR_TYPE])]
  tokenizer: ParamVar,

  #[shard_param("Language", "Optional language code (e.g. 'en', 'fr'). If not specified, language will be auto-detected.", [common_type::string])]
  language: ParamVar,

  #[shard_param("Task", "The task type ('transcribe' or 'translate').", [common_type::string])]
  task: ParamVar,

  #[shard_param("Timestamps", "Whether to include timestamps in output.", [common_type::bool])]
  timestamps: ClonedVar,

  #[shard_param("BeamSize", "Beam size for decoding (default: 5).", [common_type::int])]
  beam_size: ClonedVar,

  #[shard_param("Seed", "The seed to use for the generation.", [common_type::int, common_type::int_var])]
  seed: ParamVar,

  output: ClonedVar,
}

impl Default for SpeechToTextShard {
  fn default() -> Self {
    Self {
      required: ExposedTypes::new(),
      model: ParamVar::default(),
      tokenizer: ParamVar::default(),
      language: ParamVar::default(),
      task: ParamVar::default(),
      timestamps: false.into(),
      beam_size: 5i64.into(),
      seed: ParamVar::new(42i64.into()),
      output: ClonedVar::default(),
    }
  }
}

#[shards::shard_impl]
impl Shard for SpeechToTextShard {
  fn input_types(&mut self) -> &Types {
    &TENSOR_TYPE_VEC
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

    if self.model.is_none() {
      return Err("Model is required");
    }
    if self.tokenizer.is_none() {
      return Err("Tokenizer is required");
    }

    Ok(STRING_TYPES[0])
  }

  fn activate(&mut self, _context: &Context, input: &Var) -> Result<Option<Var>, &str> {
    let model_var = self.model.get();
    let model = unsafe { &mut *Var::from_ref_counted_object::<Model>(&model_var, &*MODEL_TYPE)? };

    let tokenizer_var = self.tokenizer.get();
    let tokenizer = unsafe {
      &mut *Var::from_ref_counted_object::<crate::tokenizer::Tokenizer>(
        &tokenizer_var,
        &*TOKENIZER_TYPE,
      )?
    };

    let mel_tensor = unsafe { &*Var::from_ref_counted_object::<Tensor>(input, &*TENSOR_TYPE)? };

    // Convert to f32 before processing
    let mel_tensor = mel_tensor
      .0
      .to_dtype(candle_core::DType::F32)
      .map_err(|e| {
        shlog_error!("Failed to convert tensor to f32: {}", e);
        "Failed to convert tensor to f32"
      })?;

    // Get language token if specified
    let language_token = if !self.language.get().is_none() {
      let lang: &str = self.language.get().as_ref().try_into()?;
      Some(tokenizer.get_language_token(lang)?)
    } else {
      None
    };

    // Get task type if specified
    let task = if !self.task.get().is_none() {
      let task_str: &str = self.task.get().as_ref().try_into()?;
      match task_str {
        "translate" => Some(crate::whisper::Task::Translate),
        "transcribe" => Some(crate::whisper::Task::Transcribe),
        _ => return Err("Invalid task type"),
      }
    } else {
      None
    };

    let timestamps: bool = self.timestamps.as_ref().try_into()?;

    // Extract the underlying TokenizerPure from our Tokenizer enum
    let tokenizer_pure = match tokenizer {
      crate::tokenizer::Tokenizer::Normal(t) | crate::tokenizer::Tokenizer::Quantized(t) => t,
    };

    // Create decoder
    let mut decoder = crate::whisper::Decoder::new(
      match model {
        Model::Whisper(m) => crate::whisper::Model::Normal(m.clone()),
        Model::WhisperQuantized(m) => crate::whisper::Model::Quantized(m.clone()),
        _ => return Err("Model must be a Whisper model"),
      },
      tokenizer_pure.clone(),
      self.seed.get().as_ref().try_into()?,
      mel_tensor.device(),
      language_token,
      task,
      timestamps,
      false, // verbose
    )
    .map_err(|e| {
      shlog_error!("Failed to create decoder: {}", e);
      "Failed to create decoder"
    })?;

    // Run the decoder
    let segments = decoder.run(&mel_tensor).map_err(|e| {
      shlog_error!("Failed to run decoder: {}", e);
      "Failed to run decoder"
    })?;

    // Collect all text segments
    let mut final_text = String::new();
    for segment in segments {
      if !final_text.is_empty() {
        final_text.push(' ');
      }
      final_text.push_str(&segment.dr.text);
    }

    self.output = final_text.into();
    Ok(Some(self.output.0))
  }
}
