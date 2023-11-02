use shards::{
  core::register_shard,
  types::{
    common_type, Context, ExposedTypes, InstanceData, ParamVar, Seq, ShardsVar, Type, Types, Var,
    ANYS_TYPES, BOOL_VAR_OR_NONE_SLICE, SHARDS_OR_NONE_TYPES,
  },
};

use crate::shards::shard;
use crate::shards::shard::Shard;
use crate::util::{self};

use crate::{EguiId, CONTEXTS_NAME, FLOAT_VAR_SLICE, PARENTS_UI_NAME};

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
  #[shard_param("MaxGridWidth", "Maximum grid width.", FLOAT_VAR_SLICE)]
  pub max_grid_width: ParamVar,
  #[shard_param("ItemWidth", "The width of each item.", FLOAT_VAR_SLICE)]
  pub item_width: ParamVar,
  #[shard_param("ItemHeight", "The height of each item.", FLOAT_VAR_SLICE)]
  pub item_height: ParamVar,
  #[shard_param("ColumnSpacing", "Spacing between columns.", FLOAT_VAR_SLICE)]
  pub column_spacing: ParamVar,
  #[shard_param("RowSpacing", "Spacing between rows.", FLOAT_VAR_SLICE)]
  pub row_spacing: ParamVar,
  #[shard_warmup]
  contexts: ParamVar,
  #[shard_warmup]
  parents: ParamVar,
  #[shard_required]
  required: ExposedTypes,
}

impl Default for AutoGridShard {
  fn default() -> Self {
    Self {
      contexts: ParamVar::new_named(CONTEXTS_NAME),
      parents: ParamVar::new_named(PARENTS_UI_NAME),
      striped: ParamVar::new(false.into()),
      max_grid_width: ParamVar::default(),
      item_width: ParamVar::default(),
      item_height: ParamVar::default(),
      column_spacing: ParamVar::default(),
      row_spacing: ParamVar::default(),
      required: Vec::new(),
      contents: ShardsVar::default(),
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

    Ok(())
  }

  fn cleanup(&mut self, ctx: Option<&Context>) -> Result<(), &str> {
    self.cleanup_helper(ctx)?;

    Ok(())
  }

  fn compose(&mut self, data: &InstanceData) -> Result<Type, &str> {
    self.compose_helper(data)?;
    util::require_parents(&mut self.required);

    let input_type = data.inputType;
    let slice = unsafe {
      let ptr = input_type.details.seqTypes.elements;
      std::slice::from_raw_parts(ptr, input_type.details.seqTypes.len as usize)
    };

    let element_type = if unsafe { input_type.details.seqTypes.len == 1 } {
      slice[0]
    } else {
      common_type::any
    };

    let mut data = *data;
    data.inputType = element_type;
    self.contents.compose(&data)?;

    if self.item_width.is_none() {
      return Err("Item width is required");
    }

    shards::util::require_shards_contents(&mut self.required, &self.contents);

    // Always passthrough the input
    Ok(input_type)
  }

  fn activate(&mut self, context: &Context, input: &Var) -> Result<Var, &str> {
    if self.contents.is_empty() {
      return Ok(*input);
    }

    if let Some(ui) = util::get_current_parent_opt(self.parents.get())? {
      let mut grid = egui::Grid::new(EguiId::new(self, 0));
      let seq = Seq::try_from(input)?;

      let striped = self.striped.get();
      if !striped.is_none() {
        grid = grid.striped(striped.try_into()?);
      }

      let column_spacing = self.column_spacing.get();
      let column_spacing: f32 = if !column_spacing.is_none() {
        column_spacing.try_into()?
      } else {
        ui.style().spacing.item_spacing.x
      };

      let row_spacing = self.row_spacing.get();
      let row_spacing: f32 = if !row_spacing.is_none() {
        row_spacing.try_into()?
      } else {
        ui.style().spacing.item_spacing.y
      };

      grid = grid.spacing(egui::Vec2::new(column_spacing, row_spacing));

      let item_width_var = self.item_width.get();
      let item_width: f32 = item_width_var.try_into().unwrap();

      grid = grid.min_col_width(item_width);

      let item_height_var = self.item_height.get();
      if let Some(item_height) = TryInto::<f32>::try_into(item_height_var).ok() {
        grid = grid.min_row_height(item_height);
      }

      let max_grid_width = self.max_grid_width.get();
      let max_grid_width = if !max_grid_width.is_none() {
        max_grid_width.try_into()?
      } else {
        ui.available_width()
      };

      grid
        .show(ui, |ui| {
          let mut current_width: f32 = 0.0;
          // Pass each element in the given sequence to the shards in the contents and render
          for i in 0..seq.len() {
            // Check if contents will exceed max grid width if added onto current row
            if (current_width + item_width > max_grid_width) {
              ui.end_row();
              current_width = 0.0;
            }

            let input_elem = &seq[i];

            ui.push_id(i, |ui| {
              ui.set_min_width(item_width);
              ui.set_max_width(item_width);
              let response = util::activate_ui_contents(
                context,
                input_elem,
                ui,
                &mut self.parents,
                &mut self.contents,
              );
              ui.allocate_space(ui.available_size());
              response
            })
            .inner?;

            current_width += item_width;
          }
          Ok::<(), &'static str>(())
        })
        .inner?;

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
