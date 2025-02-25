/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2022 Fragcolor Pte. Ltd. */

use crate::util;
use crate::EguiId;
use crate::BOOL_VAR_SLICE;
use crate::FLOAT_VAR_OR_NONE_SLICE;
use crate::HELP_OUTPUT_EQUAL_INPUT;

use crate::PARENTS_UI_NAME;

use shards::core::register_shard;
use shards::shard::{Shard};

use shards::shardsc::{SHType_Seq as SHTYPE_SEQ};


use shards::types::common_type;
use shards::types::ClonedVar;
use shards::types::Context;

use shards::types::ExposedTypes;
use shards::types::InstanceData;
use shards::types::OptionalString;
use shards::types::ParamVar;

use shards::types::Seq;
use shards::types::ShardsVar;
use shards::types::Type;
use shards::types::Types;
use shards::types::Var;
use shards::types::WireState;
use shards::types::ANY_TYPES;
use shards::types::BOOL_VAR_OR_NONE_SLICE;


use shards::types::STRING_VAR_OR_NONE_SLICE;

use std::cell::RefCell;



// Thread-local context for Table2 composition
thread_local! {
    static TABLE_CONTEXT: RefCell<TableContext> = RefCell::new(TableContext::default());
}

// Struct to hold column settings
struct ColumnSettings {
  pub width_type: ParamVar,  // "auto", "initial", "exact", "remainder"
  pub width_value: ParamVar, // Value for initial/exact
  pub min_width: ParamVar,   // Minimum width
  pub max_width: ParamVar,   // Maximum width
  pub clip: ParamVar,        // Whether to clip content
  pub resizable: ParamVar,   // Whether column is resizable
}

impl ColumnSettings {
  fn warmup(&mut self, ctx: &Context) {
    self.width_type.warmup(ctx);
    self.width_value.warmup(ctx);
    self.min_width.warmup(ctx);
    self.max_width.warmup(ctx);
    self.clip.warmup(ctx);
    self.resizable.warmup(ctx);
  }

  fn compose(&mut self, data: &InstanceData, out_exp: &mut ExposedTypes) -> Result<Type, &str> {
    shards::util::collect_required_variables(&data.shared, out_exp, (&self.width_type).into())?;
    shards::util::collect_required_variables(&data.shared, out_exp, (&self.width_value).into())?;
    shards::util::collect_required_variables(&data.shared, out_exp, (&self.min_width).into())?;
    shards::util::collect_required_variables(&data.shared, out_exp, (&self.max_width).into())?;
    shards::util::collect_required_variables(&data.shared, out_exp, (&self.clip).into())?;
    shards::util::collect_required_variables(&data.shared, out_exp, (&self.resizable).into())?;
    Ok(data.inputType)
  }

  fn cleanup(&mut self, ctx: Option<&Context>) {
    self.width_type.cleanup(ctx);
    self.width_value.cleanup(ctx);
    self.min_width.cleanup(ctx);
    self.max_width.cleanup(ctx);
    self.clip.cleanup(ctx);
  }
}

struct TableContext {
  current_header: Option<ShardsVar>,
  current_column_settings: Option<ColumnSettings>,
  in_table_compose: bool,
}

