use either::Either;
use image::DynamicImage;
use mistralrs::blocking::BlockingModel;
use mistralrs::{
  AudioInput, ChatCompletionResponse, EmbeddingModelBuilder, EmbeddingRequest, GgufModelBuilder,
  IsqBits, ModelBuilder, ModelDType, RequestBuilder, TextMessageRole, UqffMultimodalModelBuilder,
};
use std::path::PathBuf;
use std::sync::Arc;

use shards::fourCharacterCode;
use shards::ref_counted_object_type_impl;
use shards::shard::Shard;
use shards::shardsc::{SHImage, SHIMAGE_FLAGS_16BITS_INT, SHIMAGE_FLAGS_32BITS_FLOAT};
use shards::shlog_error;
use shards::types::common_type;
use shards::types::AutoSeqVar;
use shards::types::ExposedTypes;
use shards::types::IMAGE_TYPES;
use shards::types::InstanceData;
use shards::types::ParamVar;
use shards::types::SeqVar;
use shards::types::FRAG_CC;
use shards::types::SEQ_OF_FLOAT_TYPES;
use shards::types::SEQ_OF_INT_TYPES;
use shards::types::STRING_TYPES;
use shards::types::{ClonedVar, Context, Type, Types, Var};

// --- Chat message types ---

pub enum ChatMessage {
  Text {
    role: TextMessageRole,
    text: String,
  },
  Image {
    role: TextMessageRole,
    text: String,
    image: DynamicImage,
  },
  Audio {
    role: TextMessageRole,
    text: String,
    audio: AudioInput,
  },
}

fn build_request(messages: &[ChatMessage]) -> RequestBuilder {
  let mut rb = RequestBuilder::new();
  for msg in messages {
    match msg {
      ChatMessage::Text { role, text } => {
        rb = rb.add_message(role.clone(), text);
      }
      ChatMessage::Image { role, text, image } => {
        rb = rb.add_image_message(role.clone(), text, vec![image.clone()]);
      }
      ChatMessage::Audio { role, text, audio } => {
        rb = rb.add_audio_message(
          role.clone(),
          text,
          vec![AudioInput {
            samples: audio.samples.clone(),
            sample_rate: audio.sample_rate,
            channels: audio.channels,
          }],
        );
      }
    }
  }
  rb
}

// --- Object Types ---

mod model_obj {
  use super::*;
  use crate::quantized_bert::QuantizedBertModel;

  pub enum LLMModelInner {
    /// mistral.rs model (chat, multimodal, embedding via EmbeddingModelBuilder)
    Mistral {
      blocking: BlockingModel,
      rt: Arc<tokio::runtime::Runtime>,
    },
    /// Quantized BERT from GGUF (embedding only, uses candle directly)
    QuantizedBert {
      model: QuantizedBertModel,
      tokenizer: tokenizers::Tokenizer,
    },
  }

  pub struct LLMModel(pub LLMModelInner);
  ref_counted_object_type_impl!(LLMModel);
}
pub use model_obj::{LLMModel, LLMModelInner};

mod chat_obj {
  use super::*;
  pub struct LLMChat {
    pub model_var: ClonedVar,
    pub messages: Vec<ChatMessage>,
  }
  ref_counted_object_type_impl!(LLMChat);

  impl LLMChat {
    pub fn blocking_model(&self) -> Result<(&BlockingModel, &Arc<tokio::runtime::Runtime>), &'static str> {
      let model =
        unsafe { &*Var::from_ref_counted_object::<LLMModel>(&self.model_var.0, &*LLM_MODEL_TYPE)? };
      match &model.0 {
        LLMModelInner::Mistral { blocking, rt } => Ok((blocking, rt)),
        LLMModelInner::QuantizedBert { .. } => Err("Cannot use a GGUF BERT model for chat"),
      }
    }
  }
}
pub use chat_obj::LLMChat;

lazy_static! {
  pub static ref LLM_MODEL_TYPE: Type = Type::object(FRAG_CC, fourCharacterCode(*b"aiMD"));
  pub static ref LLM_MODEL_TYPE_VEC: Vec<Type> = vec![*LLM_MODEL_TYPE];
  pub static ref LLM_MODEL_VAR_TYPE: Type = Type::context_variable(&LLM_MODEL_TYPE_VEC);

  pub static ref LLM_CHAT_TYPE: Type = Type::object(FRAG_CC, fourCharacterCode(*b"aiCH"));
  pub static ref LLM_CHAT_TYPE_VEC: Vec<Type> = vec![*LLM_CHAT_TYPE];
  pub static ref LLM_CHAT_VAR_TYPE: Type = Type::context_variable(&LLM_CHAT_TYPE_VEC);
}

