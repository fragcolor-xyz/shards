use ratatui::layout::Rect;
use shards::core::{register_enum, register_object_type, register_shard};
use shards::shard::Shard;
use shards::types::{
  common_type, ClonedVar, SeqVar, ShardsVar, WireState, ANY_TYPES, NONE_TYPES,
  SHARDS_OR_NONE_TYPES, STRING_TYPES,
};
use shards::types::{Context, ExposedTypes, InstanceData, ParamVar, Type, Types, Var};

use crossterm::event::{self, Event};
use ratatui::widgets::{Block, Paragraph};
use ratatui::{DefaultTerminal, Frame};

use lazy_static::lazy_static;
use shards::fourCharacterCode;
use shards::ref_counted_object_type_impl;
use shards::types::FRAG_CC;

#[derive(Clone)]
enum TUIWidget {
  // `Block<'static>` will work as long as all text content is either string literals or owned strings. No lifetime propagation needed in your `TUIElement`.
  Block(Block<'static>),
  Paragraph(Paragraph<'static>),
}

mod tui_element {
  use super::*;

  #[derive(Clone)]
  pub struct TUIElement {
    pub widget: TUIWidget,
    pub area: Rect,
  }
  ref_counted_object_type_impl!(TUIElement);

  lazy_static! {
    pub static ref TUI_ELEMENT_TYPE: Type = Type::object(FRAG_CC, fourCharacterCode(*b"tuiE"));
    pub static ref TUI_ELEMENT_TYPE_VEC: Vec<Type> = vec![*TUI_ELEMENT_TYPE];
    pub static ref TUI_ELEMENT_VAR_TYPE: Type = Type::context_variable(&TUI_ELEMENT_TYPE_VEC);
    pub static ref TUI_ELEMENTS_TYPE: Type = Type::seq(&TUI_ELEMENT_TYPE_VEC);
    pub static ref TUI_ELEMENTS_TYPE_VEC: Vec<Type> = vec![*TUI_ELEMENTS_TYPE];
  }
}

mod tui_elements {
  use super::*;

  #[derive(Clone)]
  pub struct TUIElements {
    pub elements: Vec<tui_element::TUIElement>,
  }
  ref_counted_object_type_impl!(TUIElements);

  lazy_static! {
    pub static ref TUI_ELEMENT_COLLECTION_TYPE: Type =
      Type::object(FRAG_CC, fourCharacterCode(*b"tuiS"));
    pub static ref TUI_ELEMENT_COLLECTION_TYPE_VEC: Vec<Type> = vec![*TUI_ELEMENT_COLLECTION_TYPE];
    pub static ref TUI_ELEMENT_COLLECTION_VAR_TYPE: Type =
      Type::context_variable(&TUI_ELEMENT_COLLECTION_TYPE_VEC);
  }
}

// #[derive(shards::shard)]
// #[shard_info("TUI.Block", "Creates a block widget.")]
// struct TUIBlockShard {
//   #[shard_required]
//   required: ExposedTypes,

//   #[shard_param("TopTitle", "The top title of the block", [common_type::string, common_type::string_var])]
//   top_title: ParamVar,

//   #[shard_param("BottomTitle", "The bottom title of the block", [common_type::string, common_type::string_var])]
//   bottom_title: ParamVar,

//   #[shard_param("Width", "The width of the block, 0 for auto", [common_type::int, common_type::int_var])]
//   width: ParamVar,

//   #[shard_param("Height", "The height of the block, 0 for auto", [common_type::int, common_type::int_var])]
//   height: ParamVar,

//   #[shard_param("Contents", "The contents of the block", SHARDS_OR_NONE_TYPES)]
//   contents: ShardsVar,

//   #[shard_warmup]
//   collection: ParamVar,

//   output: ClonedVar,
// }

// impl Default for TUIBlockShard {
//   fn default() -> Self {
//     Self {
//       required: ExposedTypes::new(),
//       top_title: ParamVar::new(Var::ephemeral_string("")),
//       bottom_title: ParamVar::new(Var::ephemeral_string("")),
//       width: ParamVar::new(0.into()),
//       height: ParamVar::new(0.into()),
//       output: ClonedVar::default(),
//       contents: ShardsVar::default(),
//       collection: ParamVar::new_named("_TUI.Elements"),
//     }
//   }
// }

// #[shards::shard_impl]
// impl Shard for TUIBlockShard {
//   fn input_types(&mut self) -> &Types {
//     &ANY_TYPES
//   }

//   fn output_types(&mut self) -> &Types {
//     &tui_element::TUI_ELEMENT_TYPE_VEC
//   }

//   fn warmup(&mut self, ctx: &Context) -> Result<(), &str> {
//     self.warmup_helper(ctx)?;

//     let collection = tui_elements::TUIElements { elements: vec![] };
//     self.collection.set_cloning(&Var::new_ref_counted(
//       collection,
//       &tui_elements::TUI_ELEMENT_COLLECTION_TYPE,
//     ));

//     Ok(())
//   }

//   fn cleanup(&mut self, ctx: Option<&Context>) -> Result<(), &str> {
//     self.cleanup_helper(ctx)?;

//     self.output = ClonedVar::default();

//     self.collection.set_cloning(&Var::default());

//     Ok(())
//   }

//   fn compose(&mut self, data: &InstanceData) -> Result<Type, &str> {
//     self.compose_helper(data)?;

//     self.contents.compose(data)?;

//     Ok(self.output_types()[0])
//   }

//   fn activate(&mut self, context: &Context, input: &Var) -> Result<Option<Var>, &str> {
//     let top_title = self.top_title.get();
//     let top_title: &str = top_title.try_into()?;

//     let bottom_title = self.bottom_title.get();
//     let bottom_title: &str = bottom_title.try_into()?;

//     let width = self.width.get();
//     let width: u16 = width.try_into()?;

//     let height = self.height.get();
//     let height: u16 = height.try_into()?;

//     let mut output = Var::default();
//     let state = self.contents.activate(context, input, &mut output);
//     if state != WireState::Continue {
//       return Ok(None);
//     }

//     let block = Block::default()
//       .title_top(top_title)
//       .title_bottom(bottom_title);

//     let widget = TUIWidget::Block(block);
//     let element = tui_element::TUIElement {
//       widget,
//       area: Rect::new(0, 0, width, height),
//     };
//     self.output = Var::new_ref_counted(element, &tui_element::TUI_ELEMENT_TYPE).into();
//     Ok(Some(self.output.0))
//   }
// }

#[derive(shards::shard)]
#[shard_info("TUI.Paragraph", "Creates a paragraph widget.")]
struct TUIParagraphShard {
  #[shard_required]
  required: ExposedTypes,

  #[shard_param("TopTitle", "The top title of the block", [common_type::string, common_type::string_var])]
  top_title: ParamVar,

  #[shard_param("BottomTitle", "The bottom title of the block", [common_type::string, common_type::string_var])]
  bottom_title: ParamVar,

  #[shard_param("Width", "The width of the block, 0 for auto", [common_type::int, common_type::int_var])]
  width: ParamVar,

  #[shard_param("Height", "The height of the block, 0 for auto", [common_type::int, common_type::int_var])]
  height: ParamVar,

  output: ClonedVar,
}

impl Default for TUIParagraphShard {
  fn default() -> Self {
    Self {
      required: ExposedTypes::new(),
      top_title: ParamVar::new(Var::ephemeral_string("")),
      bottom_title: ParamVar::new(Var::ephemeral_string("")),
      width: ParamVar::new(0.into()),
      height: ParamVar::new(0.into()),
      output: ClonedVar::default(),
    }
  }
}

#[shards::shard_impl]
impl Shard for TUIParagraphShard {
  fn input_types(&mut self) -> &Types {
    &STRING_TYPES
  }

  fn output_types(&mut self) -> &Types {
    &tui_element::TUI_ELEMENT_TYPE_VEC
  }

  fn warmup(&mut self, ctx: &Context) -> Result<(), &str> {
    self.warmup_helper(ctx)?;

    Ok(())
  }

  fn cleanup(&mut self, ctx: Option<&Context>) -> Result<(), &str> {
    self.cleanup_helper(ctx)?;

    self.output = ClonedVar::default();

    Ok(())
  }

  fn compose(&mut self, data: &InstanceData) -> Result<Type, &str> {
    self.compose_helper(data)?;

    Ok(self.output_types()[0])
  }

  fn activate(&mut self, _context: &Context, input: &Var) -> Result<Option<Var>, &str> {
    let input: &str = input.try_into()?;

    let top_title = self.top_title.get();
    let top_title: &str = top_title.try_into()?;

    let bottom_title = self.bottom_title.get();
    let bottom_title: &str = bottom_title.try_into()?;

    let width = self.width.get();
    let width: u16 = width.try_into()?;

    let height = self.height.get();
    let height: u16 = height.try_into()?;

    let block = Block::default()
      .title_top(top_title)
      .title_bottom(bottom_title);

    let widget = TUIWidget::Paragraph(Paragraph::new(input).block(block));
    let element = tui_element::TUIElement {
      widget,
      area: Rect::new(0, 0, width, height),
    };
    self.output = Var::new_ref_counted(element, &tui_element::TUI_ELEMENT_TYPE).into();
    Ok(Some(self.output.0))
  }
}

#[derive(shards::shard)]
#[shard_info("TUI.Draw", "Draws a hierarchy or terminal UI elements.")]
struct TUIDrawShard {
  #[shard_required]
  required: ExposedTypes,

  terminal: Option<DefaultTerminal>,
}

impl Default for TUIDrawShard {
  fn default() -> Self {
    Self {
      required: ExposedTypes::new(),
      terminal: None,
    }
  }
}

#[shards::shard_impl]
impl Shard for TUIDrawShard {
  fn input_types(&mut self) -> &Types {
    &tui_element::TUI_ELEMENTS_TYPE_VEC
  }

  fn output_types(&mut self) -> &Types {
    &tui_element::TUI_ELEMENTS_TYPE_VEC
  }

  fn warmup(&mut self, ctx: &Context) -> Result<(), &str> {
    self.warmup_helper(ctx)?;

    self.terminal = Some(ratatui::init());

    Ok(())
  }

  fn cleanup(&mut self, ctx: Option<&Context>) -> Result<(), &str> {
    self.cleanup_helper(ctx)?;

    ratatui::restore();

    self.terminal = None;

    Ok(())
  }

  fn compose(&mut self, data: &InstanceData) -> Result<Type, &str> {
    self.compose_helper(data)?;
    Ok(self.output_types()[0])
  }

  fn activate(&mut self, _context: &Context, input: &Var) -> Result<Option<Var>, &str> {
    let widgets: SeqVar = input.try_into()?;
    self
      .terminal
      .as_mut()
      .unwrap()
      .draw(|frame| {
        for widget in widgets.iter() {
          let widget = unsafe {
            &*Var::from_ref_counted_object::<tui_element::TUIElement>(
              &widget,
              &tui_element::TUI_ELEMENT_TYPE,
            )
            .unwrap()
          };
          match &widget.widget {
            TUIWidget::Block(block) => {
              if widget.area.width > 0 && widget.area.height > 0 {
                frame.render_widget(block, widget.area);
              } else {
                frame.render_widget(block, frame.area());
              }
            }
            TUIWidget::Paragraph(paragraph) => {
              if widget.area.width > 0 && widget.area.height > 0 {
                frame.render_widget(paragraph, widget.area);
              } else {
                frame.render_widget(paragraph, frame.area());
              }
            }
          }
        }
      })
      .map_err(|_| "Failed to draw terminal")?;

    Ok(None)
  }
}

#[no_mangle]
pub extern "C" fn shardsRegister_tui_rust(core: *mut shards::shardsc::SHCore) {
  unsafe {
    shards::core::Core = core;
  }

  register_shard::<TUIDrawShard>();
  register_shard::<TUIParagraphShard>();

  register_object_type::<tui_element::TUIElement>(FRAG_CC, fourCharacterCode(*b"tuiE"));
}
