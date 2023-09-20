use shards::{
  core::register_shard,
  types::{
    common_type, Context, ExposedTypes, InstanceData, ParamVar, Seq, ShardsVar, Type, Types, Var,
    ANYS_TYPES, BOOL_VAR_OR_NONE_SLICE, SHARDS_OR_NONE_TYPES,
  },
};

use crate::shards::shard::Shard;
use crate::util::{self};
use crate::{shards::shard, INT_VAR_OR_NONE_SLICE};

use crate::{EguiId, CONTEXTS_NAME, FLOAT2_VAR_SLICE, FLOAT_VAR_SLICE, PARENTS_UI_NAME};

#[derive(shard)]
#[shard_info("UI.AutoGrid", "Works like UI.Grid, but given a Sequence, it will, it each object in the Sequence, execute the Shard provided in its Contents and automatically wrap the generated contents when it exceeds the grid's width.")]
struct AutoGridShard {
  #[shard_param("Contents", "The UI contents to be generated and inserted in each column for each element in the given sequence.", SHARDS_OR_NONE_TYPES)]
  pub contents: ShardsVar,
  #[shard_param(
    "Striped",
    "Whether to alternate a subtle background color to every other row.",
    BOOL_VAR_OR_NONE_SLICE
  )]
  pub striped: ParamVar,
  // #[shard_param("NumColumns", "The number of columns in the grid. After this given number of elements are processed, a new row will automatically be started.", INT_VAR_OR_NONE_SLICE)]
  // pub num_columns: ParamVar,
  // #[shard_param("MinColumnWidth", "Minimum column width.", FLOAT_VAR_SLICE)]
  // pub min_column_width: ParamVar,
  // #[shard_param("MaxColumnWidth", "Maximum column width.", FLOAT_VAR_SLICE)]
  // pub max_column_width: ParamVar,
  #[shard_param("MaxGridWidth", "Maximum grid width.", FLOAT_VAR_SLICE)]
  pub max_grid_width: ParamVar,
  #[shard_param("ItemWidth", "The width of each item.", FLOAT_VAR_SLICE)]
  pub item_width: ParamVar,
  #[shard_param("Spacing", "Spacing between columns/rows.", FLOAT2_VAR_SLICE)]
  pub spacing: ParamVar,
  contexts: ParamVar,
  parents: ParamVar,
  inner_exposed: ExposedTypes,
  #[shard_required]
  required: ExposedTypes,
}

impl Default for AutoGridShard {
  fn default() -> Self {
    Self {
      contexts: ParamVar::new_named(CONTEXTS_NAME),
      parents: ParamVar::new_named(PARENTS_UI_NAME),
      striped: ParamVar::new(false.into()),
      // num_columns: ParamVar::default(),
      // min_column_width: ParamVar::default(),
      // max_column_width: ParamVar::default(),
      max_grid_width: ParamVar::default(),
      item_width: ParamVar::default(),
      spacing: ParamVar::default(),
      required: Vec::new(),
      contents: ShardsVar::default(),
      inner_exposed: ExposedTypes::new(),
    }
  }
}

