// MuScriptor audio-to-MIDI transcription shards, backed by
// candle_transformers::models::muscriptor (shards-lang/candle fork).
//
// MuScriptor.Load       — safetensors path → model object
// MuScriptor.Transcribe — mono f32 samples → note tables
// MuScriptor.ToMidi     — note tables → Standard MIDI File bytes
//
// The resampler and MIDI serializer are ported from candle's muscriptor
// example (candle-examples/examples/muscriptor/{audio,midi}.rs) — the example
// crate is not a library we can link against.

use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::{Arc, Mutex};

use candle_core::{DType, Device};
use candle_nn::VarBuilder;
use candle_transformers::generation::Sampling;
use candle_transformers::models::muscriptor::{
  tokenizer, Config, GenerateOptions, Model, FRAME_RATE, SAMPLE_RATE, SEGMENT_DURATION,
};

use shards::core::{run_blocking, BlockingShard};
use shards::fourCharacterCode;
use shards::ref_counted_object_type_impl;
use shards::shard::Shard;
use shards::shlog_error;
use shards::shlog_warn;
use shards::shstr;
use shards::types::{
  common_type, AutoSeqVar, AutoTableVar, ClonedVar, Context, ExposedTypes, InstanceData, ParamVar,
  SeqVar, TableVar, Type, Types, Var, BYTES_TYPES, FRAG_CC, SEQ_OF_FLOAT_TYPES, STRING_TYPES,
};

use crate::get_global_device;

pub struct MuScriptorModel {
  model: Mutex<Model>,
  // Model does not expose its device; kept here for the auto batch size.
  cpu: bool,
}

ref_counted_object_type_impl!(MuScriptorModel);

lazy_static! {
  pub static ref MUSCRIPTOR_TYPE: Type = Type::object(FRAG_CC, fourCharacterCode(*b"muSC"));
  pub static ref MUSCRIPTOR_TYPE_VEC: Vec<Type> = vec![*MUSCRIPTOR_TYPE];
  pub static ref MUSCRIPTOR_VAR_TYPE: Type = Type::context_variable(&MUSCRIPTOR_TYPE_VEC);

  // Note table: {pitch: Int, onset: Float, offset: Float, program: Int, drum: Bool, instrument: String}
  static ref NOTE_KEYS: Vec<Var> = vec![
    shstr!("pitch").into(),
    shstr!("onset").into(),
    shstr!("offset").into(),
    shstr!("program").into(),
    shstr!("drum").into(),
    shstr!("instrument").into(),
  ];
  static ref NOTE_VALUE_TYPES: Vec<Type> = vec![
    common_type::int,
    common_type::float,
    common_type::float,
    common_type::int,
    common_type::bool,
    common_type::string,
  ];
  static ref NOTE_TABLE_TYPE: Type = Type::table(&NOTE_KEYS, &NOTE_VALUE_TYPES);
  static ref NOTE_TABLE_TYPES: Vec<Type> = vec![*NOTE_TABLE_TYPE];
  static ref NOTES_SEQ_TYPE: Type = Type::seq(&NOTE_TABLE_TYPES);
  static ref NOTES_TYPES: Vec<Type> = vec![*NOTES_SEQ_TYPE];

  static ref INSTRUMENTS_PARAM_TYPES: Vec<Type> = vec![
    common_type::none,
    common_type::string,
    common_type::string_var,
    common_type::strings,
    common_type::strings_var,
  ];
}

// --- MuScriptor.Load ---

#[derive(shards::shard)]
#[shard_info(
  "MuScriptor.Load",
  "Load a MuScriptor audio-to-MIDI transcription model from a safetensors file. The configuration is read from a config.json next to the weights, or inferred from the weights themselves."
)]
pub(crate) struct LoadShard {
  #[shard_required]
  required: ExposedTypes,

  #[shard_param("GPU", "Whether to use the GPU (if available). On Metal the transformer runs in f16.", [common_type::bool])]
  gpu: ClonedVar,

  output: ClonedVar,
}

impl Default for LoadShard {
  fn default() -> Self {
    Self {
      required: ExposedTypes::new(),
      gpu: true.into(),
      output: ClonedVar::default(),
    }
  }
}

#[shards::shard_impl]
impl Shard for LoadShard {
  fn input_types(&mut self) -> &Types {
    &STRING_TYPES
  }

