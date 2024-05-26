/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2022 Fragcolor Pte. Ltd. */

use super::Table;
use crate::util;
use crate::EguiId;
use crate::HELP_OUTPUT_EQUAL_INPUT;
use crate::INT_VAR_OR_NONE_SLICE;
use crate::PARENTS_UI_NAME;
use shards::shard::LegacyShard;
use shards::shardsc::SHType_Int;
use shards::shardsc::SHType_Seq;
use shards::shardsc::SHType_ShardRef;
use shards::shardsc::SHType_String;
use shards::types::common_type;
use shards::types::ClonedVar;
use shards::types::Context;
use shards::types::ExposedInfo;
use shards::types::ExposedTypes;
use shards::types::InstanceData;
use shards::types::OptionalString;
use shards::types::ParamVar;
use shards::types::Parameters;
use shards::types::Seq;
use shards::types::ShardsVar;
use shards::types::Type;
use shards::types::Types;
use shards::types::Var;
use shards::types::ANYS_TYPES;
use shards::types::BOOL_VAR_OR_NONE_SLICE;
use shards::types::SEQ_OF_ANY_TABLE_TYPES;
use shards::types::SEQ_OF_SHARDS_TYPES;
use std::cmp::Ordering;
use std::ffi::CStr;
use shards::util::from_raw_parts;

lazy_static! {
  static ref TABLE_PARAMETERS: Parameters = vec![
    (
      cstr!("Builder"),
      shccstr!("Sequence of shards to build each column, repeated for each row."),
      &SEQ_OF_SHARDS_TYPES[..]
    )
      .into(),
    (
      cstr!("Columns"),
      shccstr!("Configuration of the columns."),
      &SEQ_OF_ANY_TABLE_TYPES[..]
    )
      .into(),
    (
      cstr!("Striped"),
      shccstr!("Whether to alternate a subtle background color to every other row."),
      BOOL_VAR_OR_NONE_SLICE,
    )
      .into(),
    (
      cstr!("Resizable"),
      shccstr!("Whether columns can be resized within their specified range."),
      BOOL_VAR_OR_NONE_SLICE,
    )
      .into(),
    (
      cstr!("RowIndex"),
      shccstr!("Variable to hold the row index, to be used within Rows."),
      INT_VAR_OR_NONE_SLICE,
    )
      .into(),
  ];
}

impl Default for Table {
  fn default() -> Self {
    let mut parents = ParamVar::default();
    parents.set_name(PARENTS_UI_NAME);
    let mut row_index = ParamVar::default();
    row_index.set_name("Table.RowIndex");
    Self {
      parents,
      requiring: Vec::new(),
      rows: Seq::new().as_ref().into(),
      columns: ClonedVar::default(),
      striped: ParamVar::default(),
      resizable: ParamVar::default(),
      row_index,
      shards: Vec::new(),
      header_shards: Vec::new(),
      exposing: Vec::new(),
    }
  }
}

impl LegacyShard for Table {
  fn registerName() -> &'static str
  where
    Self: Sized,
  {
    cstr!("UI.Table")
  }

  fn hash() -> u32
  where
    Self: Sized,
  {
    compile_time_crc32::crc32!("UI.Table-rust-0x20200101")
  }

  fn name(&mut self) -> &str {
    "UI.Table"
  }

  fn help(&mut self) -> OptionalString {
    OptionalString(shccstr!("Table layout."))
  }

  fn inputTypes(&mut self) -> &Types {
    &ANYS_TYPES
  }

  fn inputHelp(&mut self) -> OptionalString {
    OptionalString(shccstr!(
      "The value that will be passed to the Columns and Rows shards of the table."
    ))
  }

  fn outputTypes(&mut self) -> &Types {
    &ANYS_TYPES
  }

  fn outputHelp(&mut self) -> OptionalString {
    *HELP_OUTPUT_EQUAL_INPUT
  }

  fn parameters(&mut self) -> Option<&Parameters> {
    Some(&TABLE_PARAMETERS)
  }

