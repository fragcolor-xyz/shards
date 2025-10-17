use crate::util;

use crate::util::with_object_stack_var;
use crate::CONTEXTS_NAME;
use crate::EGUI_UI_TYPE;
use crate::HELP_OUTPUT_EQUAL_INPUT;

use crate::PARENTS_UI_NAME;
use egui::ahash::HashMap;
use shards::core::register_shard;
use shards::fourCharacterCode;
use shards::shard;
use shards::shard::Shard;
use shards::shard_impl;
use shards::shardsc;
use shards::types::ClonedVar;
use shards::types::Context;

use shards::types::ExposedTypes;
use shards::types::InstanceData;
use shards::types::OptionalString;
use shards::types::ParamVar;
use shards::types::Seq;
use shards::types::ShardRef;
use shards::types::ShardsVar;
use shards::types::Type;
use shards::types::Types;
use shards::types::Var;
use shards::types::WireState;
use shards::types::ANY_TYPES;

use shards::types::BOOL_TYPES;
use shards::types::BYTES_TYPES;
use shards::types::FRAG_CC;
use shards::types::INT_TYPES;
use shards::types::SEQ_OF_ANY_TABLE_TYPES;
use shards::types::SHARDS_OR_NONE_TYPES;
use shards::types::STRING_OR_NONE_SLICE;
use shards::types::STRING_TYPES;

use egui_dock::NodeIndex;
use egui_dock::SurfaceIndex;
use egui_dock::TabIndex;

use std::cell::UnsafeCell;
use std::convert::TryInto;
use std::sync::Arc;

const DOCK_AREA_VAR_NAME: &'static str = "$dock-area";

#[derive(serde::Deserialize, serde::Serialize)]
struct TabRef {
  id: String, // ID if provided, otherwise title
}

#[derive(serde::Deserialize, serde::Serialize)]
struct SerializableDockState {
  tabs: Vec<TabRef>,
}

struct TabData {
  title: ParamVar,
  id: String,
  contents: ShardsVar,
  closeable: bool,
}

impl TabRef {
  fn new(id: String) -> TabRef {
    TabRef {
      id: id, // This will be set when added to the dock state
    }
  }
}

struct DockState {
  pub tabs: Arc<UnsafeCell<egui_dock::DockState<TabRef>>>,
}
ref_counted_object_type_impl!(DockState);
static DOCK_OUTPUT_TYPE: Type = Type::object(FRAG_CC, fourCharacterCode(*b"uidk"));
lazy_static! {
  static ref DOCK_OUTPUT_TYPES: Vec<Type> = vec![DOCK_OUTPUT_TYPE];
  static ref DOCK_OUTPUT_VAR_TYPE: Type = Type::context_variable(&DOCK_OUTPUT_TYPES);
  static ref DOCK_OUTPUT_VAR_TYPES: Vec<Type> = vec![DOCK_OUTPUT_VAR_TYPE.clone()];
}

#[derive(shard)]
#[shard_info(
  "UI.DockArea",
  "Serves as a container for tabs defined as a sequence of table objects."
)]
struct DockArea {
  #[shard_param(
    "Tabs",
    "Sequence of table objects defining tabs with title, id, closeable, contents fields.",
    SEQ_OF_ANY_TABLE_TYPES
  )]
  tabs_param: ClonedVar,

  exposing: ExposedTypes,
  tabs: Arc<UnsafeCell<egui_dock::DockState<TabRef>>>,
  tabs_var: ClonedVar,
  tab_data: HashMap<String, TabData>,

  #[shard_warmup]
  instance: ParamVar,
  #[shard_warmup]
  parents: ParamVar,
  #[shard_warmup]
  dock_area_var: ParamVar,
  #[shard_required]
  requiring: ExposedTypes,
}

impl Default for DockArea {
  fn default() -> Self {
    Self {
      instance: ParamVar::new_named(CONTEXTS_NAME),
      requiring: Vec::new(),
      tabs_param: ClonedVar::default(),
      parents: ParamVar::new_named(PARENTS_UI_NAME),
      dock_area_var: ParamVar::new_named(DOCK_AREA_VAR_NAME),
      exposing: Vec::new(),
      tabs: Arc::new(UnsafeCell::new(egui_dock::DockState::new(Vec::new()))),
      tabs_var: ClonedVar::default(),
      tab_data: HashMap::default(),
    }
  }
}

