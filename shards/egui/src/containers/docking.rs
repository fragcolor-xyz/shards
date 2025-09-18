use super::Tab;
use super::EXPERIMENTAL_TRUE;
use crate::util;

use crate::util::with_object_stack_var;
use crate::CONTEXTS_NAME;
use crate::EGUI_UI_TYPE;
use crate::HELP_OUTPUT_EQUAL_INPUT;

use crate::PARENTS_UI_NAME;
use shards::core::register_legacy_shard;
use shards::core::register_shard;
use shards::fourCharacterCode;
use shards::shard::LegacyShard;
use shards::shard::Shard;
use shards::shardsc;
use shards::types::ClonedVar;
use shards::types::Context;

use shards::types::ExposedTypes;
use shards::types::InstanceData;
use shards::types::OptionalString;
use shards::types::ParamVar;
use shards::types::Parameters;
use shards::types::Seq;
use shards::types::ShardRef;
use shards::types::ShardsVar;
use shards::types::Table;
use shards::types::Type;
use shards::types::Types;
use shards::types::Var;
use shards::types::WireState;
use shards::types::ANY_TYPES;

use shards::types::BYTES_TYPES;
use shards::types::FRAG_CC;
use shards::types::INT_TYPES;
use shards::types::SHARDS_OR_NONE_TYPES;
use shards::types::STRING_TYPES;

use std::cell::UnsafeCell;
use std::convert::TryInto;
use std::sync::Arc;

const TAB_NAME: &'static str = "UI.Tab";

#[derive(serde::Deserialize, serde::Serialize)]
struct TabRef {
  title: String,
  type_id: usize,
}

struct TabData {
  title: Option<Var>,
  contents: Option<shards::Shards>,
}

struct DockState {
  pub tabs: Arc<UnsafeCell<egui_dock::DockState<TabRef>>>,
}
ref_counted_object_type_impl!(DockState);
static DOCK_OUTPUT_TYPE: Type = Type::object(FRAG_CC, fourCharacterCode(*b"uidk"));
lazy_static! {
  static ref DOCK_OUTPUT_TYPES: Vec<Type> = vec![DOCK_OUTPUT_TYPE];
}

lazy_static! {
  static ref TAB_PARAMETERS: Parameters = vec![
    (
      cstr!("Title"),
      cstr!("The title of the tab."),
      &STRING_TYPES[..],
    )
      .into(),
    (
      cstr!("Contents"),
      cstr!("The UI contents."),
      &SHARDS_OR_NONE_TYPES[..],
    )
      .into(),
  ];
  static ref DOCKAREA_PARAMETERS: Parameters = vec![
    (
      cstr!("Contents"),
      cstr!("The UI contents containing tabs."),
      &SHARDS_OR_NONE_TYPES[..],
    )
      .into(),
    (
      cstr!("DefaultTab"),
      cstr!("The index of the tab to open by default. 0 being the right most tab."),
      &INT_TYPES[..],
    )
      .into(),
  ];
}

impl Default for Tab {
  fn default() -> Self {
    let mut parents = ParamVar::default();
    parents.set_name(PARENTS_UI_NAME);
    Self {
      parents,
      requiring: Vec::new(),
      title: ParamVar::default(),
      contents: ShardsVar::default(),
      exposing: Vec::new(),
    }
  }
}

impl LegacyShard for Tab {
  fn properties(&mut self) -> Option<&Table> {
    Some(&EXPERIMENTAL_TRUE)
  }

  fn registerName() -> &'static str
  where
    Self: Sized,
  {
    cstr!("UI.Tab")
  }

  fn hash() -> u32
  where
    Self: Sized,
  {
    compile_time_crc32::crc32!("UI.Tab-rust-0x20250822")
  }

  fn name(&mut self) -> &str {
    TAB_NAME
  }

  fn help(&mut self) -> OptionalString {
    OptionalString(shccstr!("Represents a tab inside a DockArea."))
  }

  fn inputTypes(&mut self) -> &Types {
    &ANY_TYPES
  }

  fn inputHelp(&mut self) -> OptionalString {
    OptionalString(shccstr!(
      "The value that will be passed to the Contents shards of the tab."
    ))
  }

  fn outputTypes(&mut self) -> &Types {
    &ANY_TYPES
  }

  fn outputHelp(&mut self) -> OptionalString {
    OptionalString(shccstr!("The output of this shard will be its input."))
  }

  fn parameters(&mut self) -> Option<&Parameters> {
    Some(&TAB_PARAMETERS)
  }

  fn setParam(&mut self, index: i32, value: &Var) -> Result<(), &str> {
    match index {
      0 => self.title.set_param(value),
      1 => self.contents.set_param(value),
      _ => Err("Invalid parameter index"),
    }
  }