  fn setParam(&mut self, index: i32, value: &Var) -> Result<(), &str> {
    match index {
      0 => {
        self.shards.clear();

        let seq = Seq::try_from(value)?;

        for shard in seq.iter() {
          let mut s = ShardsVar::default();
          s.set_param(&shard)?;
          self.shards.push(s);
        }

        Ok(self.rows = value.into())
      }
      1 => {
        self.header_shards.clear();

        if let Ok(columns) = Seq::try_from(value) {
          for column in columns.iter() {
            let column: shards::types::Table = column.as_ref().try_into()?;
            if let Some(header) = column.get_static("Header") {
              match header.valueType {
                SHType_String => {
                  self.header_shards.push(None);
                }
                SHType_ShardRef | SHType_Seq => {
                  let mut s = ShardsVar::default();
                  s.set_param(&header)?;
                  self.header_shards.push(Some(s));
                }
                _ => unreachable!(),
              }
            } else {
              self.header_shards.push(None);
            }
          }
        }

        Ok(self.columns = value.into())
      }
      2 => self.striped.set_param(value),
      3 => self.resizable.set_param(value),
      4 => self.row_index.set_param(value),
      _ => Err("Invalid parameter index"),
    }
  }

  fn getParam(&mut self, index: i32) -> Var {
    match index {
      0 => self.rows.0,
      1 => self.columns.0,
      2 => self.striped.get_param(),
      3 => self.resizable.get_param(),
      4 => self.row_index.get_param(),
      _ => Var::default(),
    }
  }

  fn requiredVariables(&mut self) -> Option<&ExposedTypes> {
    self.requiring.clear();

    // Add UI.Parents to the list of required variables
    util::require_parents(&mut self.requiring);

    Some(&self.requiring)
  }

  fn exposedVariables(&mut self) -> Option<&ExposedTypes> {
    self.exposing.clear();

    let mut exposed = false;
    for s in &self.shards {
      exposed |= util::expose_contents_variables(&mut self.exposing, s);
    }

    if exposed {
      Some(&self.exposing)
    } else {
      None
    }
  }

  fn hasCompose() -> bool {
    true
  }

  fn compose(&mut self, data: &InstanceData) -> Result<Type, &str> {
    // validate

    if !self.columns.0.is_none() {
      let columns = Seq::try_from(&self.columns.0)?;
      let rows = Seq::try_from(&self.rows.0)?;
      if columns.len() != rows.len() {
        return Err("Columns and Builder parameters must have the same length.");
      }
    }

    // compose header shards
    for header_shard in &mut self.header_shards {
      if let Some(header_shard) = header_shard {
        header_shard.compose(data)?;
      }
    }

    // compose row shards
    let mut data = *data;
    let mut shared: ExposedTypes;

    // we need to inject the row index to the inner shards
    if self.row_index.is_variable() {
      // clone shared into a new vector we can append things to
      shared = data.shared.into();
      let mut should_expose = true;
      for var in &shared {
        let (a, b) = unsafe {
          (
            CStr::from_ptr(var.name),
            CStr::from_ptr(self.row_index.get_name()),
          )
        };
        if CStr::cmp(a, b) == Ordering::Equal {
          should_expose = false;
          if var.exposedType.basicType != SHType_Int {
            return Err("Table: int variable required.");
          }
          break;
        }
      }

      if should_expose {
        // expose row index
        let index_info = ExposedInfo {
          exposedType: common_type::int,
          name: self.row_index.get_name(),
          help: cstr!("The row index.").into(),
          isMutable: false,
          isProtected: false,
          global: false,
          exposed: false,
        };
        shared.push(index_info);
        // update shared
        data.shared = (&shared).into();
      }
    }

    let input_type = data.inputType;
    let slice = unsafe {
      let ptr = input_type.details.seqTypes.elements;
      from_raw_parts(ptr, input_type.details.seqTypes.len as usize)
    };
    let element_type = if !slice.is_empty() {
      slice[0]
    } else {
      common_type::any
    };

    for s in &mut self.shards {
      data.inputType = element_type;
      s.compose(&data)?;
    }

    // Always passthrough the input
    Ok(input_type)
  }

