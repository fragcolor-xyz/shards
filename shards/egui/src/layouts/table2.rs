/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2022 Fragcolor Pte. Ltd. */

use crate::util;
use crate::EguiId;
use crate::BOOL_VAR_SLICE;
use crate::FLOAT_VAR_OR_NONE_SLICE;
use crate::HELP_OUTPUT_EQUAL_INPUT;
use crate::INT_VAR_OR_NONE_SLICE;
use crate::PARENTS_UI_NAME;
use core::slice;
use shards::core::register_shard;
use shards::shard::{Shard, ShardGenerated, ShardGeneratedOverloads};
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
use shards::types::WireState;
use shards::types::ANY_TYPES;
use shards::types::BOOL_VAR_OR_NONE_SLICE;
use shards::types::INT_TYPES;
use shards::types::NONE_TYPES;
use shards::util::from_raw_parts_allow_null;
use std::cell::RefCell;
use std::cmp::Ordering;
use std::ffi::CStr;

// Thread-local context for Table2 composition
thread_local! {
    static TABLE_CONTEXT: RefCell<TableContext> = RefCell::new(TableContext::default());
}

struct TableContext {
  current_header: Option<ShardsVar>,
  in_table_compose: bool,
}

impl Default for TableContext {
  fn default() -> Self {
    Self {
      current_header: None,
      in_table_compose: false,
    }
  }
}

// Helper functions for context management
impl TableContext {
  fn enter_table_compose() {
    TABLE_CONTEXT.with(|ctx| {
      let mut ctx = ctx.borrow_mut();
      ctx.in_table_compose = true;
      ctx.current_header = None;
    });
  }

  fn exit_table_compose() {
    TABLE_CONTEXT.with(|ctx| {
      let mut ctx = ctx.borrow_mut();
      ctx.in_table_compose = false;
      ctx.current_header = None;
    });
  }

  fn set_header(header: ShardsVar) -> Result<(), &'static str> {
    TABLE_CONTEXT.with(|ctx| {
      let mut ctx = ctx.borrow_mut();
      if !ctx.in_table_compose {
        return Err("UI.Header can only be used within UI.Table2 columns");
      }
      if ctx.current_header.is_some() {
        return Err("Only one UI.Header allowed per column");
      }
      ctx.current_header = Some(header);
      Ok(())
    })
  }

  fn take_header() -> Option<ShardsVar> {
    TABLE_CONTEXT.with(|ctx| {
      let mut ctx = ctx.borrow_mut();
      ctx.current_header.take()
    })
  }

  fn is_inside_table_compose() -> bool {
    TABLE_CONTEXT.with(|ctx| {
      let ctx = ctx.borrow();
      ctx.in_table_compose
    })
  }
}

lazy_static::lazy_static! {
  static ref SHARDS_OR_NONE_TYPES: Vec<Type> = vec![common_type::shard, common_type::shards];
  static ref SEQ_OF_SHARDS: Type = Type::seq(&SHARDS_OR_NONE_TYPES);
  static ref SEQ_OF_SHARDS_TYPES: Vec<Type> = vec![*SEQ_OF_SHARDS];

  static ref ANY_SEQ: Type = Type::seq(&ANY_TYPES);
  static ref INPUT_TYPES: Vec<Type> = vec![common_type::int, *ANY_SEQ];
}