#[shard_impl]
impl Shard for DockArea {
  fn input_types(&mut self) -> &Types {
    &ANY_TYPES
  }

  fn input_help(&mut self) -> OptionalString {
    OptionalString(shccstr!("The input of this shard is ignored."))
  }

  fn output_types(&mut self) -> &Types {
    &DOCK_OUTPUT_TYPES
  }

  fn output_help(&mut self) -> OptionalString {
    *HELP_OUTPUT_EQUAL_INPUT
  }

  fn compose(&mut self, data: &InstanceData) -> Result<Type, &str> {
    self.compose_helper(data)?;

    // Process tabs parameter - should be a sequence of table objects
    let tabs_value = &self.tabs_param.0;

    match tabs_value.valueType {
      shardsc::SHType_None => {}
      shardsc::SHType_Seq => {
        let seq = Seq::try_from(tabs_value)?;

        // Clear existing data
        self.tab_data.clear();

        for tab_var in seq.iter() {
          // Each tab should be a table with title, id, closeable, contents fields
          let table = shards::types::Table::try_from(tab_var)?;

          // Extract title (required)
          let title = table.get_fast_static("title");
          let mut title_param = ParamVar::new(*title);

          // Extract id (optional)
          let id_value = table.get_fast_static("id");
          let tab_id: Option<String> = if id_value.valueType != shardsc::SHType_None {
            let id_str: &str = id_value.try_into().map_err(|_| "Tab id must be a string")?;
            Some(id_str.to_string())
          } else {
            None
          };

          // Extract closeable (optional, defaults to true)
          let closeable_value = table.get_fast_static("closeable");
          let is_closeable: bool = if closeable_value.valueType != shardsc::SHType_None {
            closeable_value
              .try_into()
              .map_err(|_| "Tab closeable must be a boolean")?
          } else {
            true
          };

          // Extract contents (required)
          let contents_value = table.get_fast_static("contents");
          if contents_value.valueType == shardsc::SHType_None {
            return Err("Tab table must have a 'contents' field");
          }

          let mut contents_shards: ShardsVar = ShardsVar::default();
          contents_shards.set_param(contents_value)?;

          // Manually compose the contents shards
          contents_shards.compose(data)?;

          let id = if let Some(tab_id) = tab_id {
            tab_id
          } else {
            if let Ok(fallback_id) = title.try_into() {
              fallback_id
            } else {
              return Err("Tab ID or Title must be a constant string to identify the tab");
            }
          };

          // Store the processed tab data
          self.tab_data.insert(
            id.clone(),
            TabData {
              title: title_param,
              id: id,
              contents: contents_shards, // Will be set later in warmup
              closeable: is_closeable,
            },
          );
        }
      }
      _ => return Err("Tabs parameter must be a sequence of table objects"),
    }

    util::require_context(&mut self.requiring);
    util::require_parents(&mut self.requiring);

    // Always passthrough the input
    Ok(DOCK_OUTPUT_TYPE)
  }

  fn warmup(&mut self, ctx: &Context) -> Result<(), &str> {
    self.warmup_helper(ctx)?;

    for (s, t) in &mut self.tab_data {
      t.contents.warmup(ctx)?;
      t.title.warmup(ctx);
    }

    self.tabs = Arc::new(UnsafeCell::new(egui_dock::DockState::new(Vec::new())));
    let tabs = unsafe { self.tabs.as_mut_unchecked() };

    for (i, _) in &self.tab_data {
      tabs.push_to_first_leaf(TabRef::new(i.clone()));
    }

    self.tabs_var.assign(&Var::new_ref_counted(
      DockState {
        tabs: self.tabs.clone(),
      },
      &DOCK_OUTPUT_TYPE,
    ));

    Ok(())
  }

  fn cleanup(&mut self, ctx: Option<&Context>) -> Result<(), &str> {
    self.cleanup_helper(ctx)?;
    for (k, v) in &mut self.tab_data {
      v.title.cleanup(ctx);
      v.contents.cleanup(ctx);
    }

    Ok(())
  }

