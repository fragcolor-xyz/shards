/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2021 Fragcolor Pte. Ltd. */

#[macro_use]
extern crate shards;

extern crate compile_time_crc32;

use shards::types::ClonedVar;
use shards::types::Context;
use shards::types::Type;
use shards::types::BYTES_TYPES;
use shards::types::STRING_TYPES;

use shards::types::Var;

use core::convert::TryInto;

use shards::core::register_shard;
use shards::shard::Shard;
use shards::types::{ExposedTypes, InstanceData, Types};

#[derive(shards::shard)]
#[shard_info(
  "PDF.ToText",
  "Extract text from a stream of bytes representing a PDF document"
)]
struct PdfToTextShard {
  #[shard_required]
  required: ExposedTypes,
  output: ClonedVar,
}

impl Default for PdfToTextShard {
  fn default() -> Self {
    Self {
      required: ExposedTypes::new(),
      output: ClonedVar::default(),
    }
  }
}

#[shards::shard_impl]
impl Shard for PdfToTextShard {
  fn input_types(&mut self) -> &Types {
    &BYTES_TYPES
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
    let bytes: &[u8] = input.try_into()?;
    let text = pdf_extract::extract_text_from_mem(bytes).map_err(|e| {
      shlog_error!("Error extracting text from PDF: {}", e);
      "Error extracting text from PDF"
    })?;
    self.output.assign_string(text.as_str());
    Ok(Some(self.output.0))
  }
}

#[no_mangle]
pub extern "C" fn shardsRegister_pdf_rust(core: *mut shards::shardsc::SHCore) {
  unsafe {
    shards::core::Core = core;
  }

  register_shard::<PdfToTextShard>();
}
