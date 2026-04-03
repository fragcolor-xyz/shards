//! Quantized BERT model for GGUF embedding files.
//!
//! Loads BERT-architecture GGUF files (e.g. e5-small-v2.Q5_K_M.gguf) and runs
//! forward passes for sentence embedding generation with mean pooling.
//!
//! GGUF tensor naming follows llama.cpp convention for BERT models.

use candle_core::{Device, Module, Result, Tensor};
use candle_nn::LayerNorm;
use candle_transformers::quantized_nn::{layer_norm, linear, Embedding};
use candle_transformers::quantized_var_builder::VarBuilder;

#[derive(Debug, Clone)]
pub struct BertConfig {
  pub hidden_size: usize,
  pub intermediate_size: usize,
  pub num_attention_heads: usize,
  pub num_hidden_layers: usize,
  pub layer_norm_eps: f64,
  pub max_position_embeddings: usize,
  pub vocab_size: usize,
  pub token_type_vocab_size: usize,
}

impl BertConfig {
  pub fn from_gguf_metadata(
    metadata: &std::collections::HashMap<String, candle_core::quantized::gguf_file::Value>,
  ) -> Result<Self> {
    let get = |key: &str| {
      metadata
        .get(key)
        .ok_or_else(|| candle_core::Error::Msg(format!("missing metadata key: {key}")))
    };

    Ok(Self {
      hidden_size: get("bert.embedding_length")?.to_u32()? as usize,
      intermediate_size: get("bert.feed_forward_length")?.to_u32()? as usize,
      num_attention_heads: get("bert.attention.head_count")?.to_u32()? as usize,
      num_hidden_layers: get("bert.block_count")?.to_u32()? as usize,
      layer_norm_eps: get("bert.attention.layer_norm_epsilon")
        .and_then(|v| v.to_f32())
        .unwrap_or(1e-12) as f64,
      max_position_embeddings: get("bert.context_length")
        .and_then(|v| v.to_u32())
        .unwrap_or(512) as usize,
      vocab_size: metadata
        .get("tokenizer.ggml.tokens")
        .map(|v| match v {
          candle_core::quantized::gguf_file::Value::Array(arr) => arr.len(),
          _ => 30522,
        })
        .unwrap_or(30522),
      token_type_vocab_size: get("tokenizer.ggml.token_type_count")
        .and_then(|v| v.to_u32())
        .unwrap_or(2) as usize,
    })
  }
}

// --- Embeddings ---

#[derive(Debug, Clone)]
struct BertEmbeddings {
  word_embeddings: Embedding,
  position_embeddings: Embedding,
  token_type_embeddings: Embedding,
  layer_norm: LayerNorm,
}

impl BertEmbeddings {
  fn load(cfg: &BertConfig, vb: &VarBuilder) -> Result<Self> {
    // GGUF names: token_embd.weight, position_embd.weight, token_types.weight, token_embd_norm.*
    let word_embeddings = Embedding::new(cfg.vocab_size, cfg.hidden_size, vb.pp("token_embd"))?;
    let position_embeddings = Embedding::new(
      cfg.max_position_embeddings,
      cfg.hidden_size,
      vb.pp("position_embd"),
    )?;
    let token_type_embeddings = Embedding::new(
      cfg.token_type_vocab_size,
      cfg.hidden_size,
      vb.pp("token_types"),
    )?;
    let layer_norm = layer_norm(cfg.hidden_size, cfg.layer_norm_eps, vb.pp("token_embd_norm"))?;
    Ok(Self {
      word_embeddings,
      position_embeddings,
      token_type_embeddings,
      layer_norm,
    })
  }

  fn forward(&self, input_ids: &Tensor, token_type_ids: &Tensor, device: &Device) -> Result<Tensor> {
    let seq_len = input_ids.dim(1)?;
    let position_ids = Tensor::arange(0u32, seq_len as u32, device)?.unsqueeze(0)?;

    let word_emb = self.word_embeddings.forward(input_ids)?;
    let pos_emb = self.position_embeddings.forward(&position_ids)?;
    let type_emb = self.token_type_embeddings.forward(token_type_ids)?;

    let embeddings = (word_emb + pos_emb + type_emb)?;
    embeddings.apply(&self.layer_norm)
  }
}

// --- Self-Attention ---

#[derive(Debug, Clone)]
struct BertSelfAttention {
  query: candle_transformers::quantized_nn::Linear,
  key: candle_transformers::quantized_nn::Linear,
  value: candle_transformers::quantized_nn::Linear,
  num_attention_heads: usize,
  attention_head_size: usize,
}

impl BertSelfAttention {
  fn load(cfg: &BertConfig, vb: VarBuilder) -> Result<Self> {
    let attention_head_size = cfg.hidden_size / cfg.num_attention_heads;
    let all_head_size = cfg.num_attention_heads * attention_head_size;
    // GGUF: blk.N.attn_q.weight, blk.N.attn_k.weight, blk.N.attn_v.weight
    let query = linear(cfg.hidden_size, all_head_size, vb.pp("attn_q"))?;
    let key = linear(cfg.hidden_size, all_head_size, vb.pp("attn_k"))?;
    let value = linear(cfg.hidden_size, all_head_size, vb.pp("attn_v"))?;
    Ok(Self {
      query,
      key,
      value,
      num_attention_heads: cfg.num_attention_heads,
      attention_head_size,
    })
  }