  fn activate(&mut self, context: &Context, _input: &Var) -> Result<Option<Var>, &str> {
    let tabs = unsafe { self.tabs.as_mut_unchecked() };
    if tabs.surfaces_count() == 0 {
      return Ok(Some(self.tabs_var.0));
    }

    // Set the $dock-area variable to reference this dock area's state
    self.dock_area_var.assign(&self.tabs_var.0);

    util::with_object_stack_var(
      &mut self.dock_area_var,
      &self.tabs_var.0,
      &DOCK_OUTPUT_TYPE,
      || {
        let gui_ctx = util::get_current_context(&self.instance)?;
        let style = egui_dock::Style::from_egui(gui_ctx.egui_ctx.style().as_ref());

        let area = egui_dock::DockArea::new(tabs).style(style);

        let parents_stack_var = self.parents.get().clone();
        let mut viewer = MyTabViewer::new(context, &mut self.parents, &self.tab_data);
        if let Some(ui) = util::get_current_parent_opt(&parents_stack_var)? {
          area.show_inside(ui, &mut viewer);
        } else {
          area.show(&gui_ctx.egui_ctx, &mut viewer);
        }

        // Always passthrough the input
        Ok(Some(self.tabs_var.0))
      },
    )
  }
}

#[derive(shards::shard)]
#[shard_info("UI.SaveDockState", "Save the state of the DockArea")]
pub struct SaveShard {
  #[shard_param("DockState", "The state of the DockArea", DOCK_OUTPUT_VAR_TYPES)]
  dock_state: ParamVar,
  #[shard_required]
  requiring: ExposedTypes,
  data: Vec<u8>,
}

impl Default for SaveShard {
  fn default() -> Self {
    Self {
      dock_state: ParamVar::default(),
      requiring: Vec::new(),
      data: Vec::new(),
    }
  }
}

#[shards::shard_impl]
impl Shard for SaveShard {
  fn input_types(&mut self) -> &Types {
    &ANY_TYPES
  }

  fn output_types(&mut self) -> &Types {
    &BYTES_TYPES
  }

  fn compose(&mut self, data: &InstanceData) -> Result<Type, &str> {
    self.compose_helper(data)?;

    if !self.dock_state.is_variable() {
      return Err("DockState is required");
    }

    Ok(BYTES_TYPES[0])
  }

  fn warmup(&mut self, ctx: &Context) -> Result<(), &str> {
    self.warmup_helper(ctx)?;
    Ok(())
  }

  fn cleanup(&mut self, ctx: Option<&Context>) -> Result<(), &str> {
    self.cleanup_helper(ctx)?;
    Ok(())
  }

  fn activate(&mut self, _context: &Context, _input: &Var) -> Result<Option<Var>, &str> {
    let dock_state_var = self.dock_state.get();
    let dock_state = unsafe {
      (*Var::from_ref_counted_object::<DockState>(&dock_state_var, &DOCK_OUTPUT_TYPE).unwrap())
        .tabs
        .as_mut_unchecked()
    };

    // // Convert to serializable format
    // let serializable_state = unsafe {
    //   let dock_state_ref = &*dock_state.get();
    //   SerializableDockState {
    //     tabs: vec![], // For now, empty; this would need proper extraction logic
    //   }
    // };

    // Serialize the dock state data to bytes
    match bitcode::serialize(dock_state) {
      Ok(data) => {
        self.data = data.clone();
        Ok(Some(self.data[..].into()))
      }
      Err(_) => Err("Failed to serialize dock state"),
    }
  }
}

#[derive(shards::shard)]
#[shard_info("UI.RestoreDockState", "Restore the state of the DockArea")]
pub struct RestoreShard {
  #[shard_param(
    "DockState",
    "The state of the DockArea to restore to",
    DOCK_OUTPUT_VAR_TYPES
  )]
  dock_state: ParamVar,
  #[shard_required]
  requiring: ExposedTypes,
}

impl Default for RestoreShard {
  fn default() -> Self {
    Self {
      dock_state: ParamVar::default(),
      requiring: Vec::new(),
    }
  }
}

#[shards::shard_impl]
impl Shard for RestoreShard {
  fn input_types(&mut self) -> &Types {
    &BYTES_TYPES
  }