#[derive(shards::shard)]
#[shard_info("UI.Table2", "Table layout.")]
pub struct Table2 {
  #[shard_required]
  requiring: ExposedTypes,
  inner_exposed: ExposedTypes,
  #[shard_warmup]
  parents: ParamVar,
  #[shard_param(
    "Columns",
    "Column definitions with headers and content.",
    SEQ_OF_SHARDS_TYPES
  )]
  columns: ClonedVar,
  #[shard_param(
    "Striped",
    "Whether to alternate a subtle background color to every other row.",
    BOOL_VAR_SLICE
  )]
  striped: ParamVar,
  #[shard_param(
    "Resizable",
    "Whether columns can be resized within their specified range.",
    BOOL_VAR_SLICE
  )]
  resizable: ParamVar,
  #[shard_param("Reversed", "Whether the table is reversed.", BOOL_VAR_SLICE)]
  reversed: ParamVar,
  column_shards: Vec<ShardsVar>,
  header_shards: Vec<Option<ShardsVar>>,
  #[shard_param(
    "IsSelected",
    "Callback function for checking if a row is currently selected.",
    SHARDS_OR_NONE_TYPES
  )]
  is_selected_callback: ShardsVar,
  #[shard_param(
    "Clicked",
    "Callback function for when a row is clicked.",
    SHARDS_OR_NONE_TYPES
  )]
  clicked_callback: ShardsVar,
  #[shard_param(
    "DoubleClicked",
    "Callback function for when a row is double-clicked.",
    SHARDS_OR_NONE_TYPES
  )]
  double_clicked_callback: ShardsVar,
  #[shard_param(
    "ContextMenu",
    "Callback function for the right-click context menu on rows.",
    SHARDS_OR_NONE_TYPES
  )]
  context_menu: ShardsVar,
  #[shard_param(
    "DragData",
    "Enables dragging and sets the data for drag operations",
    ANY_TYPES
  )]
  drag_data: ShardsVar,
  #[shard_param(
    "RowHeight",
    "Height of each row in pixels. Default is text height.",
    FLOAT_VAR_OR_NONE_SLICE
  )]
  row_height: ParamVar,
  // Track last clicked for double-click detection
  last_clicked: [Option<egui::Id>; 2],
  can_interact: bool,
  can_drag: bool,
  remap_key_seq: bool,
}

impl Default for Table2 {
  fn default() -> Self {
    let mut parents = ParamVar::default();
    parents.set_name(PARENTS_UI_NAME);
    Self {
      requiring: ExposedTypes::new(),
      inner_exposed: ExposedTypes::new(),
      parents,
      columns: ClonedVar::default(),
      striped: ParamVar::new(Var::new_bool(false)),
      resizable: ParamVar::new(Var::new_bool(false)),
      reversed: ParamVar::new(Var::new_bool(false)),
      column_shards: Vec::new(),
      header_shards: Vec::new(),
      is_selected_callback: ShardsVar::default(),
      clicked_callback: ShardsVar::default(),
      double_clicked_callback: ShardsVar::default(),
      context_menu: ShardsVar::default(),
      drag_data: ShardsVar::default(),
      row_height: ParamVar::default(),
      last_clicked: [None, None],
      can_interact: false,
      can_drag: false,
      remap_key_seq: false,
    }
  }
}

#[shards::shard_impl]
impl Shard for Table2 {
  fn input_types(&mut self) -> &Types {
    &INPUT_TYPES
  }

  fn input_help(&mut self) -> OptionalString {
    OptionalString(shccstr!(
      "The values that will be passed to the Columns and Rows shards of the table, or number of items in the sequence."
    ))
  }

  fn output_types(&mut self) -> &Types {
    &INPUT_TYPES
  }

  fn output_help(&mut self) -> OptionalString {
    *HELP_OUTPUT_EQUAL_INPUT
  }

  fn exposed_variables(&mut self) -> Option<&ExposedTypes> {
    Some(&self.inner_exposed)
  }