  fn getParam(&mut self, index: i32) -> Var {
    match index {
      0 => self.title.get_param(),
      1 => self.contents.get_param(),
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

    if util::expose_contents_variables(&mut self.exposing, &self.contents) {
      Some(&self.exposing)
    } else {
      None
    }
  }

  fn hasCompose() -> bool {
    true
  }

  fn compose(&mut self, data: &InstanceData) -> Result<Type, &str> {
    if !self.contents.is_empty() {
      self.contents.compose(&data)?;
    }

    // Always passthrough the input
    Ok(data.inputType)
  }

  fn warmup(&mut self, ctx: &Context) -> Result<(), &str> {
    self.parents.warmup(ctx);
    self.title.warmup(ctx);
    if !self.contents.is_empty() {
      self.contents.warmup(ctx)?;
    }

    Ok(())
  }

  fn cleanup(&mut self, ctx: Option<&Context>) -> Result<(), &str> {
    if !self.contents.is_empty() {
      self.contents.cleanup(ctx);
    }
    self.title.cleanup(ctx);
    self.parents.cleanup(ctx);

    Ok(())
  }

  fn activate(&mut self, context: &Context, input: &Var) -> Result<Option<Var>, &str> {
    if self.contents.is_empty() {
      // no contents, same as inactive
      return Ok(None);
    }

    if let Some(ui) = util::get_current_parent_opt(self.parents.get())? {
      util::activate_ui_contents(context, input, ui, &mut self.parents, &mut self.contents)?;

      // Always passthrough the input
      Ok(None)
    } else {
      Err("No UI parent")
    }
  }
}

struct DockArea {
  instance: ParamVar,
  requiring: ExposedTypes,
  contents: ParamVar,
  parents: ParamVar,
  exposing: ExposedTypes,
  headers: Vec<ParamVar>,
  shards: Vec<ShardsVar>,
  tabs: Arc<UnsafeCell<egui_dock::DockState<TabRef>>>,
  tabs_var: ClonedVar,
  tab_data: Vec<TabData>,
  default_tab: i64,
}

impl Default for DockArea {
  fn default() -> Self {
    let mut ctx = ParamVar::default();
    ctx.set_name(CONTEXTS_NAME);
    let mut parents = ParamVar::default();
    parents.set_name(PARENTS_UI_NAME);
    Self {
      instance: ctx,
      requiring: Vec::new(),
      contents: ParamVar::default(),
      parents,
      exposing: Vec::new(),
      headers: Vec::new(),
      shards: Vec::new(),
      tabs: Arc::new(UnsafeCell::new(egui_dock::DockState::new(Vec::new()))),
      tabs_var: ClonedVar::default(),
      tab_data: Vec::new(),
      default_tab: 0,
    }
  }
}

impl LegacyShard for DockArea {
  fn registerName() -> &'static str
  where
    Self: Sized,
  {
    cstr!("UI.DockArea")
  }

  fn hash() -> u32
  where
    Self: Sized,
  {
    compile_time_crc32::crc32!("UI.DockArea-rust-0x20250822")
  }

  fn name(&mut self) -> &str {
    "UI.DockArea"
  }

  fn help(&mut self) -> OptionalString {
    OptionalString(shccstr!("Serves as a container for the UI.Tab shard."))
  }

  fn inputTypes(&mut self) -> &Types {
    &ANY_TYPES
  }

  fn inputHelp(&mut self) -> OptionalString {
    OptionalString(shccstr!("The input of this shard is ignored."))
  }

  fn outputTypes(&mut self) -> &Types {
    &DOCK_OUTPUT_TYPES
  }

  fn outputHelp(&mut self) -> OptionalString {
    *HELP_OUTPUT_EQUAL_INPUT
  }

  fn parameters(&mut self) -> Option<&Parameters> {
    Some(&DOCKAREA_PARAMETERS)
  }

  fn setParam(&mut self, index: i32, value: &Var) -> Result<(), &str> {
    match index {
      0 => {
        let filter_closure = |s: &Var| {
          if let Ok(r) = ShardRef::try_from(s) {
            if r.name() == TAB_NAME {
              let mut title = ParamVar::default();
              match title.set_param(&r.get_parameter(0)) {
                Ok(_) => {}
                Err(err) => return Some(Err(err)),
              }
              let mut contents = ShardsVar::default();
              match contents.set_param(&s) {
                Ok(_) => Some(Ok((title, contents))),
                Err(err) => Some(Err(err)),
              }
            } else {
              None
            }
          } else {
            None
          }
        };

        match value.valueType {
          shardsc::SHType_None => {}
          shardsc::SHType_ShardRef => {
            if let Some(tab_spec) = filter_closure(&value) {
              let (title, contents) = tab_spec?;
              self.headers.push(title);
              self.shards.push(contents);
            }
          }
          shardsc::SHType_Seq => {
            let seq = Seq::try_from(value)?;
            let tab_specs = seq.iter().rev().filter_map(filter_closure);
            for tab_spec in tab_specs {
              let (title, contents) = tab_spec?;
              self.headers.push(title);
              self.shards.push(contents);
            }
          }
          _ => return Err("Invalid parameter type"),
        }

        self.contents.set_param(value)
      }
      1 => {
        if let Ok(int_value) = value.try_into() {
          self.default_tab = int_value;
        }
        Ok(())
      }
      _ => Err("Invalid parameter index"),
    }
  }