// --- Enums ---

#[derive(shards::shards_enum, Debug, PartialEq, Eq, PartialOrd, Ord)]
#[enum_info(b"aISQ", "ISQBits", "In-situ quantization bit width.")]
pub enum ISQBitsEnum {
  #[enum_value("No quantization.")]
  None = 0x0,
  #[enum_value("4-bit quantization.")]
  Four = 0x1,
  #[enum_value("8-bit quantization.")]
  Eight = 0x2,
  #[enum_value("2-bit quantization.")]
  Two = 0x3,
}

#[derive(shards::shards_enum, Debug, PartialEq, Eq, PartialOrd, Ord)]
#[enum_info(b"aROL", "ChatRole", "The role of a chat message.")]
pub enum ChatRole {
  #[enum_value("User message.")]
  User = 0x0,
  #[enum_value("Assistant message.")]
  Assistant = 0x1,
  #[enum_value("System prompt.")]
  System = 0x2,
}

impl From<ChatRole> for TextMessageRole {
  fn from(role: ChatRole) -> Self {
    match role {
      ChatRole::User => TextMessageRole::User,
      ChatRole::Assistant => TextMessageRole::Assistant,
      ChatRole::System => TextMessageRole::System,
    }
  }
}

// --- Helper: convert SHImage to DynamicImage ---

fn sh_image_to_dynamic(img: &SHImage) -> Result<DynamicImage, &'static str> {
  let w = img.width as u32;
  let h = img.height as u32;
  let channels = img.channels as usize;
  let flags = img.flags as u32;

  if flags & SHIMAGE_FLAGS_32BITS_FLOAT != 0 || flags & SHIMAGE_FLAGS_16BITS_INT != 0 {
    return Err("Only 8-bit images are supported for LLM.AddImage");
  }

  let data_len = (w as usize) * (h as usize) * channels;
  let data = unsafe { std::slice::from_raw_parts(img.data, data_len) };

  match channels {
    3 => {
      let buf = image::ImageBuffer::<image::Rgb<u8>, _>::from_raw(w, h, data.to_vec())
        .ok_or("Failed to create RGB image buffer")?;
      Ok(DynamicImage::ImageRgb8(buf))
    }
    4 => {
      let buf = image::ImageBuffer::<image::Rgba<u8>, _>::from_raw(w, h, data.to_vec())
        .ok_or("Failed to create RGBA image buffer")?;
      Ok(DynamicImage::ImageRgba8(buf))
    }
    1 => {
      let buf = image::ImageBuffer::<image::Luma<u8>, _>::from_raw(w, h, data.to_vec())
        .ok_or("Failed to create grayscale image buffer")?;
      Ok(DynamicImage::ImageLuma8(buf))
    }
    _ => Err("Unsupported image channel count"),
  }
}

// --- LLM.Model ---

#[derive(shards::shard)]
#[shard_info("LLM.Model", "Load a model via mistral.rs. Accepts a HuggingFace model ID or local path. Auto-detects architecture. For GGUF models, set the Files parameter. For embedding models, set Embedding: true.")]
pub(crate) struct ModelShard {
  #[shard_required]
  required: ExposedTypes,

  #[shard_param("ISQ", "In-situ quantization bit width. Quantizes at load time.", ISQBITSENUM_TYPES)]
  isq: ClonedVar,

  #[shard_param("Files", "GGUF filename(s) within the repo. When set, uses GGUF loader.", [common_type::string, common_type::none])]
  files: ClonedVar,

  #[shard_param("UQFF", "UQFF filename (e.g. 'q4k-0.uqff'). When set, loads pre-quantized UQFF model — no ISQ needed.", [common_type::string, common_type::none])]
  uqff: ClonedVar,

  #[shard_param("Embedding", "When true, loads as an embedding model for use with LLM.Embed.", [common_type::bool])]
  embedding: ClonedVar,

  output: ClonedVar,
}