  fn output_types(&mut self) -> &Types {
    &MUSCRIPTOR_TYPE_VEC
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

  fn activate(&mut self, context: &Context, input: &Var) -> Result<Option<Var>, &str> {
    Ok(Some(run_blocking(self, context, input)))
  }
}

/// Identify a known model variant from the token-embedding shape and layer
/// count when no config.json is available (same logic as the candle example).
fn infer_config(weights: &std::path::Path) -> Result<Config, &'static str> {
  let st = unsafe { candle_core::safetensors::MmapedSafetensors::new(weights) }.map_err(|e| {
    shlog_error!("Failed to open safetensors {}: {}", weights.display(), e);
    "Failed to open safetensors file"
  })?;
  let mut dim_card = None;
  let mut num_layers = 0;
  for (name, view) in st.tensors() {
    if name == "emb.0.weight" || name == "emb.weight" {
      let shape = view.shape();
      dim_card = Some((shape[1], shape[0] - 1));
    }
    if let Some(n) = name
      .strip_prefix("transformer.layers.")
      .and_then(|s| s.split('.').next())
      .and_then(|s| s.parse::<usize>().ok())
    {
      num_layers = num_layers.max(n + 1);
    }
  }
  let Some((dim, card)) = dim_card else {
    return Err("No token embedding found in the safetensors file");
  };
  for config in [Config::small(), Config::medium(), Config::large()] {
    if config.dim == dim && config.card == card && config.num_layers == num_layers {
      return Ok(config);
    }
  }
  shlog_error!(
    "Cannot infer the MuScriptor architecture (dim={}, card={}, layers={})",
    dim,
    card,
    num_layers
  );
  Err("Cannot infer the model architecture; put a config.json next to the weights")
}

impl BlockingShard for LoadShard {
  fn activate_blocking(&mut self, _context: &Context, input: &Var) -> Result<Var, &'static str> {
    let path: &str = input.try_into()?;
    let weights = std::path::Path::new(path);
    if !weights.is_file() {
      shlog_error!("MuScriptor weights not found: {}", path);
      return Err("Weights file not found");
    }

    let config = match weights.parent().map(|p| p.join("config.json")) {
      Some(config_path) if config_path.is_file() => {
        let file = std::fs::File::open(&config_path).map_err(|e| {
          shlog_error!("Failed to open {}: {}", config_path.display(), e);
          "Failed to open config.json"
        })?;
        serde_json::from_reader(file).map_err(|e| {
          shlog_error!("Failed to parse {}: {}", config_path.display(), e);
          "Failed to parse config.json"
        })?
      }
      _ => infer_config(weights)?,
    };

    let gpu: bool = self.gpu.0.as_ref().try_into()?;
    let device = if gpu {
      get_global_device().clone()
    } else {
      Device::Cpu
    };
    // Decoding is bandwidth-bound: f16 on Metal, full precision elsewhere.
    // The conditioners always run in f32, matching the reference.
    let dtype = if device.is_metal() {
      DType::F16
    } else {
      DType::F32
    };

    let vb =
      unsafe { VarBuilder::from_mmaped_safetensors(&[weights], dtype, &device) }.map_err(|e| {
        shlog_error!("Failed to load MuScriptor weights: {}", e);
        "Failed to load model weights"
      })?;
    let vb_f32 = unsafe { VarBuilder::from_mmaped_safetensors(&[weights], DType::F32, &device) }
      .map_err(|e| {
        shlog_error!("Failed to load MuScriptor weights: {}", e);
        "Failed to load model weights"
      })?;
    let model = Model::new(&config, vb, vb_f32).map_err(|e| {
      shlog_error!("Failed to build MuScriptor model: {}", e);
      "Failed to build model"
    })?;

    self.output = Var::new_ref_counted(
      MuScriptorModel {
        model: Mutex::new(model),
        cpu: device.is_cpu(),
      },
      &*MUSCRIPTOR_TYPE,
    )
    .into();
    Ok(self.output.0)
  }
}

// --- MuScriptor.Transcribe ---

#[derive(shards::shard)]
#[shard_info(
  "MuScriptor.Transcribe",
  "Transcribe mono audio samples to MIDI notes using a MuScriptor model. Outputs a sequence of note tables: {pitch: Int, onset: Float, offset: Float, program: Int, drum: Bool, instrument: String}."
)]
pub(crate) struct TranscribeShard {
  #[shard_required]
  required: ExposedTypes,

  #[shard_param("Model", "The MuScriptor model to use.", [*MUSCRIPTOR_VAR_TYPE])]
  model: ParamVar,

  #[shard_param("SampleRate", "Sample rate of the input samples in Hz; resampled to 16000 if different.", [common_type::int])]
  sample_rate: ClonedVar,

  #[shard_param("Instruments", "Instrument names to condition the model on (e.g. \"acoustic_piano\" or [\"acoustic_piano\" \"drums\"]).", INSTRUMENTS_PARAM_TYPES)]
  instruments: ParamVar,

  #[shard_param("BatchSize", "5-second chunks transcribed per batch. 0 = auto (4 on GPU, 1 on CPU).", [common_type::int])]
  batch_size: ClonedVar,

  #[shard_param("MaxTokens", "Maximum generated tokens per chunk.", [common_type::int])]
  max_tokens: ClonedVar,

  #[shard_param("Temperature", "Sampling temperature. 0 = greedy decoding (the reference default).", [common_type::float])]
  temperature: ClonedVar,

  #[shard_param("TopP", "Top-p (nucleus) sampling; takes precedence over TopK. 0 disables.", [common_type::float])]
  top_p: ClonedVar,