  fn warmup(&mut self, ctx: &Context) -> Result<(), &str> {
    self.warmup_helper(ctx)?;

    for s in &mut self.column_shards {
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
    for s in &mut self.column_shards {
      s.cleanup(ctx);
    }

    self.cleanup_helper(ctx)?;
    Ok(())
  }

  fn compose(&mut self, data: &InstanceData) -> Result<Type, &str> {
    self.compose_helper(data)?;

    TableContext::enter_table_compose();

    self.column_shards.clear();
    self.header_shards.clear();

    self.remap_key_seq = data.inputType.basicType == SHType_Seq;
    let callback_type = if self.remap_key_seq {
      unsafe {
        if data.inputType.details.seqTypes.len != 1 {
          return Err("Table2 requires a sequence of one type");
        }
        *data.inputType.details.seqTypes.elements
      }
    } else {
      common_type::int

    };

    // Compose interaction callbacks with int input type
    let callback_data = InstanceData {
      inputType: callback_type,
      ..*data
    };

    let header_data = InstanceData {
      inputType: common_type::int,
      ..*data
    };

    // Process columns
    if let Ok(columns) = Seq::try_from(&self.columns.0) {
      // Each column entry is a shards sequence directly
      for column in columns.iter() {
        // Each column should be a sequence of shards
        let mut column_shard = ShardsVar::default();
        column_shard.set_param(&column)?;

        // Compose the column shards - this will trigger any UI.Header inside
        column_shard.compose(&callback_data)?;
        shards::util::require_shards_contents(&mut self.requiring, &mut column_shard);

        // Store the header if one was registered during compose
        self.header_shards.push(TableContext::take_header());

        self.column_shards.push(column_shard);
      }
    }

    TableContext::exit_table_compose();

    // Need to compose the collected headers
    for hdr in &mut self.header_shards {
      if let Some(hdr) = hdr {
        hdr.compose(&header_data)?;
        shards::util::require_shards_contents(&mut self.requiring, hdr);
      }
    }

    // Expose variables from column shards
    self.inner_exposed.clear();
    for shard in &self.column_shards {
      if let Some(exposed) = shard.get_exposing() {
        self.inner_exposed.extend_from_slice(exposed);
      }
    }

    // Required variables from column shards
    self.requiring.clear();
    util::require_parents(&mut self.requiring);
    for shard in &self.column_shards {
      if let Some(required) = shard.get_requiring() {
        self.requiring.extend_from_slice(required);
      }
    }

    // Compose IsSelected callback
    if !self.is_selected_callback.is_empty() {
      let cr = self.is_selected_callback.compose(&callback_data)?;
      if cr.outputType != common_type::bool {
        return Err("IsSelected should return a boolean");
      }
      shards::util::require_shards_contents(&mut self.requiring, &self.is_selected_callback);
    }

    // Compose other callbacks
    self.clicked_callback.compose(&callback_data)?;
    shards::util::require_shards_contents(&mut self.requiring, &self.clicked_callback);

    self.double_clicked_callback.compose(&callback_data)?;
    shards::util::require_shards_contents(&mut self.requiring, &self.double_clicked_callback);

    self.context_menu.compose(&callback_data)?;
    shards::util::require_shards_contents(&mut self.requiring, &self.context_menu);

    self.can_interact = !self.is_selected_callback.is_empty()
      || !self.clicked_callback.is_empty()
      || !self.double_clicked_callback.is_empty()
      || !self.context_menu.is_empty();

    self.can_drag = !self.drag_data.is_empty();
    if self.can_drag {
      self.drag_data.compose(&callback_data)?;
      shards::util::require_shards_contents(&mut self.requiring, &self.drag_data);
    }

    Ok(data.inputType)
  }

  fn activate(&mut self, context: &Context, input: &Var) -> Result<Option<Var>, &str> {
    let ui = util::get_parent_ui(self.parents.get())?;
    ui.push_id(EguiId::new(self, 0), |ui| {
      self.build_table(context, input, ui)
    })
    .inner?;

    Ok(Some(input.clone()))
  }
}

impl Table2 {
  fn build_table(&mut self, context: &Context, input: &Var, ui: &mut egui::Ui) -> Result<(), &str> {
    use egui_extras::{Column, TableBuilder};

    // Get row count from input
    let row_count: i64 = if self.remap_key_seq {
      input.as_seq().unwrap().len() as i64
    } else {
      input.try_into()?
    };

    // Get row height - default to text height if not specified
    let text_height = egui::TextStyle::Body.resolve(ui.style()).size;
    let row_height = if !self.row_height.get().is_none() {
      self.row_height.get().try_into()?
    } else {
      text_height
    };

    let root_id = ui.id();

    let primary_clicked = ui.input(|i| i.pointer.primary_clicked());
    let double_clicked = ui.input(|i| {
      i.pointer
        .button_double_clicked(egui::PointerButton::Primary)
    });

    // Start building table
    let mut builder =
      TableBuilder::new(ui).cell_layout(egui::Layout::left_to_right(egui::Align::Center));

    let striped = self.striped.get();
    builder = builder.striped(striped.try_into()?);

    let resizable = self.resizable.get();
    builder = builder.resizable(resizable.try_into()?);

    if self.can_drag {
      builder = builder.sense(egui::Sense::click_and_drag());
    } else if self.can_interact {
      builder = builder.sense(egui::Sense::click());
    }

    // Configure columns
    for _ in 0..self.column_shards.len() {
      builder = builder.column(Column::remainder());
    }

    // Build table with headers and content
    let table = builder.header(20.0, |mut header_row| {
      // Render headers
      for (i, header) in self.header_shards.iter_mut().enumerate() {
        let input = Var::new_int(i as i64);
        header_row.col(|ui| {
          if let Some(header) = header {
            let _ = util::activate_ui_contents(context, &input, ui, &mut self.parents, header);
          }
        });
      }
    });

    // Populate rows
    let reverse: bool = self.reversed.get().try_into()?;
    table.body(|body: egui_extras::TableBody<'_>| {
      body.rows(row_height, row_count as usize, |mut row| {
        let index = if reverse {
          row_count - 1 - row.index() as i64
        } else {
          row.index() as i64
        };
        let idx_var: Var = if self.remap_key_seq {
          input.as_seq().unwrap()[index as usize]
        } else {
          Var::new_int(index)
        };

        // Check if row is selected
        let mut is_selected = false;
        if !self.is_selected_callback.is_empty() {
          let mut is_selected_var = Var::default();
          if self
            .is_selected_callback
            .activate(context, &idx_var, &mut is_selected_var)
            == WireState::Error
          {
            return;
          }
          is_selected = (&is_selected_var).try_into().unwrap_or(false);
        }

        // Style selected rows
        row.set_selected(is_selected);

        // Render columns
        for column in &mut self.column_shards {
          row.col(|ui| {
            let _ = util::activate_ui_contents(context, &idx_var, ui, &mut self.parents, column);
          });
        }

        // Create row response for interaction
        let row_id = root_id.with(index);
        let row_response = row.response();

        // if row_response.drag_started() {

        // } else if row_response.drag_stopped() {
        //   eprintln!("drag stopped: {:?}", row_id);
        // } else if row_response.dragged() {
        //   eprintln!("dragged {:?}", row_id);
        // }
        // let inner_response = if can_drag {
        //   let mut op = DragOp::new(interact_id, ui, &ui_ctx);
        //   if op.is_dragging() {
        //     let layer_id = egui::LayerId::new(egui::Order::Tooltip, id);
        //     let inner = ui.with_layer_id(layer_id, body).inner?;
        //     op.update_dragging(layer_id, &interact_response, ui);
        //     inner
        //   } else {
        //     op.update_not_dragging(drag_data, &interact_response, ui, &ui_ctx);
        //     ui.scope(body).inner?
        //   }
        // } else {
        //   body(ui)?
        // };

        // Handle interactions
        if !self.context_menu.is_empty() {
          row_response.context_menu(|ui| {
            let _ = util::activate_ui_contents(
              context,
              &idx_var,
              ui,
              &mut self.parents,
              &mut self.context_menu,
            );
          });
        }

        if row_response.hovered() {
          let mut elem_double_clicked = false;
          if double_clicked {
            if self.last_clicked[0] == self.last_clicked[1]
              && self.last_clicked[0].is_some()
              && self.last_clicked[0].unwrap() == row_id
            {
              elem_double_clicked = true;
              let mut _unused = Var::default();
              let _ = self
                .double_clicked_callback
                .activate(context, &idx_var, &mut _unused);
            }
          }

          if !elem_double_clicked && row_response.clicked() {
            let mut _unused = Var::default();
            let _ = self
              .clicked_callback
              .activate(context, &idx_var, &mut _unused);
          }

          if primary_clicked {
            self.last_clicked[1] = self.last_clicked[0];
            self.last_clicked[0] = Some(row_id);
          }
        }
      });
    });