impl Default for ModelShard {
  fn default() -> Self {
    Self {
      required: ExposedTypes::new(),
      isq: ISQBitsEnum::None.into(),
      files: ClonedVar::default(),
      uqff: ClonedVar::default(),
      embedding: false.into(),
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
    &LLM_MODEL_TYPE_VEC
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
    let model_id: &str = input.try_into()?;
    let isq_bits: ISQBitsEnum = self.isq.0.as_ref().try_into().unwrap_or(ISQBitsEnum::None);
    let is_embedding: bool = (&self.embedding.0).try_into().unwrap_or(false);

    let has_files = !self.files.0.is_none();
    let has_uqff = !self.uqff.0.is_none();

    // Helper to create a tokio runtime
    let make_rt = || -> Result<Arc<tokio::runtime::Runtime>, &'static str> {
      let rt = tokio::runtime::Builder::new_multi_thread()
        .enable_all()
        .build()
        .map_err(|e| {
          shlog_error!("Failed to create runtime: {}", e);
          "Failed to create tokio runtime"
        })?;
      Ok(Arc::new(rt))
    };

    // Embedding + GGUF file → Quantized BERT path (candle native, no mistral.rs)
    if is_embedding && has_files {
      let gguf_file: &str = self.files.0.as_ref().try_into()
        .map_err(|_| "Files parameter must be a string for GGUF embedding models")?;

      let device = crate::get_global_device();

      let vb = candle_transformers::quantized_var_builder::VarBuilder::from_gguf(gguf_file, device)
        .map_err(|e| {
          shlog_error!("Failed to load GGUF file: {}", e);
          "Failed to load GGUF file"
        })?;

      // Read GGUF metadata for config
      let mut reader = std::io::BufReader::new(std::fs::File::open(gguf_file).map_err(|e| {
        shlog_error!("Failed to open GGUF file: {}", e);
        "Failed to open GGUF file"
      })?);
      let content = candle_core::quantized::gguf_file::Content::read(&mut reader).map_err(|e| {
        shlog_error!("Failed to read GGUF metadata: {}", e);
        "Failed to read GGUF metadata"
      })?;

      let cfg = crate::quantized_bert::BertConfig::from_gguf_metadata(&content.metadata)
        .map_err(|e| {
          shlog_error!("Failed to parse BERT config from GGUF: {}", e);
          "Failed to parse BERT config from GGUF"
        })?;

      let bert_model = crate::quantized_bert::QuantizedBertModel::load(&cfg, &vb)
        .map_err(|e| {
          shlog_error!("Failed to load quantized BERT: {}", e);
          "Failed to load quantized BERT model"
        })?;

      // Load tokenizer from model_id on HuggingFace (or local path)
      let tokenizer = {
        let api = hf_hub::api::sync::Api::new().map_err(|e| {
          shlog_error!("Failed to create HF API: {}", e);
          "Failed to create HuggingFace API"
        })?;
        let repo = api.model(model_id.to_string());
        let tokenizer_path = repo.get("tokenizer.json").map_err(|e| {
          shlog_error!("Failed to download tokenizer: {}", e);
          "Failed to download tokenizer.json"
        })?;
        tokenizers::Tokenizer::from_file(tokenizer_path).map_err(|e| {
          shlog_error!("Failed to load tokenizer: {}", e);
          "Failed to load tokenizer"
        })?
      };

      let inner = LLMModelInner::QuantizedBert {
        model: bert_model,
        tokenizer,
      };
      self.output = Var::new_ref_counted(LLMModel(inner), &*LLM_MODEL_TYPE).into();
      return Ok(Some(self.output.0));
    }

    let inner = if is_embedding {
      // Embedding model path (mistral.rs EmbeddingModelBuilder)
      let rt = make_rt()?;
      let mut builder = EmbeddingModelBuilder::new(model_id)
        .with_logging()
        .with_dtype(ModelDType::F32);

      match isq_bits {
        ISQBitsEnum::None => {}
        ISQBitsEnum::Two => { builder = builder.with_auto_isq(IsqBits::Two); }
        ISQBitsEnum::Four => { builder = builder.with_auto_isq(IsqBits::Four); }
        ISQBitsEnum::Eight => { builder = builder.with_auto_isq(IsqBits::Eight); }
      }

      let model = rt.block_on(builder.build()).map_err(|e| {
        shlog_error!("Failed to load embedding model: {}", e);
        "Failed to load embedding model"
      })?;
      LLMModelInner::Mistral { blocking: BlockingModel::new(model, rt.clone()), rt }
    } else if has_uqff {
      let uqff_file: &str = self.uqff.0.as_ref().try_into()
        .map_err(|_| "UQFF parameter must be a string")?;

      let builder = UqffMultimodalModelBuilder::new(
        model_id,
        vec![PathBuf::from(uqff_file)],
      ).into_inner().with_logging();

      let rt = make_rt()?;
      let model = rt.block_on(builder.build()).map_err(|e| {
        shlog_error!("Failed to load UQFF model: {}", e);
        "Failed to load UQFF model"
      })?;
      LLMModelInner::Mistral { blocking: BlockingModel::new(model, rt.clone()), rt }
    } else if has_files {
      let mut gguf_files: Vec<String> = Vec::new();
      if let Ok(s) = <&str>::try_from(self.files.0.as_ref()) {
        gguf_files.push(s.to_string());
      } else if let Ok(seq) = SeqVar::try_from(self.files.0.as_ref()) {
        for item in seq.iter() {
          let s: &str = item.as_ref().try_into().map_err(|_| "Files must be strings")?;
          gguf_files.push(s.to_string());
        }
      } else {
        return Err("Files parameter must be a string or sequence of strings");
      }

      let builder = GgufModelBuilder::new(model_id, gguf_files).with_logging();

      let rt = make_rt()?;
      let model = rt.block_on(builder.build()).map_err(|e| {
        shlog_error!("Failed to load GGUF model: {}", e);
        "Failed to load GGUF model"
      })?;
      LLMModelInner::Mistral { blocking: BlockingModel::new(model, rt.clone()), rt }
    } else {
      let mut builder = ModelBuilder::new(model_id).with_logging();

      match isq_bits {
        ISQBitsEnum::None => {}
        ISQBitsEnum::Two => { builder = builder.with_auto_isq(IsqBits::Two); }
        ISQBitsEnum::Four => { builder = builder.with_auto_isq(IsqBits::Four); }
        ISQBitsEnum::Eight => { builder = builder.with_auto_isq(IsqBits::Eight); }
      }

      let rt = make_rt()?;
      let model = rt.block_on(builder.build()).map_err(|e| {
        shlog_error!("Failed to load model: {}", e);
        "Failed to load model"
      })?;
      LLMModelInner::Mistral { blocking: BlockingModel::new(model, rt.clone()), rt }
    };

    self.output = Var::new_ref_counted(LLMModel(inner), &*LLM_MODEL_TYPE).into();
    Ok(Some(self.output.0))
  }
}