  #[shard_param("TopK", "Top-k sampling. 0 disables.", [common_type::int])]
  top_k: ClonedVar,

  #[shard_param("Seed", "Random seed used when Temperature > 0.", [common_type::int])]
  seed: ClonedVar,

  cancelled: Arc<AtomicBool>,
  notes: AutoSeqVar,
}

impl Default for TranscribeShard {
  fn default() -> Self {
    Self {
      required: ExposedTypes::new(),
      model: ParamVar::default(),
      sample_rate: (SAMPLE_RATE as i64).into(),
      instruments: ParamVar::default(),
      batch_size: 0i64.into(),
      max_tokens: 2000i64.into(),
      temperature: 0.0f64.into(),
      top_p: 0.0f64.into(),
      top_k: 0i64.into(),
      seed: 299792458i64.into(),
      cancelled: Arc::new(AtomicBool::new(false)),
      notes: AutoSeqVar::new(),
    }
  }
}

#[shards::shard_impl]
impl Shard for TranscribeShard {
  fn input_types(&mut self) -> &Types {
    &SEQ_OF_FLOAT_TYPES
  }

  fn output_types(&mut self) -> &Types {
    &NOTES_TYPES
  }

  fn warmup(&mut self, ctx: &Context) -> Result<(), &str> {
    self.warmup_helper(ctx)?;
    Ok(())
  }

  fn cleanup(&mut self, ctx: Option<&Context>) -> Result<(), &str> {
    self.cleanup_helper(ctx)?;
    self.notes = AutoSeqVar::new();
    Ok(())
  }

  fn compose(&mut self, data: &InstanceData) -> Result<Type, &str> {
    self.compose_helper(data)?;
    if self.model.is_none() {
      return Err("Model is required");
    }
    Ok(self.output_types()[0])
  }

  fn activate(&mut self, context: &Context, input: &Var) -> Result<Option<Var>, &str> {
    self.cancelled.store(false, Ordering::Relaxed);
    Ok(Some(run_blocking(self, context, input)))
  }
}

fn instrument_ids(param: &mut ParamVar) -> Result<Option<Vec<u32>>, &'static str> {
  let var = param.get();
  if var.is_none() {
    return Ok(None);
  }
  let names: Vec<String> = if let Ok(s) = TryInto::<&str>::try_into(var.as_ref()) {
    s.split(',')
      .map(|s| s.trim().to_string())
      .filter(|s| !s.is_empty())
      .collect()
  } else {
    let seq: SeqVar = var.as_ref().try_into()?;
    let mut names = Vec::with_capacity(seq.len());
    for item in seq.iter() {
      let name: &str = item.as_ref().try_into()?;
      names.push(name.to_string());
    }
    names
  };
  if names.is_empty() {
    return Ok(None);
  }
  match tokenizer::instrument_class_ids(&names) {
    Ok(ids) => Ok(Some(ids)),
    Err(e) => {
      shlog_error!("MuScriptor: {}", e);
      Err("Invalid instrument name")
    }
  }
}

fn generate_options(
  max_tokens: &ClonedVar,
  temperature: &ClonedVar,
  top_p: &ClonedVar,
  top_k: &ClonedVar,
  seed: &ClonedVar,
) -> GenerateOptions {
  let max_gen_len: i64 = max_tokens.0.as_ref().try_into().unwrap_or(2000);
  let temperature: f64 = temperature.0.as_ref().try_into().unwrap_or(0.0);
  let top_p: f64 = top_p.0.as_ref().try_into().unwrap_or(0.0);
  let top_k: i64 = top_k.0.as_ref().try_into().unwrap_or(0);
  let seed: i64 = seed.0.as_ref().try_into().unwrap_or(299792458);
  // As in the reference: top-p wins over top-k, non-positive disables.
  let sampling = if temperature <= 0.0 {
    None
  } else if top_p > 0.0 {
    Some(Sampling::TopP {
      p: top_p,
      temperature,
    })
  } else if top_k > 0 {
    Some(Sampling::TopK {
      k: top_k as usize,
      temperature,
    })
  } else {
    Some(Sampling::All { temperature })
  };
  GenerateOptions {
    max_gen_len: max_gen_len.max(1) as usize,
    sampling,
    seed: seed as u64,
  }
}

fn samples_from_input(input: &Var) -> Result<Vec<f32>, &'static str> {
  let seq: SeqVar = input.try_into()?;
  let mut samples = Vec::with_capacity(seq.len());
  for item in seq.iter() {
    let v: f64 = item
      .as_ref()
      .try_into()
      .map_err(|_| "Expected float values in the audio sequence")?;
    samples.push(v as f32);
  }
  Ok(samples)
}

