use crate::util;
use crate::util::with_possible_panic;
use crate::CONTEXTS_NAME;
use crate::PARENTS_UI_NAME;
use egui::Frame;
use egui::Id;
use egui::Rect;
use egui::Rgba;
use egui::Sense;
use egui::Stroke;
use egui::Vec2;
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
  #[shard_param(
    "ContextMenu",
    "Callback function for the right-click context menu.",
    SHARDS_OR_NONE_TYPES
  )]
  context_menu: ShardsVar,
  #[shard_param(
    "ID",
    "An optional ID value in case of ID conflicts.",
    STRING_VAR_OR_NONE_SLICE
  )]
  id: ParamVar,
  #[shard_param("FillWidth", "Fill in width.", BOOL_VAR_OR_NONE_SLICE)]
  fill_width: ParamVar,
  #[shard_param("FillHeight", "Fill in height.", BOOL_VAR_OR_NONE_SLICE)]
  fill_height: ParamVar,
  #[shard_warmup]
  contexts: ParamVar,
  #[shard_warmup]
  parents: ParamVar,
  #[shard_required]
  required: ExposedTypes,
  inner_exposed: ExposedTypes,
  last_size: Option<Vec2>,
  last_clicked: [Option<Id>; 2],
}

impl Default for Selectable {
  fn default() -> Self {
    Self {
      contents: ShardsVar::default(),
      is_selected_callback: ShardsVar::default(),
      clicked_callback: ShardsVar::default(),
      double_clicked_callback: ShardsVar::default(),
      context_menu: ShardsVar::default(),
      id: ParamVar::default(),
      contexts: ParamVar::new_named(CONTEXTS_NAME),
      parents: ParamVar::new_named(PARENTS_UI_NAME),
      required: ExposedTypes::new(),
      inner_exposed: ExposedTypes::new(),
      last_size: None,
      last_clicked: [None, None],
      fill_width: ParamVar::default(),
      fill_height: ParamVar::default(),
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

    self.context_menu.compose(&clicked_data)?;
    shards::util::require_shards_contents(&mut self.required, &self.context_menu);

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

    let id = if self.id.is_none() {
      ui.id()
    } else {
      ui.id().with(TryInto::<&str>::try_into(self.id.get())?)
    };

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

    let fill_w: bool = self.fill_width.get().try_into().unwrap_or(false);
    let fill_h: bool = self.fill_height.get().try_into().unwrap_or(false);

    // Draw frame and contents
    let inner_response = with_possible_panic(|| {
      ui.push_id(id, |ui| {
        if let Some(last_size) = self.last_size {
          let rect = Rect::from_min_size(ui.cursor().min, last_size);
          let resp = ui.interact(rect, id.with("context"), Sense::click());
          // eprintln!("SI hover: {}, clicked: {}, sc: {}", resp.hovered(), resp.clicked(), resp.secondary_clicked());

          let mut err: Option<&str> = None;
          if let Some(resp) = resp.context_menu(|ui| {
            err = util::activate_ui_contents(
              context,
              input,
              ui,
              &mut self.parents,
              &mut self.context_menu,
            )
            .err();
          }) {
            // resp = resp.response;
          }
        }
        let max_size = ui.available_size();
        // if fill_w {
        //   new_size.x = ui.available_width();
        // }
        // if fill_h {
        //   new_size.y = ui.available_height();
        // }
        let result = Frame::group(ui.style()).stroke(stroke).show(ui, |ui| {
          util::activate_ui_contents(context, input, ui, &mut self.parents, &mut self.contents)
        });
        let mut new_size = ui.min_size();
        // if fill_w {
        //   new_size.x = f32::max(new_size.x, max_size.x);
        // }
        // if fill_h {
        //   new_size.y = f32::max(new_size.y, max_size.y);
        // }
        self.last_size = Some(new_size);
        result
      })
      .inner
    })?;

    // Check for errors
    inner_response.inner?;
    let mut response = inner_response.response;

    // Find clicks in other UI elements, and ignore selection response in that case
    // let ignore_click = ui_ctx.override_selection_response.is_some();

    // if !ignore_click && !self.context_menu.is_empty() {
    //   let mut err: Option<&str> = None;
    //   if let Some(resp) = response.context_menu(|ui| {
    //     err = util::activate_ui_contents(
    //       context,
    //       input,
    //       ui,
    //       &mut self.parents,
    //       &mut self.context_menu,
    //     )
    //     .err();
    //   }) {
    //     response = resp.response;
    //   }
    // }

    /*
    if response.hovered() && !ignore_click {
      if ui.input(|i| {
        i.pointer
          .button_double_clicked(egui::PointerButton::Primary)
      }) {
        if self.last_clicked[0] == self.last_clicked[1] {
          let mut _unused = Var::default();
          if self
            .double_clicked_callback
            .activate(context, &input, &mut _unused)
            == WireState::Error
          {
            return Err("DoubleClicked callback failed");
          }
        }
      } else if ui.input(|i| {
        (i.pointer.primary_released() && !i.modifiers.any())
          || (i.pointer.primary_clicked() && i.modifiers.any())
      }) {
        let mut _unused = Var::default();
        if self
          .clicked_callback
          .activate(context, &input, &mut _unused)
          == WireState::Error
        {
          return Err("Clicked callback failed");
        }
      }

      if ui.input(|i| i.pointer.primary_clicked()) {
        self.last_clicked[1] = self.last_clicked[0];
        self.last_clicked[0] = Some(id);
      }
    } */

    // Outputs whether the contents are selected or not
    Ok(is_selected.into())
  }
}

pub fn register_shards() {
  register_shard::<Selectable>();
}
