use mistralrs::blocking::BlockingModel;
use mistralrs::{
  ChatCompletionResponse, IsqBits, ModelBuilder, TextMessageRole, TextMessages,
};

use shards::fourCharacterCode;
use shards::ref_counted_object_type_impl;
use shards::shard::Shard;
use shards::shlog_error;
use shards::types::common_type;
use shards::types::ExposedTypes;
use shards::types::InstanceData;
use shards::types::ParamVar;
use shards::types::FRAG_CC;
use shards::types::STRING_TYPES;
use shards::types::{ClonedVar, Context, Type, Types, Var};

// --- Object Types ---

// Each ref_counted_object_type_impl! must be in its own module to avoid name collisions
mod model_obj {
  use super::*;
  pub struct AIModel(pub BlockingModel);
  ref_counted_object_type_impl!(AIModel);
}
pub use model_obj::AIModel;

mod chat_obj {
  use super::*;
  pub struct AIChat {
    pub model_var: Var,
    pub messages: Vec<(TextMessageRole, String)>,
  }
  ref_counted_object_type_impl!(AIChat);

  impl AIChat {
    pub fn model(&self) -> Result<&BlockingModel, &'static str> {
      let model =
        unsafe { &*Var::from_ref_counted_object::<AIModel>(&self.model_var, &*AI_MODEL_TYPE)? };
      Ok(&model.0)
    }
  }
}
pub use chat_obj::AIChat;

lazy_static! {
  pub static ref AI_MODEL_TYPE: Type = Type::object(FRAG_CC, fourCharacterCode(*b"aiMD"));
  pub static ref AI_MODEL_TYPE_VEC: Vec<Type> = vec![*AI_MODEL_TYPE];
  pub static ref AI_MODEL_VAR_TYPE: Type = Type::context_variable(&AI_MODEL_TYPE_VEC);

  pub static ref AI_CHAT_TYPE: Type = Type::object(FRAG_CC, fourCharacterCode(*b"aiCH"));
  pub static ref AI_CHAT_TYPE_VEC: Vec<Type> = vec![*AI_CHAT_TYPE];
  pub static ref AI_CHAT_VAR_TYPE: Type = Type::context_variable(&AI_CHAT_TYPE_VEC);
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

// --- AI.Model ---

#[derive(shards::shard)]
#[shard_info("LLM.Model", "Load a model via mistral.rs. Accepts a HuggingFace model ID or local path. Auto-detects architecture (text, vision, audio, embedding).")]
pub(crate) struct ModelShard {
  #[shard_required]
  required: ExposedTypes,

  #[shard_param("ISQ", "In-situ quantization bit width. Quantizes at load time.", ISQBITSENUM_TYPES)]
  isq: ClonedVar,

  output: ClonedVar,
}

impl Default for ModelShard {
  fn default() -> Self {
    Self {
      required: ExposedTypes::new(),
      isq: ISQBitsEnum::None.into(),
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
    &AI_MODEL_TYPE_VEC
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

    let mut builder = ModelBuilder::new(model_id).with_logging();

    match isq_bits {
      ISQBitsEnum::None => {}
      ISQBitsEnum::Two => { builder = builder.with_auto_isq(IsqBits::Two); }
      ISQBitsEnum::Four => { builder = builder.with_auto_isq(IsqBits::Four); }
      ISQBitsEnum::Eight => { builder = builder.with_auto_isq(IsqBits::Eight); }
    }

    let model = BlockingModel::from_auto_builder(builder).map_err(|e| {
      shlog_error!("Failed to load model: {}", e);
      "Failed to load model"
    })?;

    self.output = Var::new_ref_counted(AIModel(model), &*AI_MODEL_TYPE).into();
    Ok(Some(self.output.0))
  }
}

// --- AI.Chat ---

#[derive(shards::shard)]
#[shard_info("LLM.Chat", "Create a chat session from a loaded AI model.")]
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
    &AI_MODEL_TYPE_VEC
  }

  fn output_types(&mut self) -> &Types {
    &AI_CHAT_TYPE_VEC
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
      unsafe { &*Var::from_ref_counted_object::<AIModel>(input, &*AI_MODEL_TYPE)? };

    let chat = AIChat {
      model_var: *input,
      messages: Vec::new(),
    };

    self.output = Var::new_ref_counted(chat, &*AI_CHAT_TYPE).into();
    Ok(Some(self.output.0))
  }
}

// --- AI.AddText ---

#[derive(shards::shard)]
#[shard_info("LLM.AddText", "Add a text message to a chat session.")]
pub(crate) struct AddTextShard {
  #[shard_required]
  required: ExposedTypes,

  #[shard_param("Chat", "The chat session.", [*AI_CHAT_VAR_TYPE])]
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
      &mut *Var::from_ref_counted_object::<AIChat>(&self.chat.get(), &*AI_CHAT_TYPE)?
    };

    chat.messages.push((role.into(), text.to_string()));
    Ok(Some(*input))
  }
}

// --- AI.Generate ---

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
    &AI_CHAT_TYPE_VEC
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
      &mut *Var::from_ref_counted_object::<AIChat>(input, &*AI_CHAT_TYPE)?
    };

    let mut messages = TextMessages::new();
    for (role, text) in &chat.messages {
      messages = messages.add_message(role.clone(), text);
    }

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

    chat.messages.push((TextMessageRole::Assistant, text.to_string()));

    self.output = Var::ephemeral_string(text).into();
    Ok(Some(self.output.0))
  }
}

// --- AI.Reset ---

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
    &AI_CHAT_TYPE_VEC
  }

  fn output_types(&mut self) -> &Types {
    &AI_CHAT_TYPE_VEC
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
      &mut *Var::from_ref_counted_object::<AIChat>(input, &*AI_CHAT_TYPE)?
    };
    chat.messages.clear();
    Ok(Some(*input))
  }
}