/// Transcribe one batch of equally-sized 16 kHz segments and advance the
/// streaming token decoder in chunk order. `last_has_next` is false only for
/// the final segment of the whole stream (its notes are not clipped at the
/// segment boundary).
#[allow(clippy::too_many_arguments)]
fn transcribe_batch(
  model: &mut Model,
  batch: &[Vec<f32>],
  first_chunk_idx: usize,
  last_has_next: bool,
  instrument_ids: Option<&[u32]>,
  opts: &GenerateOptions,
  cancelled: &AtomicBool,
  decoder: &mut tokenizer::TokenDecoder,
  events: &mut Vec<tokenizer::NoteEvent>,
) -> Result<(), &'static str> {
  let seek_time = |chunk_idx: usize| chunk_idx as f64 * SEGMENT_DURATION;
  let prefix = model
    .build_prefix(batch, instrument_ids)
    .map_err(|e| {
      shlog_error!("MuScriptor conditioning failed: {}", e);
      "Failed to build the conditioning prefix"
    })?;
  let rows = model
    .generate(&prefix, opts, |_tokens| {
      if cancelled.load(Ordering::Relaxed) {
        Err(candle_core::Error::Msg("cancelled".into()))
      } else {
        Ok(())
      }
    })
    .map_err(|e| {
      shlog_error!("MuScriptor generation failed: {}", e);
      "Generation failed"
    })?;
  for (j, row) in rows.iter().enumerate() {
    let chunk_idx = first_chunk_idx + j;
    if row.len() >= opts.max_gen_len {
      shlog_warn!(
        "MuScriptor: chunk {} (seek={:.1}s) did not emit EOS within {} tokens",
        chunk_idx,
        seek_time(chunk_idx),
        opts.max_gen_len
      );
    }
    let has_next = j + 1 < rows.len() || last_has_next;
    let next = has_next.then(|| seek_time(chunk_idx + 1));
    decoder.start_chunk(seek_time(chunk_idx), next, events);
    for &token in row {
      decoder.push(token, events);
    }
  }
  Ok(())
}

fn push_note_table(out: &mut AutoSeqVar, note: &tokenizer::Note) {
  let mut table = AutoTableVar::new();
  table
    .0
    .insert_fast_static("pitch", &(note.pitch as i64).into());
  table.0.insert_fast_static("onset", &note.onset.into());
  table.0.insert_fast_static("offset", &note.offset.into());
  table
    .0
    .insert_fast_static("program", &(note.program as i64).into());
  table.0.insert_fast_static("drum", &note.is_drum.into());
  let instrument = if note.is_drum {
    "drums".to_string()
  } else {
    tokenizer::instrument_for_program(note.program)
  };
  table
    .0
    .insert_fast_static("instrument", &Var::ephemeral_string(&instrument));
  out.0.emplace_table(table);
}

impl BlockingShard for TranscribeShard {
  fn activate_blocking(&mut self, _context: &Context, input: &Var) -> Result<Var, &'static str> {
    let mut samples = samples_from_input(input)?;
    if samples.is_empty() {
      return Err("Empty audio input");
    }

    let sample_rate: i64 = self
      .sample_rate
      .0
      .as_ref()
      .try_into()
      .unwrap_or(SAMPLE_RATE as i64);
    if sample_rate <= 0 {
      return Err("SampleRate must be positive");
    }
    if sample_rate as usize != SAMPLE_RATE {
      samples = resample(&samples, sample_rate as usize, SAMPLE_RATE);
    }

    let instrument_ids = instrument_ids(&mut self.instruments)?;
    let opts = generate_options(
      &self.max_tokens,
      &self.temperature,
      &self.top_p,
      &self.top_k,
      &self.seed,
    );

    let model_obj = unsafe {
      &*Var::from_ref_counted_object::<MuScriptorModel>(&self.model.get(), &*MUSCRIPTOR_TYPE)?
    };
    let mut model = model_obj.model.lock().map_err(|_| "Model mutex poisoned")?;

    let batch_size: i64 = self.batch_size.0.as_ref().try_into().unwrap_or(0);
    let batch_size = if batch_size > 0 {
      batch_size as usize
    } else if model_obj.cpu {
      1
    } else {
      4
    };

    // Split into fixed 5-second segments, zero-padding the last one.
    let segment_samples = (SEGMENT_DURATION * SAMPLE_RATE as f64) as usize;
    let num_chunks = samples.len().div_ceil(segment_samples).max(1);
    let chunks: Vec<Vec<f32>> = (0..num_chunks)
      .map(|i| {
        let start = i * segment_samples;
        let mut chunk = samples[start..(start + segment_samples).min(samples.len())].to_vec();
        chunk.resize(segment_samples, 0.);
        chunk
      })
      .collect();

    let cancelled = self.cancelled.clone();
    let mut decoder = tokenizer::TokenDecoder::new(FRAME_RATE);
    let mut events: Vec<tokenizer::NoteEvent> = Vec::new();

    for batch_start in (0..num_chunks).step_by(batch_size) {
      let batch = &chunks[batch_start..(batch_start + batch_size).min(num_chunks)];
      transcribe_batch(
        &mut model,
        batch,
        batch_start,
        batch_start + batch.len() < num_chunks,
        instrument_ids.as_deref(),
        &opts,
        &cancelled,
        &mut decoder,
        &mut events,
      )?;
    }
    decoder.finish(&mut events);
    drop(model);

