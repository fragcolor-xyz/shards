use candle_core::{Device, Tensor as CandleTensor};
use shards::shard::Shard;
use shards::shlog_error;
use shards::types::common_type;
use shards::types::ExposedTypes;
use shards::types::InstanceData;
use shards::types::ParamVar;
use shards::types::IMAGE_TYPES;
use shards::types::STRING_TYPES;
use shards::types::{ClonedVar, Context, Type, Types, Var};

use crate::get_global_device;
use crate::model::{Model, MODEL_TYPE, MODEL_VAR_TYPE};
use crate::tokenizer::{TOKENIZER_TYPE, TOKENIZER_VAR_TYPE};

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