#[shard_impl]
impl Shard for AutoGridShard {
  fn input_types(&mut self) -> &Types {
    &ANYS_TYPES
  }
  fn output_types(&mut self) -> &Types {
    &ANYS_TYPES
  }
  fn warmup(&mut self, context: &Context) -> Result<(), &str> {
    self.warmup_helper(context)?;
    self.contexts.warmup(context);
    self.parents.warmup(context);
    self.striped.warmup(context);
    // self.num_columns.warmup(context);
    // self.min_column_width.warmup(context);
    // self.max_column_width.warmup(context);
    self.max_grid_width.warmup(context);
    self.item_width.warmup(context);
    self.spacing.warmup(context);
    Ok(())
  }
  fn cleanup(&mut self) -> Result<(), &str> {
    self.cleanup_helper()?;
    self.contexts.cleanup();
    self.parents.cleanup();
    self.striped.cleanup();
    // self.num_columns.cleanup();
    // self.min_column_width.cleanup();
    // self.max_column_width.cleanup();
    self.max_grid_width.cleanup();
    self.spacing.cleanup();
    Ok(())
  }
  fn exposed_variables(&mut self) -> Option<&ExposedTypes> {
    Some(&self.inner_exposed)
  }
  fn compose(&mut self, data: &InstanceData) -> Result<Type, &str> {
    self.compose_helper(data)?;
    util::require_parents(&mut self.required);

    // TODO: To verify this section from table.rs if it works
    let input_type = data.inputType;
    let slice = unsafe {
      let ptr = input_type.details.seqTypes.elements;
      std::slice::from_raw_parts(ptr, input_type.details.seqTypes.len as usize)
    };
    let element_type = if !slice.is_empty() {
      slice[0]
    } else {
      common_type::any
    };

    // TODO: Notes
    // if multiple seqTypes then put as common_type::any
    // clone data? then overwrite inputType
    // let mut data = *data; already a copy

    // TODO: How to compose with contents? or no need.
    // for s in &mut self.shards {
    //   data.inputType = element_type;
    //   s.compose(&data)?;
    // }

    // we don't need variable, just need a value
    // if !self.item_width.is_variable() {
    //   return Err("Item width is required");
    // }

    self.inner_exposed.clear();
    // let output_type = self.contents.compose(data)?.outputType;
    shards::util::expose_shards_contents(&mut self.inner_exposed, &self.contents);
    shards::util::require_shards_contents(&mut self.required, &self.contents);

    Ok(input_type)
  }
  fn activate(&mut self, context: &Context, input: &Var) -> Result<Var, &str> {
    if self.contents.is_empty() {
      return Ok(*input);
    }

    let ui_ctx = util::get_current_context(&self.contexts)?;

    if let Some(ui) = util::get_current_parent_opt(self.parents.get())? {
      let mut grid = egui::Grid::new(EguiId::new(self, 0));
      let seq = Seq::try_from(input)?;

      let striped = self.striped.get();
      if !striped.is_none() {
        grid = grid.striped(striped.try_into()?);
      }

      let spacing = self.spacing.get();
      if !spacing.is_none() {
        let spacing: (f32, f32) = spacing.try_into()?;
        let spacing: egui::Vec2 = spacing.into();
        grid = grid.spacing(spacing)
      };

      // let num_columns = self.num_columns.get();
      // if !num_columns.is_none() {
      //   grid = grid.num_columns(num_columns.try_into()?);
      // };

      let item_width = self.item_width.get();
      let item_width: f32 = if !item_width.is_none() {
        item_width.try_into()?
      } else {
        return Err("Item width is required");
      };

      // Set min width accordingly to at least item_width
      // let min_column_width = self.min_column_width.get();
      // let min_column_width = if !min_column_width.is_none() {
      //   let min_column_width: f32 = min_column_width.try_into()?;
      //   if min_column_width < item_width {
      //     item_width
      //   } else {
      //     min_column_width
      //   }
      // } else {
      //   item_width
      // };
      grid = grid.min_col_width(item_width);

      // let max_column_width = self.max_column_width.get();
      // if !max_column_width.is_none() {
      //   grid = grid.max_col_width(max_column_width.try_into()?)
      // };

      let max_grid_width = self.max_grid_width.get();
      let max_grid_width = if !max_grid_width.is_none() {
        max_grid_width.try_into()?
      } else {
        ui.available_width()
      };

      if item_width > max_grid_width {
        return Err("Item size cannot be bigger than the maximum size of the grid");
      }

      // let item_width: f32 = self.item_width.get().try_into()?;
      // &seq[row_index] each of its elements]
      shlog_debug!("AutoGridShard: max_grid_width: {}", max_grid_width);

      grid.show(ui, |ui| {
        // let available_width = ui.available_width();
        // shlog_debug!("AutoGridShard: available_width: {}", available_width);
        let mut current_width: f32 = 0.0;
        // Pass each element in the given sequence to the shards in the contents and render
        for i in 0..seq.len() {
          // Check if contents will exceed max grid width if added onto current row
          if (current_width + item_width > max_grid_width) {
            ui.end_row();
            current_width = 0.0;
          }

          let input_elem = &seq[i];
          util::activate_ui_contents(
            context,
            input_elem,
            ui,
            &mut self.parents,
            &mut self.contents,
            // )?;
          )?;

          current_width += item_width;
        }
        Ok::<(), &'static str>(())
        // util::activate_ui_contents(context, input, ui, &mut self.parents, &mut self.contents)
      });
      // .inner?;

      // Always passthrough the input
      Ok(*input)
    } else {
      Err("No UI parent")
    }
  }
}

pub fn register_shards() {
  register_shard::<AutoGridShard>();
}