  fn output_types(&mut self) -> &Types {
    &BYTES_TYPES
  }

  fn compose(&mut self, data: &InstanceData) -> Result<Type, &str> {
    self.compose_helper(data)?;
    Ok(data.inputType)
  }

  fn warmup(&mut self, ctx: &Context) -> Result<(), &str> {
    self.warmup_helper(ctx)?;
    Ok(())
  }

  fn cleanup(&mut self, ctx: Option<&Context>) -> Result<(), &str> {
    self.cleanup_helper(ctx)?;
    Ok(())
  }

  fn activate(&mut self, _context: &Context, input: &Var) -> Result<Option<Var>, &str> {
    let dock_state_var = self.dock_state.get();
    let dock_state_obj = unsafe {
      Var::from_ref_counted_object::<DockState>(&dock_state_var, &DOCK_OUTPUT_TYPE)
        .map_err(|_| "Invalid dock state object")?
    };

    let bytes: &[u8] = input.try_into().map_err(|_| "Input must be bytes")?;

    // Deserialize the saved dock state
    let saved_state: egui_dock::DockState<TabRef> =
      bitcode::deserialize(bytes).map_err(|_| "Failed to deserialize dock state")?;

    // Restore the dock state by creating a new one from serializable state
    unsafe {
      let tabs = (*dock_state_obj).tabs.as_mut_unchecked();
      *tabs = saved_state;
    }

    Ok(Some(input.clone()))
  }
}

//
// Tab control shards for manipulating dock state
//

#[derive(shards::shard)]
#[shard_info("UI.TabIsOpen", "Check if a tab is currently open in the dock area")]
pub struct TabIsOpenShard {
  #[shard_param("TabID", "The ID or title of the tab to check", STRING_TYPES)]
  tab_id: ParamVar,
  #[shard_param(
    "DockArea",
    "The dock area to check. Defaults to $dock-area context variable.",
    DOCK_OUTPUT_VAR_TYPES
  )]
  dock_area: ParamVar,
  #[shard_required]
  requiring: ExposedTypes,
}

impl Default for TabIsOpenShard {
  fn default() -> Self {
    let mut dock_area = ParamVar::default();
    dock_area.set_name(DOCK_AREA_VAR_NAME);
    Self {
      tab_id: ParamVar::default(),
      dock_area,
      requiring: Vec::new(),
    }
  }
}

#[shards::shard_impl]
impl Shard for TabIsOpenShard {
  fn input_types(&mut self) -> &Types {
    &ANY_TYPES
  }

  fn output_types(&mut self) -> &Types {
    &shards::types::BOOL_TYPES
  }

  fn compose(&mut self, data: &InstanceData) -> Result<Type, &str> {
    self.compose_helper(data)?;
    Ok(shards::types::common_type::bool)
  }

  fn warmup(&mut self, ctx: &Context) -> Result<(), &str> {
    self.warmup_helper(ctx)?;
    Ok(())
  }

  fn cleanup(&mut self, ctx: Option<&Context>) -> Result<(), &str> {
    self.cleanup_helper(ctx)?;
    Ok(())
  }

  fn activate(&mut self, _context: &Context, input: &Var) -> Result<Option<Var>, &str> {
    let dock_state_var = self.dock_area.get();
    let dock_state = unsafe {
      Var::from_ref_counted_object::<DockState>(&dock_state_var, &DOCK_OUTPUT_TYPE)
        .map_err(|_| "Invalid dock state object")?
    };

    let tab_id: &str = self
      .tab_id
      .get()
      .try_into()
      .map_err(|_| "TabID must be a string")?;

    let tabs = unsafe { (*dock_state).tabs.as_ref().get().as_ref().unwrap() };

    // Check if a tab with the given ID exists by searching through all tabs
    let is_open = tabs.iter_all_tabs().any(|(_, tab)| {
      // Check if the tab's key matches the requested ID
      tab.id == tab_id
    });

    Ok(Some(is_open.into()))
  }
}