// --- LLM.Chat ---

#[derive(shards::shard)]
#[shard_info("LLM.Chat", "Create a chat session from a loaded model.")]
pub(crate) struct ChatShard {
  #[shard_required]
  required: ExposedTypes,

  output: ClonedVar,
}

impl Default for ChatShard {
  fn default() -> Self {
    Self {
      required: ExposedTypes::new(),
      output: ClonedVar::default(),
    }
  }
}

#[shards::shard_impl]
impl Shard for ChatShard {
  fn input_types(&mut self) -> &Types {
    &LLM_MODEL_TYPE_VEC
  }

  fn output_types(&mut self) -> &Types {
    &LLM_CHAT_TYPE_VEC
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
    // Validate that input is a valid LLMModel
    let _ = unsafe { &*Var::from_ref_counted_object::<LLMModel>(input, &*LLM_MODEL_TYPE)? };

    let chat = LLMChat {
      model_var: input.into(),
      messages: Vec::new(),
    };

    self.output = Var::new_ref_counted(chat, &*LLM_CHAT_TYPE).into();
    Ok(Some(self.output.0))
  }
}

// --- LLM.AddText ---

#[derive(shards::shard)]
#[shard_info("LLM.AddText", "Add a text message to a chat session.")]
pub(crate) struct AddTextShard {
  #[shard_required]
  required: ExposedTypes,

  #[shard_param("Chat", "The chat session.", [*LLM_CHAT_VAR_TYPE])]
  chat: ParamVar,

  #[shard_param("Role", "Message role.", CHATROLE_TYPES)]
  role: ClonedVar,
}

impl Default for AddTextShard {
  fn default() -> Self {
    Self {
      required: ExposedTypes::new(),
      chat: ParamVar::default(),
      role: ChatRole::User.into(),
    }
  }
}

#[shards::shard_impl]
impl Shard for AddTextShard {
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
    if self.chat.is_none() {
      return Err("Chat parameter is required");
    }
    Ok(self.output_types()[0])
  }

  fn activate(&mut self, _context: &Context, input: &Var) -> Result<Option<Var>, &str> {
    let text: &str = input.try_into()?;
    let role: ChatRole = self.role.0.as_ref().try_into().unwrap_or(ChatRole::User);

    let chat = unsafe {
      &mut *Var::from_ref_counted_object::<LLMChat>(&self.chat.get(), &*LLM_CHAT_TYPE)?
    };

    chat.messages.push(ChatMessage::Text {
      role: role.into(),
      text: text.to_string(),
    });
    Ok(Some(*input))
  }
}

