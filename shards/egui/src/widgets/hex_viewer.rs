/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2022 Fragcolor Pte. Ltd. */

use super::HexViewer;
use crate::util;
use crate::HELP_OUTPUT_EQUAL_INPUT;
use crate::PARENTS_UI_NAME;
use shards::shard::LegacyShard;
use shards::shardsc::SHType_Bytes;
use shards::shardsc::SHType_Enum;
use shards::shardsc::SHType_Float;
use shards::shardsc::SHType_Float2;
use shards::shardsc::SHType_Float3;
use shards::shardsc::SHType_Float4;
use shards::shardsc::SHType_Int;
use shards::shardsc::SHType_Int16;
use shards::shardsc::SHType_Int2;
use shards::shardsc::SHType_Int3;
use shards::shardsc::SHType_Int4;
use shards::shardsc::SHType_Int8;
use shards::shardsc::SHType_String;
use shards::types::common_type;
use shards::types::Context;
use shards::types::ExposedTypes;
use shards::types::InstanceData;
use shards::types::OptionalString;
use shards::types::ParamVar;
use shards::types::Type;
use shards::types::Types;
use shards::types::Var;

lazy_static! {
  static ref HEXVIEWER_TYPES: Types = vec![
    common_type::bytes,
    common_type::enumeration,
    common_type::float,
    common_type::float2,
    common_type::float3,
    common_type::float4,
    common_type::int,
    common_type::int2,
    common_type::int3,
    common_type::int4,
    common_type::int8,
    common_type::int16,
    common_type::string
  ];
}

impl Default for HexViewer {
  fn default() -> Self {
    let mut parents = ParamVar::default();
    parents.set_name(PARENTS_UI_NAME);
    Self {
      parents,
      requiring: Vec::new(),
      editor: None,
    }
  }
}

impl LegacyShard for HexViewer {
  fn registerName() -> &'static str
  where
    Self: Sized,
  {
    cstr!("UI.HexViewer")
  }

  fn hash() -> u32
  where
    Self: Sized,
  {
    compile_time_crc32::crc32!("UI.HexViewer-rust-0x20200101")
  }

  fn name(&mut self) -> &str {
    "UI.HexViewer"
  }

  fn help(&mut self) -> OptionalString {
    OptionalString(shccstr!("Displays an hexadecimal viewer of data."))
  }

  fn inputTypes(&mut self) -> &Types {
    &HEXVIEWER_TYPES
  }

  fn inputHelp(&mut self) -> OptionalString {
    OptionalString(shccstr!(
      "The data to visualize. Supports primitive types only (floats, ints, string and bytes)."
    ))
  }

  fn outputTypes(&mut self) -> &Types {
    &HEXVIEWER_TYPES
  }

  fn outputHelp(&mut self) -> OptionalString {
    *HELP_OUTPUT_EQUAL_INPUT
  }

  fn hasCompose() -> bool {
    true
  }

  fn compose(&mut self, data: &InstanceData) -> Result<Type, &str> {
    Ok(data.inputType)
  }

  fn requiredVariables(&mut self) -> Option<&ExposedTypes> {
    self.requiring.clear();

    // Add UI.Parents to the list of required variables
    util::require_parents(&mut self.requiring);

    Some(&self.requiring)
  }

  fn warmup(&mut self, context: &Context) -> Result<(), &str> {
    self.parents.warmup(context);

    Ok(())
  }

  fn cleanup(&mut self, ctx: Option<&Context>) -> Result<(), &str> {
    self.parents.cleanup(ctx);

    Ok(())
  }

  fn activate(&mut self, _context: &Context, input: &Var) -> Result<Option<Var>, &str> {
    use egui_memory_editor::MemoryEditor;

    if let Some(ui) = util::get_current_parent_opt(self.parents.get())? {
      let mem = unsafe {
        let (data, len) = match input.valueType {
          SHType_Bytes => (
            input.payload.__bindgen_anon_1.__bindgen_anon_4.bytesValue,
            input.payload.__bindgen_anon_1.__bindgen_anon_4.bytesSize as usize,
          ),
          SHType_Enum => (
            &input.payload.__bindgen_anon_1.__bindgen_anon_3.enumValue as *const i32 as *mut u8,
            4,
          ),
          SHType_Float => (
            &input.payload.__bindgen_anon_1.floatValue as *const f64 as *mut u8,
            8,
          ),
          SHType_Float2 => (
            &input.payload.__bindgen_anon_1.float2Value as *const f64 as *mut u8,
            16,
          ),
          SHType_Float3 => (
            &input.payload.__bindgen_anon_1.float3Value as *const f32 as *mut u8,
            12,
          ),
          SHType_Float4 => (
            &input.payload.__bindgen_anon_1.float4Value as *const f32 as *mut u8,
            16,
          ),
          SHType_Int => (
            &input.payload.__bindgen_anon_1.intValue as *const i64 as *mut u8,
            8,
          ),
          SHType_Int2 => (
            &input.payload.__bindgen_anon_1.int2Value as *const i64 as *mut u8,
            16,
          ),
          SHType_Int3 => (
            &input.payload.__bindgen_anon_1.int3Value as *const i32 as *mut u8,
            12,
          ),
          SHType_Int4 => (
            &input.payload.__bindgen_anon_1.int4Value as *const i32 as *mut u8,
            16,
          ),
          SHType_Int8 => (
            &input.payload.__bindgen_anon_1.int8Value as *const i16 as *mut u8,
            16,
          ),
          SHType_Int16 => (
            &input.payload.__bindgen_anon_1.int16Value as *const i8 as *mut u8,
            16,
          ),
          SHType_String => (
            input.payload.__bindgen_anon_1.__bindgen_anon_2.stringValue as *mut u8,
            input.payload.__bindgen_anon_1.__bindgen_anon_2.stringLen as usize,
          ),
          _ => unreachable!(),
        };
        std::slice::from_raw_parts_mut(data, len)
      };

      let (mem_editor, range) = self
        .editor
        .get_or_insert_with(|| (MemoryEditor::new(), 0..0));
      let mem_range = 0..mem.len();
      if *range != mem_range {
        *range = mem_range.clone();
        mem_editor.set_address_range("All", mem_range);
      }
      mem_editor.draw_editor_contents_read_only(ui, mem, |mem, address| mem[address].into());

      Ok(None)
    } else {
      Err("No UI parent")
    }
  }
}