#[derive(shards::shard)]
#[shard_info("UI.TabOpen", "Open a tab in the dock area")]
pub struct TabOpenShard {
  #[shard_param("TabID", "The ID or title of the tab to open", STRING_TYPES)]
  tab_id: ParamVar,
  #[shard_param(
    "DockArea",
    "The dock area to modify. Defaults to $dock-area context variable.",
    DOCK_OUTPUT_VAR_TYPES
  )]
  dock_area: ParamVar,
  #[shard_required]
  requiring: ExposedTypes,
}

impl Default for TabOpenShard {
  fn default() -> Self {
    let mut dock_area = ParamVar::default();
    dock_area.set_name(DOCK_AREA_VAR_NAME);
    Self {
      tab_id: ParamVar::default(),
      dock_area,
      requiring: Vec::new(),
    }
  }
}

#[shards::shard_impl]
impl Shard for TabOpenShard {
  fn input_types(&mut self) -> &Types {
    &ANY_TYPES
  }

  fn output_types(&mut self) -> &Types {
    &ANY_TYPES
  }

  fn compose(&mut self, data: &InstanceData) -> Result<Type, &str> {
    self.compose_helper(data)?;
    Ok(data.inputType)
  }

  fn warmup(&mut self, ctx: &Context) -> Result<(), &str> {
    self.warmup_helper(ctx)?;
    Ok(())
  }

  fn cleanup(&mut self, ctx: Option<&Context>) -> Result<(), &str> {
    self.cleanup_helper(ctx)?;
    Ok(())
  }

  fn activate(&mut self, _context: &Context, input: &Var) -> Result<Option<Var>, &str> {
    let dock_state_var = self.dock_area.get();
    let dock_state = unsafe {
      Var::from_ref_counted_object::<DockState>(&dock_state_var, &DOCK_OUTPUT_TYPE)
        .map_err(|_| "Invalid dock state object")?
    };

    let tab_id: &str = self
      .tab_id
      .get()
      .try_into()
      .map_err(|_| "TabID must be a string")?;

    let tabs = unsafe { (*dock_state).tabs.as_ref().get().as_mut().unwrap() };

    // First pass: find the tab location without borrowing mutably
    let mut found_tab: Option<(SurfaceIndex, NodeIndex, TabIndex)> = None;
    for (surface_index, surface) in tabs.iter_surfaces().enumerate() {
      if let Some(tree) = surface.node_tree() {
        // Iterate through all nodes to find tabs
        for (node_idx, node) in tree.iter().enumerate() {
          if let Some(node_tabs) = node.tabs() {
            // Look for matching tab by key
            for (tab_idx, tab_ref) in node_tabs.iter().enumerate() {
              if tab_ref.id == tab_id {
                // Found the tab - record its location
                let surface_idx = SurfaceIndex(surface_index);
                let node_index = NodeIndex(node_idx);
                let tab_index = TabIndex(tab_idx);
                found_tab = Some((surface_idx, node_index, tab_index));
                break;
              }
            }
            if found_tab.is_some() {
              break;
            }
          }
        }
        if found_tab.is_some() {
          break;
        }
      }
    }

    // Second pass: activate the found tab
    if let Some((surface_idx, node_index, tab_index)) = found_tab {
      tabs.set_active_tab((surface_idx, node_index, tab_index));
      tabs.set_focused_node_and_surface((surface_idx, node_index));
    }

    // Tab not found - this is not an error, just a no-op
    Ok(Some(input.clone()))
  }
}

#[derive(shards::shard)]
#[shard_info("UI.TabClose", "Close a tab in the dock area")]
pub struct TabCloseShard {
  #[shard_param("TabID", "The ID or title of the tab to close", STRING_TYPES)]
  tab_id: ParamVar,
  #[shard_param(
    "DockArea",
    "The dock area to modify. Defaults to $dock-area context variable.",
    DOCK_OUTPUT_VAR_TYPES
  )]
  dock_area: ParamVar,
  #[shard_required]
  requiring: ExposedTypes,
}

impl Default for TabCloseShard {
  fn default() -> Self {
    let mut dock_area = ParamVar::default();
    dock_area.set_name(DOCK_AREA_VAR_NAME);
    Self {
      tab_id: ParamVar::default(),
      dock_area,
      requiring: Vec::new(),
    }
  }
}

#[shards::shard_impl]
impl Shard for TabCloseShard {
  fn input_types(&mut self) -> &Types {
    &ANY_TYPES
  }