// --- LLM.AddImage ---

#[derive(shards::shard)]
#[shard_info("LLM.AddImage", "Add an image to a chat session. Requires a vision-capable model.")]
pub(crate) struct AddImageShard {
  #[shard_required]
  required: ExposedTypes,

  #[shard_param("Chat", "The chat session.", [*LLM_CHAT_VAR_TYPE])]
  chat: ParamVar,

  #[shard_param("Text", "Text prompt to accompany the image.", [common_type::string, common_type::string_var])]
  text: ParamVar,
}

impl Default for AddImageShard {
  fn default() -> Self {
    Self {
      required: ExposedTypes::new(),
      chat: ParamVar::default(),
      text: ParamVar::default(),
    }
  }
}

#[shards::shard_impl]
impl Shard for AddImageShard {
  fn input_types(&mut self) -> &Types {
    &IMAGE_TYPES
  }

  fn output_types(&mut self) -> &Types {
    &IMAGE_TYPES
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
    if self.chat.is_none() {
      return Err("Chat parameter is required");
    }
    Ok(self.output_types()[0])
  }

  fn activate(&mut self, _context: &Context, input: &Var) -> Result<Option<Var>, &str> {
    let sh_img: &SHImage = input.try_into()?;
    let dynamic_img = sh_image_to_dynamic(sh_img)?;

    let text = if self.text.is_none() {
      String::new()
    } else {
      let t: &str = self.text.get().as_ref().try_into().unwrap_or("");
      t.to_string()
    };

    let chat = unsafe {
      &mut *Var::from_ref_counted_object::<LLMChat>(&self.chat.get(), &*LLM_CHAT_TYPE)?
    };

    chat.messages.push(ChatMessage::Image {
      role: TextMessageRole::User,
      text,
      image: dynamic_img,
    });

    Ok(Some(*input))
  }
}

// --- LLM.AddAudio ---

#[derive(shards::shard)]
#[shard_info("LLM.AddAudio", "Add audio samples to a chat session. Requires an audio-capable model (e.g. Gemma 4 E2B/E4B).")]
pub(crate) struct AddAudioShard {
  #[shard_required]
  required: ExposedTypes,

  #[shard_param("Chat", "The chat session.", [*LLM_CHAT_VAR_TYPE])]
  chat: ParamVar,

  #[shard_param("Text", "Text prompt to accompany the audio.", [common_type::string, common_type::string_var])]
  text: ParamVar,

  #[shard_param("SampleRate", "Audio sample rate in Hz.", [common_type::int])]
  sample_rate: ClonedVar,
}

impl Default for AddAudioShard {
  fn default() -> Self {
    Self {
      required: ExposedTypes::new(),
      chat: ParamVar::default(),
      text: ParamVar::default(),
      sample_rate: 16000i64.into(),
    }
  }
}

#[shards::shard_impl]
impl Shard for AddAudioShard {
  fn input_types(&mut self) -> &Types {
    &SEQ_OF_FLOAT_TYPES
  }

  fn output_types(&mut self) -> &Types {
    &SEQ_OF_FLOAT_TYPES
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
    if self.chat.is_none() {
      return Err("Chat parameter is required");
    }
    Ok(self.output_types()[0])
  }

  fn activate(&mut self, _context: &Context, input: &Var) -> Result<Option<Var>, &str> {
    let seq: SeqVar = input.try_into()?;
    let sample_rate: i64 = self.sample_rate.0.as_ref().try_into().unwrap_or(16000);

    let mut samples = Vec::with_capacity(seq.len());
    for item in seq.iter() {
      let v: f64 = item.as_ref().try_into().map_err(|_| "Expected float values in audio sequence")?;
      samples.push(v as f32);
    }

    let text = if self.text.is_none() {
      String::new()
    } else {
      let t: &str = self.text.get().as_ref().try_into().unwrap_or("");
      t.to_string()
    };

    let chat = unsafe {
      &mut *Var::from_ref_counted_object::<LLMChat>(&self.chat.get(), &*LLM_CHAT_TYPE)?
    };

    chat.messages.push(ChatMessage::Audio {
      role: TextMessageRole::User,
      text,
      audio: AudioInput {
        samples,
        sample_rate: sample_rate as u32,
        channels: 1,
      },
    });

    Ok(Some(*input))
  }
}