  fn warmup(&mut self, ctx: &Context) -> Result<(), &str> {
    self.parents.warmup(ctx);

    self.striped.warmup(ctx);
    self.resizable.warmup(ctx);
    self.row_index.warmup(ctx);

    if self.row_index.is_variable() {
      self.row_index.get_mut().valueType = common_type::int.basicType;
    }

    for s in &mut self.shards {
      s.warmup(ctx)?;
    }
    for s in &mut self.header_shards {
      if let Some(s) = s {
        s.warmup(ctx)?;
      }
    }

    Ok(())
  }

  fn cleanup(&mut self, ctx: Option<&Context>) -> Result<(), &str> {
    for s in &mut self.header_shards {
      if let Some(s) = s {
        s.cleanup(ctx);
      }
    }
    for s in &mut self.shards {
      s.cleanup(ctx);
    }

    self.row_index.cleanup(ctx);
    self.resizable.cleanup(ctx);
    self.striped.cleanup(ctx);

    self.parents.cleanup(ctx);
    Ok(())
  }

  fn activate(&mut self, context: &Context, input: &Var) -> Result<Var, &str> {
    if let Some(ui) = util::get_current_parent_opt(self.parents.get())? {
      ui.push_id(EguiId::new(self, 0), |ui| {
        use egui_extras::{Column, TableBuilder};

        let seq = Seq::try_from(input)?;

        let text_height = egui::TextStyle::Body.resolve(ui.style()).size;

        // start building a table
        let mut builder =
          TableBuilder::new(ui).cell_layout(egui::Layout::left_to_right(egui::Align::Center));
        let striped = self.striped.get();
        if !striped.is_none() {
          builder = builder.striped(striped.try_into()?);
        }
        let resizable = self.resizable.get();
        if !resizable.is_none() {
          builder = builder.resizable(resizable.try_into()?);
        }

        // configure columns
        let table = {
          let columns: Seq = self.columns.0.as_ref().try_into().unwrap_or_default();

          for column in columns.iter() {
            let column: shards::types::Table = column.as_ref().try_into()?;
            let mut size = if let Some(initial) = column.get_static("Initial") {
              Column::initial(initial.try_into()?)
            } else {
              Column::remainder()
            };
            if let Some(at_least) = column.get_static("AtLeast") {
              size = size.at_least(at_least.try_into()?);
            }
            if let Some(at_most) = column.get_static("AtMost") {
              size = size.at_most(at_most.try_into()?);
            }
            builder = builder.column(size);
          }

          // add columns
          for _ in columns.len()..self.shards.len() {
            builder = builder.column(Column::remainder());
          }

          // column headers
          let row_height = if columns.len() > 0 { 20.0 } else { 0.0 };
          builder.header(row_height, |mut header_row| {
            for i in 0..columns.len() {
              let column: shards::types::Table = columns[i].as_ref().try_into().unwrap(); // iterated successfully above, qed
              if let Some(header) = column.get_static("Header") {
                match header.valueType {
                  SHType_String => {
                    header_row.col(|ui| {
                      let text: &str = header.try_into().unwrap_or("");
                      ui.heading(text);
                    });
                  }
                  SHType_ShardRef | SHType_Seq => {
                    if let Some(header_shard) = &mut self.header_shards[i] {
                      header_row.col(|ui| {
                        let _ = util::activate_ui_contents(
                          context,
                          input,
                          ui,
                          &mut self.parents,
                          header_shard,
                        );
                      });
                    } else {
                      header_row.col(|_| {});
                    }
                  }
                  _ => unreachable!(),
                }
              } else {
                header_row.col(|_| {});
              }
            }
          })
        };

        // populate rows
        table.body(|body| {
          body.rows(text_height, seq.len(), |mut row| {
            let index = row.index();
            if self.row_index.is_variable() {
              let var: &mut i64 = self.row_index.get_mut().try_into().unwrap(); // row_index is set int during warmup, qed
              *var = index as i64;
            }
            for s in &mut self.shards {
              row.col(|ui| {
                let _ = util::activate_ui_contents(context, &seq[index], ui, &mut self.parents, s);
              });
            }
          })
        });

        Ok::<(), &str>(())
      })
      .inner?;

      // Always passthrough the input
      Ok(*input)
    } else {
      Err("No UI parent")
    }
  }
}