  fn output_types(&mut self) -> &Types {
    &ANY_TYPES
  }

  fn compose(&mut self, data: &InstanceData) -> Result<Type, &str> {
    self.compose_helper(data)?;
    Ok(data.inputType)
  }

  fn warmup(&mut self, ctx: &Context) -> Result<(), &str> {
    self.warmup_helper(ctx)?;
    Ok(())
  }

  fn cleanup(&mut self, ctx: Option<&Context>) -> Result<(), &str> {
    self.cleanup_helper(ctx)?;
    Ok(())
  }

  fn activate(&mut self, _context: &Context, input: &Var) -> Result<Option<Var>, &str> {
    let dock_state_var = self.dock_area.get();
    let dock_state = unsafe {
      Var::from_ref_counted_object::<DockState>(&dock_state_var, &DOCK_OUTPUT_TYPE)
        .map_err(|_| "Invalid dock state object")?
    };

    let tab_id: &str = self
      .tab_id
      .get()
      .try_into()
      .map_err(|_| "TabID must be a string")?;

    let tabs = unsafe { (*dock_state).tabs.as_ref().get().as_mut().unwrap() };

    // First pass: find the tab location without borrowing mutably
    let mut found_tab: Option<(SurfaceIndex, NodeIndex, TabIndex)> = None;
    for (surface_index, surface) in tabs.iter_surfaces().enumerate() {
      if let Some(tree) = surface.node_tree() {
        // Iterate through all nodes to find tabs
        for (node_idx, node) in tree.iter().enumerate() {
          if let Some(node_tabs) = node.tabs() {
            // Look for matching tab by key
            for (tab_idx, tab_ref) in node_tabs.iter().enumerate() {
              if tab_ref.id == tab_id {
                // Found the tab - record its location
                let surface_idx = SurfaceIndex(surface_index);
                let node_index = NodeIndex(node_idx);
                let tab_index = TabIndex(tab_idx);
                found_tab = Some((surface_idx, node_index, tab_index));
                break;
              }
            }
            if found_tab.is_some() {
              break;
            }
          }
        }
        if found_tab.is_some() {
          break;
        }
      }
    }

    // Second pass: remove the found tab
    if let Some((surface_idx, node_index, tab_index)) = found_tab {
      tabs.remove_tab((surface_idx, node_index, tab_index));
    }

    // Tab not found - this is not an error, just a no-op
    Ok(Some(input.clone()))
  }
}

pub fn register_shards() {
  register_shard::<DockArea>();
  register_shard::<SaveShard>();
  register_shard::<RestoreShard>();
  register_shard::<TabIsOpenShard>();
  register_shard::<TabOpenShard>();
  register_shard::<TabCloseShard>();
}

struct MyTabViewer<'a> {
  context: &'a Context,
  parents: &'a mut ParamVar,
  tabs: &'a HashMap<String, TabData>,
}

impl<'a> MyTabViewer<'a> {
  pub fn new(
    context: &'a Context,
    parents: &'a mut ParamVar,
    tabs: &'a HashMap<String, TabData>,
  ) -> MyTabViewer<'a> {
    Self {
      context,
      parents,
      tabs,
    }
  }
}

impl<'a> egui_dock::TabViewer for MyTabViewer<'a> {
  type Tab = TabRef;

  fn ui(&mut self, ui: &mut egui::Ui, tab: &mut Self::Tab) {
    unsafe {
      if let Some(tab_data) = self.tabs.get(&tab.id) {
        let shards = &tab_data.contents;
        with_object_stack_var(self.parents, ui, &EGUI_UI_TYPE, || {
          let input = Var::default();
          let mut output = Var::default();
          let _wire_state: WireState = shards
            .activate(self.context, &input, &mut output)
            .into();

          Ok(())
        })
        .unwrap();
      }
    }
  }

  fn title(&mut self, tab: &mut Self::Tab) -> egui::WidgetText {
    if let Some(tab_data) = &self.tabs.get(&tab.id) {
      if let Ok(title) = tab_data.title.get().try_into() {
        let str: &str = title;
        return str.into();
      }
    }
    return "Untitled".into();
  }
}