impl Default for TableContext {
  fn default() -> Self {
    Self {
      current_header: None,
      current_column_settings: None,
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
      ctx.current_column_settings = None;
    });
  }

  fn exit_table_compose() {
    TABLE_CONTEXT.with(|ctx| {
      let mut ctx = ctx.borrow_mut();
      ctx.in_table_compose = false;
      ctx.current_header = None;
      ctx.current_column_settings = None;
    });
  }

  fn set_header(header: ShardsVar, settings: Option<ColumnSettings>) -> Result<(), &'static str> {
    TABLE_CONTEXT.with(|ctx| {
      let mut ctx = ctx.borrow_mut();
      if !ctx.in_table_compose {
        return Err("UI.Header can only be used within UI.Table2 columns");
      }
      if ctx.current_header.is_some() {
        return Err("Only one UI.Header allowed per column");
      }
      ctx.current_header = Some(header);
      ctx.current_column_settings = settings;
      Ok(())
    })
  }

  fn take_header() -> (Option<ShardsVar>, Option<ColumnSettings>) {
    TABLE_CONTEXT.with(|ctx| {
      let mut ctx = ctx.borrow_mut();
      (
        ctx.current_header.take(),
        ctx.current_column_settings.take(),
      )
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
    "RowHeight",
    "Height of each row in pixels. Default is text height.",
    FLOAT_VAR_OR_NONE_SLICE
  )]
  row_height: ParamVar,
  can_interact: bool,
  remap_key_seq: bool,
  column_settings: Vec<Option<ColumnSettings>>,
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
      row_height: ParamVar::default(),
      can_interact: false,
      remap_key_seq: false,
      column_settings: Vec::new(),
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

    for s in &mut self.column_settings {
      if let Some(s) = s {
        s.warmup(ctx);
      }
    }

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

    for s in &mut self.column_settings {
      if let Some(s) = s {
        s.cleanup(ctx);
      }
    }

    self.cleanup_helper(ctx)?;
    Ok(())
  }

  fn compose(&mut self, data: &InstanceData) -> Result<Type, &str> {
    self.compose_helper(data)?;

    TableContext::enter_table_compose();

    self.column_shards.clear();
    self.header_shards.clear();

    self.remap_key_seq = data.inputType.basicType == SHTYPE_SEQ;
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
        let (header, column_settings) = TableContext::take_header();
        self.header_shards.push(header);
        self.column_shards.push(column_shard);

        self.column_settings.push(column_settings);
      }
    }

    // Compose the column settings
    for settings in &mut self.column_settings {
      if let Some(settings) = settings {
        settings.compose(data, &mut self.requiring)?;
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

    let _root_id = ui.id();

    // Start building table
    let mut builder =
      TableBuilder::new(ui).cell_layout(egui::Layout::left_to_right(egui::Align::Center));

    let striped = self.striped.get();
    builder = builder.striped(striped.try_into()?);

    let resizable = self.resizable.get();
    builder = builder.resizable(resizable.try_into()?);

    if self.can_interact {
      builder = builder.sense(egui::Sense::click());
    }

    // Configure columns
    for i in 0..self.column_shards.len() {
      // Create column based on settings
      let column = if let Some(settings) = &self.column_settings[i] {
        // Configure column based on settings
        let mut col = match settings.width_type.get().try_into()? {
          "auto" => {
            if !settings.width_value.get().is_none() {
              Column::auto_with_initial_suggestion(settings.width_value.get().try_into()?)
            } else {
              Column::auto()
            }
          }
          "initial" => {
            if !settings.width_value.get().is_none() {
              Column::initial(settings.width_value.get().try_into()?)
            } else {
              Column::initial(100.0) // Default width
            }
          }
          "exact" => {
            if !settings.width_value.get().is_none() {
              Column::exact(settings.width_value.get().try_into()?)
            } else {
              Column::exact(100.0) // Default width
            }
          }
          _ => Column::remainder(),
        };

        // Apply additional settings
        if !settings.min_width.get().is_none() {
          col = col.at_least(settings.min_width.get().try_into()?);
        }

        if !settings.max_width.get().is_none() {
          col = col.at_most(settings.max_width.get().try_into()?);
        }

        if !settings.clip.get().is_none() {
          col = col.clip(settings.clip.get().try_into()?);
        }

        if !settings.resizable.get().is_none() {
          col = col.resizable(settings.resizable.get().try_into()?);
        }

        col
      } else {
        // Default column type
        Column::remainder()
      };

      builder = builder.column(column);
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
        let row_response = row.response();

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

        if row_response.double_clicked() {
          let mut _unused = Var::default();
          let _ = self
            .double_clicked_callback
            .activate(context, &idx_var, &mut _unused);
        }

        if row_response.clicked() {
          let mut _unused = Var::default();
          let _ = self
            .clicked_callback
            .activate(context, &idx_var, &mut _unused);
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

  #[shard_param(
    "WidthType",
    "Column width type: auto, initial, exact, or remainder.",
    STRING_VAR_OR_NONE_SLICE
  )]
  width_type: ParamVar,

  #[shard_param(
    "Width",
    "Width value for initial or exact width types.",
    FLOAT_VAR_OR_NONE_SLICE
  )]
  width_value: ParamVar,

  #[shard_param("MinWidth", "Minimum width of the column.", FLOAT_VAR_OR_NONE_SLICE)]
  min_width: ParamVar,

  #[shard_param("MaxWidth", "Maximum width of the column.", FLOAT_VAR_OR_NONE_SLICE)]
  max_width: ParamVar,

  #[shard_param(
    "Clip",
    "Whether to clip content that doesn't fit in the column.",
    BOOL_VAR_OR_NONE_SLICE
  )]
  clip: ParamVar,

  #[shard_param(
    "Resizable",
    "Whether this column can be resized.",
    BOOL_VAR_OR_NONE_SLICE
  )]
  resizable: ParamVar,
}

impl Default for Header {
  fn default() -> Self {
    Self {
      contents: ShardsVar::default(),
      width_type: ParamVar::default(),
      width_value: ParamVar::default(),
      min_width: ParamVar::default(),
      max_width: ParamVar::default(),
      clip: ParamVar::default(),
      resizable: ParamVar::default(),
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

    // Create column settings from parameters

    let have_settings = !self.width_type.is_none()
      || !self.width_value.is_none()
      || !self.min_width.is_none()
      || !self.max_width.is_none()
      || !self.clip.is_none()
      || !self.resizable.is_none();
    let settings = if have_settings {
      Some(ColumnSettings {
        width_type: ParamVar::new(self.width_type.parameter.0),
        width_value: ParamVar::new(self.width_value.parameter.0),
        min_width: ParamVar::new(self.min_width.parameter.0),
        max_width: ParamVar::new(self.max_width.parameter.0),
        clip: ParamVar::new(self.clip.parameter.0),
        resizable: ParamVar::new(self.resizable.parameter.0),
      })
    } else {
      None
    };

    // Store header contents and settings in context
    TableContext::set_header(self.contents.clone(), settings)?;

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