    let notes = tokenizer::events_to_notes(&events);
    self.notes.0.clear();
    for note in &notes {
      push_note_table(&mut self.notes, note);
    }
    Ok(self.notes.0 .0)
  }

  fn cancel_activation(&mut self, _context: &Context) {
    self.cancelled.store(true, Ordering::Relaxed);
  }
}

// --- MuScriptor.Stream ---

#[derive(shards::shard)]
#[shard_info(
  "MuScriptor.Stream",
  "Streaming audio-to-MIDI transcription. Feed mono samples incrementally (e.g. from a live audio wire); the shard buffers them, transcribes each full 5-second segment as it becomes available and keeps the decoder state across activations so notes sustained across segment boundaries survive. Outputs the notes completed during this activation (often empty while buffering). Set Drain: true on the final call to flush the remaining partial segment and reset for a new stream."
)]
pub(crate) struct StreamShard {
  #[shard_required]
  required: ExposedTypes,

  #[shard_param("Model", "The MuScriptor model to use.", [*MUSCRIPTOR_VAR_TYPE])]
  model: ParamVar,

  #[shard_param("SampleRate", "Sample rate of the input samples in Hz; each segment is resampled to 16000 if different.", [common_type::int])]
  sample_rate: ClonedVar,

  #[shard_param("Instruments", "Instrument names to condition the model on (e.g. \"acoustic_piano\" or [\"acoustic_piano\" \"drums\"]).", INSTRUMENTS_PARAM_TYPES)]
  instruments: ParamVar,

  #[shard_param("MaxTokens", "Maximum generated tokens per chunk.", [common_type::int])]
  max_tokens: ClonedVar,

  #[shard_param("Temperature", "Sampling temperature. 0 = greedy decoding (the reference default).", [common_type::float])]
  temperature: ClonedVar,

  #[shard_param("TopP", "Top-p (nucleus) sampling; takes precedence over TopK. 0 disables.", [common_type::float])]
  top_p: ClonedVar,

  #[shard_param("TopK", "Top-k sampling. 0 disables.", [common_type::int])]
  top_k: ClonedVar,

  #[shard_param("Seed", "Random seed used when Temperature > 0.", [common_type::int])]
  seed: ClonedVar,

  #[shard_param("Drain", "When true, after consuming this activation's input, zero-pad and transcribe the remaining partial segment, close all open notes and reset the stream state.", [common_type::bool, common_type::bool_var])]
  drain: ParamVar,

  #[shard_param("Reset", "When true, discard all buffered samples and decoder state before consuming this activation's input.", [common_type::bool, common_type::bool_var])]
  reset: ParamVar,

  // Streaming state, persistent across activations.
  buffer: Vec<f32>,
  chunk_idx: usize,
  decoder: tokenizer::TokenDecoder,
  open: std::collections::HashMap<usize, tokenizer::NoteStart>,

  cancelled: Arc<AtomicBool>,
  notes: AutoSeqVar,
}

impl Default for StreamShard {
  fn default() -> Self {
    Self {
      required: ExposedTypes::new(),
      model: ParamVar::default(),
      sample_rate: (SAMPLE_RATE as i64).into(),
      instruments: ParamVar::default(),
      max_tokens: 2000i64.into(),
      temperature: 0.0f64.into(),
      top_p: 0.0f64.into(),
      top_k: 0i64.into(),
      seed: 299792458i64.into(),
      drain: ParamVar::new(false.into()),
      reset: ParamVar::new(false.into()),
      buffer: Vec::new(),
      chunk_idx: 0,
      decoder: tokenizer::TokenDecoder::new(FRAME_RATE),
      open: std::collections::HashMap::new(),
      cancelled: Arc::new(AtomicBool::new(false)),
      notes: AutoSeqVar::new(),
    }
  }
}

#[shards::shard_impl]
impl Shard for StreamShard {
  fn input_types(&mut self) -> &Types {
    &SEQ_OF_FLOAT_TYPES
  }

  fn output_types(&mut self) -> &Types {
    &NOTES_TYPES
  }

  fn warmup(&mut self, ctx: &Context) -> Result<(), &str> {
    self.warmup_helper(ctx)?;
    Ok(())
  }

  fn cleanup(&mut self, ctx: Option<&Context>) -> Result<(), &str> {
    self.cleanup_helper(ctx)?;
    self.reset_state();
    self.notes = AutoSeqVar::new();
    Ok(())
  }

  fn compose(&mut self, data: &InstanceData) -> Result<Type, &str> {
    self.compose_helper(data)?;
    if self.model.is_none() {
      return Err("Model is required");
    }
    Ok(self.output_types()[0])
  }

  fn activate(&mut self, context: &Context, input: &Var) -> Result<Option<Var>, &str> {
    self.cancelled.store(false, Ordering::Relaxed);
    Ok(Some(run_blocking(self, context, input)))
  }
}