// --- LLM.Generate ---

#[derive(shards::shard)]
#[shard_info("LLM.Generate", "Generate a response from a chat session. Appends assistant reply to history.")]
pub(crate) struct GenerateShard {
  #[shard_required]
  required: ExposedTypes,

  #[shard_param("Temperature", "Sampling temperature.", [common_type::float])]
  temperature: ClonedVar,

  #[shard_param("TopP", "Top-p (nucleus) sampling.", [common_type::float])]
  top_p: ClonedVar,

  #[shard_param("MaxTokens", "Maximum tokens to generate.", [common_type::int])]
  max_tokens: ClonedVar,

  output: ClonedVar,
}

impl Default for GenerateShard {
  fn default() -> Self {
    Self {
      required: ExposedTypes::new(),
      temperature: 0.7f64.into(),
      top_p: 0.95f64.into(),
      max_tokens: 256i64.into(),
      output: ClonedVar::default(),
    }
  }
}

#[shards::shard_impl]
impl Shard for GenerateShard {
  fn input_types(&mut self) -> &Types {
    &LLM_CHAT_TYPE_VEC
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
    let chat = unsafe {
      &mut *Var::from_ref_counted_object::<LLMChat>(input, &*LLM_CHAT_TYPE)?
    };

    let mut request = build_request(&chat.messages);

    let temperature: f64 = self.temperature.0.as_ref().try_into().unwrap_or(0.7);
    let top_p: f64 = self.top_p.0.as_ref().try_into().unwrap_or(0.95);
    let max_tokens: i64 = self.max_tokens.0.as_ref().try_into().unwrap_or(256);

    request = request
      .set_sampler_temperature(temperature)
      .set_sampler_topp(top_p)
      .set_sampler_max_len(max_tokens as usize);

    let (blocking, _rt) = chat.blocking_model()?;

    let response: ChatCompletionResponse = blocking.send_chat_request(request).map_err(|e| {
      shlog_error!("Failed to generate: {}", e);
      "Failed to generate response"
    })?;

    let text = response
      .choices
      .first()
      .and_then(|c| c.message.content.as_ref())
      .map(|s| s.as_str())
      .unwrap_or("");

    chat.messages.push(ChatMessage::Text {
      role: TextMessageRole::Assistant,
      text: text.to_string(),
    });

    self.output = Var::ephemeral_string(text).into();
    Ok(Some(self.output.0))
  }
}

// --- LLM.Reset ---

#[derive(shards::shard)]
#[shard_info("LLM.Reset", "Clear all message history from a chat session.")]
pub(crate) struct ResetShard {
  #[shard_required]
  required: ExposedTypes,
}

impl Default for ResetShard {
  fn default() -> Self {
    Self {
      required: ExposedTypes::new(),
    }
  }
}

#[shards::shard_impl]
impl Shard for ResetShard {
  fn input_types(&mut self) -> &Types {
    &LLM_CHAT_TYPE_VEC
  }

  fn output_types(&mut self) -> &Types {
    &LLM_CHAT_TYPE_VEC
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
    let chat = unsafe {
      &mut *Var::from_ref_counted_object::<LLMChat>(input, &*LLM_CHAT_TYPE)?
    };
    chat.messages.clear();
    Ok(Some(*input))
  }
}

// --- Normalization helper ---

fn normalize_embedding(embedding: &mut Vec<f32>, norm_type: i64) {
  match norm_type {
    -1 => {} // no normalization
    0 => {
      // max absolute value normalization
      let max_abs = embedding.iter().map(|v| v.abs()).fold(0.0f32, f32::max);
      if max_abs > 0.0 {
        for v in embedding.iter_mut() {
          *v /= max_abs;
        }
      }
    }
    2 => {
      // L2 (euclidean) normalization
      let norm: f32 = embedding.iter().map(|v| v * v).sum::<f32>().sqrt();
      if norm > 0.0 {
        for v in embedding.iter_mut() {
          *v /= norm;
        }
      }
    }
    p if p > 2 => {
      // p-norm normalization
      let p_f = p as f32;
      let norm: f32 = embedding.iter().map(|v| v.abs().powf(p_f)).sum::<f32>().powf(1.0 / p_f);
      if norm > 0.0 {
        for v in embedding.iter_mut() {
          *v /= norm;
        }
      }
    }
    _ => {} // unsupported, no normalization
  }
}