    Ok(())
  }
}

#[derive(shards::shard)]
#[shard_info("UI.Header", "Defines a header for a Table2 column.")]
pub struct Header {
  #[shard_param("Contents", "The UI contents for the header.", SEQ_OF_SHARDS_TYPES)]
  contents: ShardsVar,
}

impl Default for Header {
  fn default() -> Self {
    Self {
      contents: ShardsVar::default(),
    }
  }
}

#[shards::shard_impl]
impl Shard for Header {
  fn input_types(&mut self) -> &Types {
    &ANY_TYPES
  }

  fn output_types(&mut self) -> &Types {
    &ANY_TYPES
  }

  fn compose(&mut self, data: &InstanceData) -> Result<Type, &str> {
    if !TableContext::is_inside_table_compose() {
      return Err("UI.Header can only be used within UI.Table2 columns");
    }

    // Store header contents in context during compose
    TableContext::set_header(self.contents.clone())?;

    unsafe {
      // TODO: Make external function for this
      (*data.shard).inlineShardId = 1; // Make a noop shard
    }

    // Passthrough input type
    Ok(data.inputType)
  }

  fn activate(&mut self, _context: &Context, input: &Var) -> Result<Option<Var>, &str> {
    // Passthrough during activation (will be nooped in actual use)
    Ok(Some(input.clone()))
  }
}

pub(crate) fn register_shards() {
  register_shard::<Table2>();
  register_shard::<Header>();
}