  fn getParam(&mut self, index: i32) -> Var {
    match index {
      0 => self.contents.get_param(),
      1 => Var::from(self.default_tab),
      _ => Var::default(),
    }
  }

  fn requiredVariables(&mut self) -> Option<&ExposedTypes> {
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
    self.requiring.clear();
    util::require_context(&mut self.requiring);
    util::require_parents(&mut self.requiring);

    for s in &mut self.shards {
      s.compose(data)?;
    }

    // Always passthrough the input
    Ok(DOCK_OUTPUT_TYPE)
  }

  fn warmup(&mut self, ctx: &Context) -> Result<(), &str> {
    self.instance.warmup(ctx);
    self.parents.warmup(ctx);
    self.contents.warmup(ctx);

    for s in &self.shards {
      s.warmup(ctx)?;
    }
    for s in self.headers.as_mut_slice() {
      s.warmup(ctx);
    }

    self.tabs = Arc::new(UnsafeCell::new(egui_dock::DockState::new(Vec::new())));
    let tabs = unsafe { self.tabs.as_mut_unchecked() };
    for (h, s) in std::iter::zip(self.headers.iter_mut(), self.shards.iter_mut()) {
      let td = TabData::new(h, s);
      tabs.push_to_first_leaf(td);
    }

    // Focus on specified default tab
    if tabs.surfaces_count() > 0 {
      let tab_index = self.default_tab.clamp(0, tabs.surfaces_count() as i64 - 1);
      tabs.set_active_tab((0.into(), 0.into(), (tab_index as usize).into()));
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
    self.contents.cleanup(ctx);
    self.parents.cleanup(ctx);
    self.instance.cleanup(ctx);
    for s in &mut self.shards {
      s.cleanup(ctx);
    }
    for s in &mut self.headers {
      s.cleanup(ctx);
    }

    Ok(())
  }

  fn activate(&mut self, context: &Context, _input: &Var) -> Result<Option<Var>, &str> {
    let tabs = unsafe { self.tabs.as_mut_unchecked() };
    if tabs.surfaces_count() == 0 {
      return Ok(Some(self.tabs_var.0));
    }

    let gui_ctx = util::get_current_context(&self.instance)?;
    let style = egui_dock::Style::from_egui(gui_ctx.egui_ctx.style().as_ref());

    let mut area = egui_dock::DockArea::new(tabs).style(style);

    let parents_stack_var = self.parents.get().clone();
    let mut viewer = MyTabViewer::new(context, &mut self.parents);
    if let Some(ui) = util::get_current_parent_opt(&parents_stack_var)? {
      area.show_inside(ui, &mut viewer);
    } else {
      area.show(&gui_ctx.egui_ctx, &mut viewer);
    }

    // Always passthrough the input
    Ok(Some(self.tabs_var.0))
  }
}

#[derive(shards::shard)]
#[shard_info("UI.SaveDockState", "Save the state of the DockArea")]
pub struct SaveShard {
  #[shard_param("DockState", "The state of the DockArea", &DOCK_OUTPUT_TYPES)]
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

  fn activate(&mut self, context: &Context, input: &Var) -> Result<Option<Var>, &str> {
    let dock_state = unsafe {
      (*Var::from_ref_counted_object::<DockState>(&input, &DOCK_OUTPUT_TYPE).unwrap())
        .tabs
        .as_mut_unchecked()
    };

    if let Ok(json) = bitcode::serialize(&mut *dock_state) {
    } else {
      return Err("Failed to save dock state");
    }
    let data = dock_state.save();
    Ok(Some(Var::new(data)))
  }
}

#[derive(shards::shard)]
#[shard_info("UI.RestoreDockState", "Restore the state of the DockArea")]
pub struct RestoreShard {}

pub fn register_shards() {
  register_legacy_shard::<DockArea>();
  register_legacy_shard::<Tab>();
  register_shard::<SaveShard>();
  // register_shard::<RestoreShard>();
}

struct MyTabViewer<'a> {
  context: &'a Context,
  parents: &'a mut ParamVar,
  tabs: &'a [TabData],
}

impl<'a> MyTabViewer<'a> {
  pub fn new(
    context: &'a Context,
    parents: &'a mut ParamVar,
    tabs: &'a [TabData],
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
      if let Some(shards) = self.tabs[tab.type_id].contents {
        with_object_stack_var(self.parents, ui, &EGUI_UI_TYPE, || {
          let input = Var::default();
          let mut output = Var::default();
          let _wire_state: WireState = (*shards::core::Core).runShards.unwrap()(
            shards,
            self.context as *const _ as *mut _,
            &input,
            &mut output,
          )
          .into();

          Ok(())
        })
        .unwrap();
      }
    }
  }

  fn title(&mut self, tab: &mut Self::Tab) -> egui::WidgetText {
    if let Some(title) = &self.tabs[tab.type_id].title {
      let str: &str = title.try_into().unwrap();
      str.into()
    } else {
      "Untitled".into()
    }
  }
}