impl StreamShard {
  fn reset_state(&mut self) {
    self.buffer.clear();
    self.chunk_idx = 0;
    self.decoder = tokenizer::TokenDecoder::new(FRAME_RATE);
    self.open.clear();
  }

  /// Turn newly decoded events into completed notes; starts stay pending in
  /// `open` until their matching end arrives (possibly activations later).
  /// Applies the per-note minimum-duration rule; the reference's global
  /// same-pitch overlap trimming needs the whole stream and cannot be applied
  /// here — use MuScriptor.Transcribe for offline byte-parity.
  fn emit_events(&mut self, events: Vec<tokenizer::NoteEvent>) {
    for ev in events {
      match ev {
        tokenizer::NoteEvent::Start(start) => {
          self.open.insert(start.index, start);
        }
        tokenizer::NoteEvent::End {
          end_time,
          start_index,
        } => {
          let Some(start) = self.open.remove(&start_index) else {
            continue;
          };
          let is_drum = start.instrument == "drums";
          let program = if is_drum {
            tokenizer::DRUM_PROGRAM
          } else {
            tokenizer::program_for_instrument(&start.instrument).unwrap_or(0)
          };
          let mut offset = end_time;
          if start.start_time > offset {
            offset = offset.max(start.start_time + tokenizer::MINIMUM_NOTE_DURATION_SEC);
          } else if !is_drum && offset - start.start_time < tokenizer::MINIMUM_NOTE_DURATION_SEC {
            offset = start.start_time + tokenizer::MINIMUM_NOTE_DURATION_SEC;
          }
          let note = tokenizer::Note {
            is_drum,
            program,
            onset: start.start_time,
            offset,
            pitch: start.pitch,
          };
          push_note_table(&mut self.notes, &note);
        }
      }
    }
  }
}

impl BlockingShard for StreamShard {
  fn activate_blocking(&mut self, _context: &Context, input: &Var) -> Result<Var, &'static str> {
    self.notes.0.clear();

    let reset: bool = self.reset.get().as_ref().try_into().unwrap_or(false);
    if reset {
      self.reset_state();
    }

    let sample_rate: i64 = self
      .sample_rate
      .0
      .as_ref()
      .try_into()
      .unwrap_or(SAMPLE_RATE as i64);
    if sample_rate <= 0 {
      return Err("SampleRate must be positive");
    }
    let native_rate = sample_rate as usize;
    // Segments are cut at the native rate and resampled one by one, so the
    // stream never has to resample across a segment boundary.
    let native_segment = (SEGMENT_DURATION * native_rate as f64) as usize;

    self.buffer.extend(samples_from_input(input)?);

    let drain: bool = self.drain.get().as_ref().try_into().unwrap_or(false);

    let mut segments: Vec<Vec<f32>> = Vec::new();
    let mut consumed = 0;
    while self.buffer.len() - consumed >= native_segment {
      let seg = &self.buffer[consumed..consumed + native_segment];
      segments.push(if native_rate == SAMPLE_RATE {
        seg.to_vec()
      } else {
        resample(seg, native_rate, SAMPLE_RATE)
      });
      consumed += native_segment;
    }
    if drain && self.buffer.len() > consumed {
      let mut seg = self.buffer[consumed..].to_vec();
      seg.resize(native_segment, 0.);
      segments.push(if native_rate == SAMPLE_RATE {
        seg
      } else {
        resample(&seg, native_rate, SAMPLE_RATE)
      });
      consumed = self.buffer.len();
    }
    self.buffer.drain(..consumed);

    if !segments.is_empty() {
      let instrument_ids = instrument_ids(&mut self.instruments)?;
      let opts = generate_options(
        &self.max_tokens,
        &self.temperature,
        &self.top_p,
        &self.top_k,
        &self.seed,
      );
      let model_obj = unsafe {
        &*Var::from_ref_counted_object::<MuScriptorModel>(&self.model.get(), &*MUSCRIPTOR_TYPE)?
      };
      let mut model = model_obj.model.lock().map_err(|_| "Model mutex poisoned")?;
      let cancelled = self.cancelled.clone();
      let mut events: Vec<tokenizer::NoteEvent> = Vec::new();
      transcribe_batch(
        &mut model,
        &segments,
        self.chunk_idx,
        // Unless draining, more audio may follow this activation's segments.
        !drain,
        instrument_ids.as_deref(),
        &opts,
        &cancelled,
        &mut self.decoder,
        &mut events,
      )?;
      drop(model);
      self.chunk_idx += segments.len();
      self.emit_events(events);
    }

    if drain {
      let mut events = Vec::new();
      self.decoder.finish(&mut events);
      self.emit_events(events);
      self.reset_state();
    }

    Ok(self.notes.0 .0)
  }

  fn cancel_activation(&mut self, _context: &Context) {
    self.cancelled.store(true, Ordering::Relaxed);
  }
}