// --- LLM.Embed ---

lazy_static! {
  static ref SEQ_OF_INT: Type = Type::seq(&INT_TYPES_VEC);
  static ref EMBED_INPUT_TYPES: Types = vec![common_type::string, *SEQ_OF_INT];
  static ref INT_TYPES_VEC: Vec<Type> = vec![common_type::int];
}

#[derive(shards::shard)]
#[shard_info("LLM.Embed", "Generate embeddings from text or token IDs using an embedding model. The model must be loaded with Embedding: true.")]
pub(crate) struct EmbedShard {
  #[shard_required]
  required: ExposedTypes,

  #[shard_param("Model", "The embedding model.", [*LLM_MODEL_VAR_TYPE])]
  model: ParamVar,

  #[shard_param("Normalization", "Normalization type: -1 (none), 0 (max absolute), 2 (L2/euclidean), >2 (p-norm).", [common_type::int])]
  normalization: ClonedVar,

  output: AutoSeqVar,
}

impl Default for EmbedShard {
  fn default() -> Self {
    Self {
      required: ExposedTypes::new(),
      model: ParamVar::default(),
      normalization: (-1i64).into(),
      output: AutoSeqVar::new(),
    }
  }
}

#[shards::shard_impl]
impl Shard for EmbedShard {
  fn input_types(&mut self) -> &Types {
    &EMBED_INPUT_TYPES
  }

  fn output_types(&mut self) -> &Types {
    &SEQ_OF_FLOAT_TYPES
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
    if self.model.is_none() {
      return Err("Model parameter is required");
    }
    Ok(self.output_types()[0])
  }

  fn activate(&mut self, _context: &Context, input: &Var) -> Result<Option<Var>, &str> {
    let llm_model = unsafe {
      &*Var::from_ref_counted_object::<LLMModel>(&self.model.get(), &*LLM_MODEL_TYPE)?
    };

    let norm_type: i64 = self.normalization.0.as_ref().try_into().unwrap_or(-1);

    let mut embedding = match &llm_model.0 {
      LLMModelInner::Mistral { blocking, rt } => {
        // Build embedding request — accept either text or token IDs
        let request = if let Ok(text) = <&str>::try_from(input) {
          EmbeddingRequest::builder().add_prompt(text)
        } else if let Ok(seq) = SeqVar::try_from(input) {
          let tokens: Vec<u32> = seq.iter()
            .map(|v| {
              let i: i64 = v.as_ref().try_into().unwrap_or(0);
              i as u32
            })
            .collect();
          EmbeddingRequest::builder().add_tokens(tokens)
        } else {
          return Err("Input must be a string or sequence of integers");
        };

        let embeddings = rt.block_on(
          blocking.inner().generate_embeddings(request)
        ).map_err(|e| {
          shlog_error!("Failed to generate embedding: {}", e);
          "Failed to generate embedding"
        })?;

        embeddings.into_iter().next().ok_or("No embedding returned")?
      }
      LLMModelInner::QuantizedBert { model, tokenizer } => {
        // For quantized BERT, we tokenize and run the model directly
        let text: &str = if let Ok(t) = <&str>::try_from(input) {
          t
        } else {
          return Err("Quantized BERT embedding only accepts text input");
        };

        let encoding = tokenizer.encode(text, true).map_err(|e| {
          shlog_error!("Failed to tokenize: {}", e);
          "Failed to tokenize for BERT embedding"
        })?;

        let device = crate::get_global_device();
        let token_ids = encoding.get_ids();
        let input_ids = candle_core::Tensor::new(
          &token_ids[..],
          device,
        ).map_err(|_| "Failed to create input tensor")?
          .unsqueeze(0).map_err(|_| "Failed to unsqueeze")?;

        let token_type_ids = candle_core::Tensor::zeros_like(&input_ids)
          .map_err(|_| "Failed to create token_type_ids")?;

        let embedding_tensor = model.embed(&input_ids, &token_type_ids)
          .map_err(|e| {
            shlog_error!("Failed to run BERT forward: {}", e);
            "Failed to generate BERT embedding"
          })?;

        // Extract [1, hidden_size] -> Vec<f32>
        embedding_tensor.squeeze(0)
          .map_err(|_| "Failed to squeeze")?
          .to_vec1::<f32>()
          .map_err(|_| "Failed to convert embedding to Vec<f32>")?
      }
    };

    // Apply normalization
    normalize_embedding(&mut embedding, norm_type);

    // Convert Vec<f32> to sequence of floats
    self.output.0.clear();
    for val in embedding {
      self.output.0.push(&(val as f64).into());
    }

    Ok(Some(self.output.0 .0))
  }
}