  fn transpose_for_scores(&self, xs: &Tensor) -> Result<Tensor> {
    let (b, seq_len, _) = xs.dims3()?;
    xs.reshape((b, seq_len, self.num_attention_heads, self.attention_head_size))?
      .permute((0, 2, 1, 3))?
      .contiguous()
  }

  fn forward(&self, xs: &Tensor) -> Result<Tensor> {
    let query = self.transpose_for_scores(&self.query.forward(xs)?)?;
    let key = self.transpose_for_scores(&self.key.forward(xs)?)?;
    let value = self.transpose_for_scores(&self.value.forward(xs)?)?;

    let scale = 1.0 / (self.attention_head_size as f64).sqrt();
    let attention_scores = (query.matmul(&key.t()?)? * scale)?;
    let attention_probs = candle_nn::ops::softmax_last_dim(&attention_scores)?;
    attention_probs
      .matmul(&value)?
      .permute((0, 2, 1, 3))?
      .flatten_from(candle_core::D::Minus2)
  }
}

// --- Encoder Layer ---

#[derive(Debug, Clone)]
struct BertLayer {
  attention: BertSelfAttention,
  attn_output: candle_transformers::quantized_nn::Linear,
  attn_output_norm: LayerNorm,
  intermediate: candle_transformers::quantized_nn::Linear,
  output: candle_transformers::quantized_nn::Linear,
  output_norm: LayerNorm,
}

impl BertLayer {
  fn load(cfg: &BertConfig, vb: VarBuilder) -> Result<Self> {
    let attention = BertSelfAttention::load(cfg, vb.clone())?;
    // GGUF: blk.N.attn_output.weight, blk.N.attn_output_norm.*, blk.N.ffn_up.*, blk.N.ffn_down.*, blk.N.layer_output_norm.*
    let attn_output = linear(cfg.hidden_size, cfg.hidden_size, vb.pp("attn_output"))?;
    let attn_output_norm =
      layer_norm(cfg.hidden_size, cfg.layer_norm_eps, vb.pp("attn_output_norm"))?;
    let intermediate = linear(cfg.hidden_size, cfg.intermediate_size, vb.pp("ffn_up"))?;
    let output = linear(cfg.intermediate_size, cfg.hidden_size, vb.pp("ffn_down"))?;
    let output_norm =
      layer_norm(cfg.hidden_size, cfg.layer_norm_eps, vb.pp("layer_output_norm"))?;
    Ok(Self {
      attention,
      attn_output,
      attn_output_norm,
      intermediate,
      output,
      output_norm,
    })
  }

  fn forward(&self, xs: &Tensor) -> Result<Tensor> {
    // Self-attention + residual + norm
    let attn_output = self.attention.forward(xs)?;
    let attn_output = self.attn_output.forward(&attn_output)?;
    let xs = (attn_output + xs)?.apply(&self.attn_output_norm)?;

    // FFN + residual + norm
    let ffn = self.intermediate.forward(&xs)?.gelu()?;
    let ffn = self.output.forward(&ffn)?;
    (ffn + xs)?.apply(&self.output_norm)
  }
}

// --- Full Model ---

#[derive(Debug, Clone)]
pub struct QuantizedBertModel {
  embeddings: BertEmbeddings,
  layers: Vec<BertLayer>,
  device: Device,
}

impl QuantizedBertModel {
  pub fn load(cfg: &BertConfig, vb: &VarBuilder) -> Result<Self> {
    let embeddings = BertEmbeddings::load(cfg, vb)?;
    let mut layers = Vec::with_capacity(cfg.num_hidden_layers);
    for i in 0..cfg.num_hidden_layers {
      let layer = BertLayer::load(cfg, vb.pp(format!("blk.{i}")))?;
      layers.push(layer);
    }
    Ok(Self {
      embeddings,
      layers,
      device: vb.device().clone(),
    })
  }

  /// Run forward pass, returning the full hidden states [batch, seq_len, hidden_size].
  pub fn forward(&self, input_ids: &Tensor, token_type_ids: &Tensor) -> Result<Tensor> {
    let mut xs = self.embeddings.forward(input_ids, token_type_ids, &self.device)?;
    for layer in &self.layers {
      xs = layer.forward(&xs)?;
    }
    Ok(xs)
  }

  /// Generate mean-pooled sentence embedding from input token IDs.
  pub fn embed(&self, input_ids: &Tensor, token_type_ids: &Tensor) -> Result<Tensor> {
    let hidden_states = self.forward(input_ids, token_type_ids)?;
    // Mean pooling over sequence dimension
    let seq_len = hidden_states.dim(1)? as f64;
    let sum = hidden_states.sum(1)?;
    sum / seq_len
  }
}