/// Fractional resampling with a windowed-sinc kernel bank (Julius O. Smith's
/// algorithm, matching the `julius.resample_frac` implementation used by the
/// MuScriptor reference; ported from candle's muscriptor example).
fn resample(x: &[f32], old_sr: usize, new_sr: usize) -> Vec<f32> {
  if old_sr == new_sr || x.is_empty() {
    return x.to_vec();
  }
  fn sinc(x: f64) -> f64 {
    if x == 0. {
      1.
    } else {
      x.sin() / x
    }
  }
  let gcd = {
    let (mut a, mut b) = (old_sr, new_sr);
    while b != 0 {
      (a, b) = (b, a % b);
    }
    a
  };
  let old_sr = old_sr / gcd;
  let new_sr = new_sr / gcd;
  const ZEROS: f64 = 24.;
  const ROLLOFF: f64 = 0.945;
  // The anti-aliasing lowpass sits at rolloff * min(sr) / 2.
  let sr = old_sr.min(new_sr) as f64 * ROLLOFF;
  let width = (ZEROS * old_sr as f64 / sr).ceil() as usize;
  let kernel_len = 2 * width + old_sr;

  // One kernel per output phase within a block of new_sr samples.
  let mut kernels = vec![0f64; new_sr * kernel_len];
  for i in 0..new_sr {
    let kernel = &mut kernels[i * kernel_len..(i + 1) * kernel_len];
    let mut sum = 0.;
    for (k, v) in kernel.iter_mut().enumerate() {
      let idx = k as f64 - width as f64;
      let t = (-(i as f64) / new_sr as f64 + idx / old_sr as f64) * sr;
      let t = t.clamp(-ZEROS, ZEROS) * std::f64::consts::PI;
      let window = (t / ZEROS / 2.).cos().powi(2);
      *v = sinc(t) * window;
      sum += *v;
    }
    for v in kernel.iter_mut() {
      *v /= sum;
    }
  }

  // Replicate-pad by `width` left and `width + old_sr` right, then apply
  // each kernel with a stride of old_sr.
  let padded_len = x.len() + 2 * width + old_sr;
  let mut padded = Vec::with_capacity(padded_len);
  padded.extend(std::iter::repeat_n(x[0] as f64, width));
  padded.extend(x.iter().map(|&v| v as f64));
  padded.extend(std::iter::repeat_n(
    *x.last().unwrap() as f64,
    width + old_sr,
  ));

  let out_len = x.len() * new_sr / old_sr;
  let n_blocks = (padded_len - kernel_len) / old_sr + 1;
  let mut out = vec![0f32; out_len];
  for block in 0..n_blocks {
    let input = &padded[block * old_sr..block * old_sr + kernel_len];
    for i in 0..new_sr {
      let pos = block * new_sr + i;
      if pos >= out_len {
        break;
      }
      let kernel = &kernels[i * kernel_len..(i + 1) * kernel_len];
      out[pos] = input.iter().zip(kernel).map(|(&a, &b)| a * b).sum::<f64>() as f32;
    }
  }
  out
}

// --- MuScriptor.ToMidi ---

#[derive(shards::shard)]
#[shard_info(
  "MuScriptor.ToMidi",
  "Serialize a sequence of note tables (as produced by MuScriptor.Transcribe) to Standard MIDI File (format 0) bytes."
)]
pub(crate) struct ToMidiShard {
  #[shard_required]
  required: ExposedTypes,

  output: ClonedVar,
}

impl Default for ToMidiShard {
  fn default() -> Self {
    Self {
      required: ExposedTypes::new(),
      output: ClonedVar::default(),
    }
  }
}

#[shards::shard_impl]
impl Shard for ToMidiShard {
  fn input_types(&mut self) -> &Types {
    &NOTES_TYPES
  }

  fn output_types(&mut self) -> &Types {
    &BYTES_TYPES
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
    let seq: SeqVar = input.try_into()?;
    let mut notes = Vec::with_capacity(seq.len());
    for item in seq.iter() {
      let table: TableVar = item.as_ref().try_into()?;
      let pitch: i64 = table
        .get_static("pitch")
        .ok_or("Note table is missing 'pitch'")?
        .try_into()?;
      let onset: f64 = table
        .get_static("onset")
        .ok_or("Note table is missing 'onset'")?
        .try_into()?;
      let offset: f64 = table
        .get_static("offset")
        .ok_or("Note table is missing 'offset'")?
        .try_into()?;
      let program: i64 = table
        .get_static("program")
        .ok_or("Note table is missing 'program'")?
        .try_into()?;
      let is_drum: bool = match table.get_static("drum") {
        Some(v) => v.try_into()?,
        None => program == tokenizer::DRUM_PROGRAM as i64,
      };
      notes.push(tokenizer::Note {
        is_drum,
        program: program as i32,
        onset,
        offset,
        pitch: pitch as u8,
      });
    }
    let bytes = notes_to_midi_bytes(&notes);
    self.output = ClonedVar::new_bytes(&bytes);
    Ok(Some(self.output.0))
  }
}

