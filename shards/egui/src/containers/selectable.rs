use crate::util;
use crate::CONTEXTS_NAME;
use crate::PARENTS_UI_NAME;
use egui::Frame;
use egui::Rgba;
use egui::Sense;
use egui::Stroke;
use shards::core::register_shard;
use shards::shard::Shard;
use shards::types::BOOL_TYPES;
use shards::types::*;

#[derive(shards::shard)]
#[shard_info(
  "UI.Selectable",
  "A wrapper that detects selection over the provided contents."
)]
struct Selectable {
  #[shard_param(
    "Contents",
    "The UI contents to wrap, contain, and detect for selection.",
    SHARDS_OR_NONE_TYPES
  )]
  contents: ShardsVar,
  #[shard_param(
    "IsSelected",
    "Callback function for checking if the contents are currently selected.",
    SHARDS_OR_NONE_TYPES
  )]
  is_selected_callback: ShardsVar,
  #[shard_param(
    "Clicked",
    "Callback function for the contents of this shard is clicked.",
    SHARDS_OR_NONE_TYPES
  )]
  clicked_callback: ShardsVar,
  #[shard_param(
    "DoubleClicked",
    "Callback function for the contents of this shard is clicked.",
    SHARDS_OR_NONE_TYPES
  )]
  double_clicked_callback: ShardsVar,
  #[shard_warmup]
  contexts: ParamVar,
  #[shard_warmup]
  parents: ParamVar,
  #[shard_required]
  required: ExposedTypes,
  inner_exposed: ExposedTypes,
}

impl Default for Selectable {
  fn default() -> Self {
    Self {
      contents: ShardsVar::default(),
      is_selected_callback: ShardsVar::default(),
      clicked_callback: ShardsVar::default(),
      double_clicked_callback: ShardsVar::default(),
      contexts: ParamVar::new_named(CONTEXTS_NAME),
      parents: ParamVar::new_named(PARENTS_UI_NAME),
      required: ExposedTypes::new(),
      inner_exposed: ExposedTypes::new(),
    }
  }
}

#[shard_impl]
impl Shard for Selectable {
  fn input_types(&mut self) -> &Types {
    &ANY_TYPES
  }
  fn output_types(&mut self) -> &Types {
    &BOOL_TYPES
  }
  fn compose(&mut self, data: &InstanceData) -> Result<Type, &str> {
    self.compose_helper(data)?;

    if self.contents.is_empty() {
      return Err("Contents are required");
    }

    let contents_composed = self.contents.compose(data)?;
    shards::util::merge_exposed_types(&mut self.inner_exposed, &contents_composed.exposedInfo);
    shards::util::merge_exposed_types(&mut self.required, &contents_composed.requiredInfo);

    let clicked_data = shards::SHInstanceData {
      inputType: common_type::any,
      ..*data
    };
    self.clicked_callback.compose(&clicked_data)?;
    shards::util::require_shards_contents(&mut self.required, &self.clicked_callback);

    self.double_clicked_callback.compose(&clicked_data)?;
    shards::util::require_shards_contents(&mut self.required, &self.double_clicked_callback);

    let is_selected_data = shards::SHInstanceData {
      inputType: common_type::any,
      ..*data
    };
    let cr = self.is_selected_callback.compose(&is_selected_data)?;
    if cr.outputType != common_type::bool {
      return Err("IsSelected should return a boolean");
    }
    shards::util::require_shards_contents(&mut self.required, &self.is_selected_callback);

    Ok(contents_composed.outputType)
  }
  fn warmup(&mut self, context: &Context) -> Result<(), &str> {
    self.warmup_helper(context)?;
    Ok(())
  }
  fn cleanup(&mut self, ctx: Option<&Context>) -> Result<(), &str> {
    self.cleanup_helper(ctx)?;
    Ok(())
  }
  fn exposed_variables(&mut self) -> Option<&ExposedTypes> {
    Some(&self.inner_exposed)
  }
  fn activate(&mut self, context: &Context, input: &Var) -> Result<Var, &str> {
    let ui = util::get_parent_ui(self.parents.get())?;
    let ui_ctx = util::get_current_context(&self.contexts)?;

    // Resolve IsSelected
    let mut is_selected_var = Var::default();
    if self
      .is_selected_callback
      .activate(context, &input, &mut is_selected_var)
      == WireState::Error
    {
      return Err("IsSelected callback failed");
    }
    let is_selected: bool = (&is_selected_var).try_into().unwrap();

    let style = ui.visuals().widgets.active;
    // Draw frame as either white or gray, depending on whether the contents are selected
    let stroke = if is_selected {
      Stroke {
        color: Rgba::WHITE.into(),
        ..style.bg_stroke
      }
    } else {
      Stroke {
        color: Rgba::from_gray(0.2).into(),
        ..style.bg_stroke
      }
    };

    // Draw frame and contents
    let inner_response = Frame::group(ui.style()).stroke(stroke).show(ui, |ui| {
      util::activate_ui_contents(context, input, ui, &mut self.parents, &mut self.contents)
    });
    // Check for errors
    inner_response.inner?;
    let response = inner_response.response;

    if response.hovered() {
      if ui.input(|i| {
        i.pointer
          .button_double_clicked(egui::PointerButton::Primary)
          && i.pointer.is_still()
      }) {
        let mut _unused = Var::default();
        if self
          .double_clicked_callback
          .activate(context, &input, &mut _unused)
          == WireState::Error
        {
          return Err("DoubleClicked callback failed");
        }
      } else if ui.input(|i| i.pointer.primary_clicked()) {
        let mut _unused = Var::default();
        if self
          .clicked_callback
          .activate(context, &input, &mut _unused)
          == WireState::Error
        {
          return Err("Clicked callback failed");
        }
      }
    }

    // Outputs whether the contents are selected or not
    Ok(is_selected.into())
  }
}

pub fn register_shards() {
  register_shard::<Selectable>();
}