// --- LLM.Tokenize ---

#[derive(shards::shard)]
#[shard_info("LLM.Tokenize", "Tokenize text into token IDs using the model's tokenizer.")]
pub(crate) struct TokenizeShard {
  #[shard_required]
  required: ExposedTypes,

  #[shard_param("Model", "The model whose tokenizer to use.", [*LLM_MODEL_VAR_TYPE])]
  model: ParamVar,

  output: AutoSeqVar,
}

impl Default for TokenizeShard {
  fn default() -> Self {
    Self {
      required: ExposedTypes::new(),
      model: ParamVar::default(),
      output: AutoSeqVar::new(),
    }
  }
}

#[shards::shard_impl]
impl Shard for TokenizeShard {
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
    self.output = AutoSeqVar::new();
    Ok(())
  }

  fn compose(&mut self, data: &InstanceData) -> Result<Type, &str> {
    self.compose_helper(data)?;
    if self.model.is_none() {
      return Err("Model parameter is required");
    }
    Ok(self.output_types()[0])
  }

  fn activate(&mut self, _context: &Context, input: &Var) -> Result<Option<Var>, &str> {
    let text: &str = input.try_into()?;

    let llm_model = unsafe {
      &*Var::from_ref_counted_object::<LLMModel>(&self.model.get(), &*LLM_MODEL_TYPE)?
    };

    let tokens: Vec<u32> = match &llm_model.0 {
      LLMModelInner::Mistral { blocking, rt } => {
        rt.block_on(
          blocking.inner().tokenize(
            Either::Right(text.to_string()),
            None, true, false, None,
          )
        ).map_err(|e| {
          shlog_error!("Failed to tokenize: {}", e);
          "Failed to tokenize"
        })?
      }
      LLMModelInner::QuantizedBert { tokenizer, .. } => {
        let encoding = tokenizer.encode(text, true).map_err(|e| {
          shlog_error!("Failed to tokenize: {}", e);
          "Failed to tokenize"
        })?;
        encoding.get_ids().to_vec()
      }
    };

    self.output.0.clear();
    for token in tokens {
      self.output.0.push(&(token as i64).into());
    }

    Ok(Some(self.output.0 .0))
  }
}

// --- LLM.Detokenize ---

#[derive(shards::shard)]
#[shard_info("LLM.Detokenize", "Convert token IDs back to text using the model's tokenizer.")]
pub(crate) struct DetokenizeShard {
  #[shard_required]
  required: ExposedTypes,

  #[shard_param("Model", "The model whose tokenizer to use.", [*LLM_MODEL_VAR_TYPE])]
  model: ParamVar,

  output: ClonedVar,
}

impl Default for DetokenizeShard {
  fn default() -> Self {
    Self {
      required: ExposedTypes::new(),
      model: ParamVar::default(),
      output: ClonedVar::default(),
    }
  }
}

#[shards::shard_impl]
impl Shard for DetokenizeShard {
  fn input_types(&mut self) -> &Types {
    &SEQ_OF_INT_TYPES
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
      return Err("Model parameter is required");
    }
    Ok(self.output_types()[0])
  }

  fn activate(&mut self, _context: &Context, input: &Var) -> Result<Option<Var>, &str> {
    let seq: SeqVar = input.try_into()?;

    let llm_model = unsafe {
      &*Var::from_ref_counted_object::<LLMModel>(&self.model.get(), &*LLM_MODEL_TYPE)?
    };

    let tokens: Vec<u32> = seq.iter()
      .map(|v| {
        let i: i64 = v.as_ref().try_into().unwrap_or(0);
        i as u32
      })
      .collect();

    let text = match &llm_model.0 {
      LLMModelInner::Mistral { blocking, rt } => {
        rt.block_on(
          blocking.inner().detokenize(tokens, true)
        ).map_err(|e| {
          shlog_error!("Failed to detokenize: {}", e);
          "Failed to detokenize"
        })?
      }
      LLMModelInner::QuantizedBert { tokenizer, .. } => {
        tokenizer.decode(&tokens, true).map_err(|e| {
          shlog_error!("Failed to detokenize: {}", e);
          "Failed to detokenize"
        })?
      }
    };

    self.output = Var::ephemeral_string(&text).into();
    Ok(Some(self.output.0))
  }
}