// Standard MIDI File (format 0) serialization, matching the MuScriptor
// reference `note_event2midi` byte for byte: same event ordering, channel
// assignment, running status and end-of-track handling. Ported from candle's
// muscriptor example.

const TICKS_PER_BEAT: u32 = 480;
const TEMPO_US: f64 = 500_000.; // 120 bpm
const VELOCITY: u8 = 100;

#[derive(Debug, Clone)]
struct MidiEvent {
  is_drum: bool,
  program: i32,
  time: f64,
  /// 1 for onset, 0 for offset.
  velocity: u8,
  pitch: u8,
}

fn second2tick(second: f64) -> i64 {
  (second * TICKS_PER_BEAT as f64 * 1e6 / TEMPO_US).round_ties_even() as i64
}

fn push_varlen(out: &mut Vec<u8>, mut value: u64) {
  let mut bytes = vec![(value & 0x7f) as u8];
  value >>= 7;
  while value > 0 {
    bytes.push((value & 0x7f) as u8 | 0x80);
    value >>= 7;
  }
  bytes.reverse();
  out.extend(bytes);
}

fn notes_to_midi_bytes(notes: &[tokenizer::Note]) -> Vec<u8> {
  // Notes to on/off events; drums get only an onset here...
  let mut events = Vec::with_capacity(notes.len() * 2);
  for note in notes {
    events.push(MidiEvent {
      is_drum: note.is_drum,
      program: note.program,
      time: note.onset,
      velocity: 1,
      pitch: note.pitch,
    });
    if !note.is_drum {
      events.push(MidiEvent {
        is_drum: false,
        program: note.program,
        time: note.offset,
        velocity: 0,
        pitch: note.pitch,
      });
    }
  }
  // ...and a synthetic offset 10ms after each drum hit.
  let drum_offsets: Vec<MidiEvent> = events
    .iter()
    .filter(|ev| ev.is_drum)
    .map(|ev| MidiEvent {
      time: ev.time + 0.01,
      velocity: 0,
      ..ev.clone()
    })
    .collect();
  events.extend(drum_offsets);
  events.sort_by(|a, b| {
    a.time
      .total_cmp(&b.time)
      .then(a.is_drum.cmp(&b.is_drum))
      .then(a.program.cmp(&b.program))
      .then(a.velocity.cmp(&b.velocity))
      .then(a.pitch.cmp(&b.pitch))
  });

  let mut track: Vec<u8> = Vec::new();
  let mut running_status: Option<u8> = None;
  let mut push_message = |track: &mut Vec<u8>, delta: u64, status: u8, data: &[u8]| {
    push_varlen(track, delta);
    if running_status != Some(status) {
      track.push(status);
      running_status = Some(status);
    }
    track.extend_from_slice(data);
  };

  // Programs are assigned channels 0-8, 10-15 in order of first appearance;
  // drums always use channel 9. Overflow falls back to channel 15.
  let mut program_to_channel: std::collections::HashMap<i32, u8> = Default::default();
  let mut available_channels: std::collections::VecDeque<u8> = (0..9).chain(10..16).collect();
  let mut drums_initialized = false;
  let mut current_tick = 0i64;
  for ev in &events {
    let absolute_tick = second2tick(ev.time);
    let mut delta_tick = (absolute_tick - current_tick).max(0) as u64;
    current_tick = current_tick.max(absolute_tick);

    let channel = match program_to_channel.get(&ev.program) {
      Some(&ch) => ch,
      None if ev.program == tokenizer::DRUM_PROGRAM || ev.is_drum => {
        if !drums_initialized {
          push_message(&mut track, delta_tick, 0xc9, &[0]);
          delta_tick = 0;
          drums_initialized = true;
        }
        9
      }
      None => {
        let ch = available_channels.pop_front().unwrap_or(15);
        program_to_channel.insert(ev.program, ch);
        push_message(&mut track, delta_tick, 0xc0 | ch, &[ev.program as u8]);
        delta_tick = 0;
        ch
      }
    };

    let (status_nibble, velocity) = if ev.velocity > 0 {
      (0x90u8, VELOCITY)
    } else {
      (0x80u8, 0)
    };
    push_message(
      &mut track,
      delta_tick,
      status_nibble | channel,
      &[ev.pitch, velocity],
    );
  }
  // End of track meta event.
  track.extend_from_slice(&[0x00, 0xff, 0x2f, 0x00]);

  let mut out = Vec::with_capacity(track.len() + 22);
  out.extend_from_slice(b"MThd");
  out.extend_from_slice(&6u32.to_be_bytes());
  out.extend_from_slice(&0u16.to_be_bytes()); // format 0
  out.extend_from_slice(&1u16.to_be_bytes()); // one track
  out.extend_from_slice(&(TICKS_PER_BEAT as u16).to_be_bytes());
  out.extend_from_slice(b"MTrk");
  out.extend_from_slice(&(track.len() as u32).to_be_bytes());
  out.extend_from_slice(&track);
  out
}
