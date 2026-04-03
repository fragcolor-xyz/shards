use image::DynamicImage;
use mistralrs::blocking::BlockingModel;
use mistralrs::{
  AudioInput, ChatCompletionResponse, GgufModelBuilder, IsqBits, ModelBuilder,
  MultimodalMessages, TextMessageRole,
};
use std::sync::Arc;

use shards::fourCharacterCode;
use shards::ref_counted_object_type_impl;
use shards::shard::Shard;
use shards::shardsc::{SHImage, SHIMAGE_FLAGS_16BITS_INT, SHIMAGE_FLAGS_32BITS_FLOAT};
use shards::shlog_error;
use shards::types::common_type;
use shards::types::ExposedTypes;
use shards::types::IMAGE_TYPES;
use shards::types::InstanceData;
use shards::types::ParamVar;
use shards::types::SeqVar;
use shards::types::FRAG_CC;
use shards::types::SEQ_OF_FLOAT_TYPES;
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

fn build_multimodal_messages(messages: &[ChatMessage]) -> MultimodalMessages {
  let mut mm = MultimodalMessages::new();
  for msg in messages {
    match msg {
      ChatMessage::Text { role, text } => {
        mm = mm.add_message(role.clone(), text);
      }
      ChatMessage::Image { role, text, image } => {
        mm = mm.add_image_message(role.clone(), text, vec![image.clone()]);
      }
      ChatMessage::Audio { role, text, audio } => {
        mm = mm.add_audio_message(
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
  mm
}

// --- Object Types ---

mod model_obj {
  use super::*;
  pub struct LLMModel(pub BlockingModel);
  ref_counted_object_type_impl!(LLMModel);
}
pub use model_obj::LLMModel;

mod chat_obj {
  use super::*;
  pub struct LLMChat {
    pub model_var: Var,
    pub messages: Vec<ChatMessage>,
  }
  ref_counted_object_type_impl!(LLMChat);

  impl LLMChat {
    pub fn model(&self) -> Result<&BlockingModel, &'static str> {
      let model =
        unsafe { &*Var::from_ref_counted_object::<LLMModel>(&self.model_var, &*LLM_MODEL_TYPE)? };
      Ok(&model.0)
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
#[shard_info("LLM.Model", "Load a model via mistral.rs. Accepts a HuggingFace model ID or local path. Auto-detects architecture. For GGUF models, set the Files parameter.")]
pub(crate) struct ModelShard {
  #[shard_required]
  required: ExposedTypes,

  #[shard_param("ISQ", "In-situ quantization bit width. Quantizes at load time.", ISQBITSENUM_TYPES)]
  isq: ClonedVar,

  #[shard_param("Files", "GGUF filename(s) within the repo. When set, uses GGUF loader instead of auto-detect.", [common_type::string, common_type::none])]
  files: ClonedVar,

  output: ClonedVar,
}

impl Default for ModelShard {
  fn default() -> Self {
    Self {
      required: ExposedTypes::new(),
      isq: ISQBitsEnum::None.into(),
      files: ClonedVar::default(),
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

    let has_files = !self.files.0.is_none();

    let model = if has_files {
      // GGUF path: extract filenames
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

      // GgufModelBuilder doesn't have from_builder on BlockingModel,
      // so we create the runtime manually
      let rt = tokio::runtime::Builder::new_multi_thread()
        .enable_all()
        .build()
        .map_err(|e| {
          shlog_error!("Failed to create runtime: {}", e);
          "Failed to create tokio runtime"
        })?;
      let inner = rt.block_on(builder.build()).map_err(|e| {
        shlog_error!("Failed to load GGUF model: {}", e);
        "Failed to load GGUF model"
      })?;
      BlockingModel::new(inner, Arc::new(rt))
    } else {
      // Auto-detect path (safetensors from HuggingFace)
      let mut builder = ModelBuilder::new(model_id).with_logging();

      match isq_bits {
        ISQBitsEnum::None => {}
        ISQBitsEnum::Two => { builder = builder.with_auto_isq(IsqBits::Two); }
        ISQBitsEnum::Four => { builder = builder.with_auto_isq(IsqBits::Four); }
        ISQBitsEnum::Eight => { builder = builder.with_auto_isq(IsqBits::Eight); }
      }

      BlockingModel::from_auto_builder(builder).map_err(|e| {
        shlog_error!("Failed to load model: {}", e);
        "Failed to load model"
      })?
    };

    self.output = Var::new_ref_counted(LLMModel(model), &*LLM_MODEL_TYPE).into();
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
    let _model =
      unsafe { &*Var::from_ref_counted_object::<LLMModel>(input, &*LLM_MODEL_TYPE)? };

    let chat = LLMChat {
      model_var: *input,
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

    let messages = build_multimodal_messages(&chat.messages);
    let model = chat.model()?;

    let response: ChatCompletionResponse = model.send_chat_request(messages).map_err(|e| {
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
