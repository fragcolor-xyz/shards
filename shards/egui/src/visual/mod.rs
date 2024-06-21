use egui::*;
use nanoid::nanoid;
use std::any::Any;

use crate::{
  util::{get_current_parent_opt, require_parents},
  widgets::drag_value::CustomDragValue,
  PARENTS_UI_NAME,
};
use shards::{
  core::register_shard,
  shard::Shard,
  types::{
    Context as ShardsContext, ExposedTypes, InstanceData, ParamVar, Type, Types, Var, NONE_TYPES,
  },
};

use pest::Parser;

use shards_lang::{ast::*, ast_visitor::*};

mod directory;

fn draw_arrow_head(ui: &mut egui::Ui, from: Rect, to: Rect) {
  let painter = ui.painter();

  // Calculate arrow position
  let arrow_x = (from.right() + to.left()) / 2.0;
  let arrow_y = from.center().y; // Align with the vertical center of the frames
  let arrow_pos = pos2(arrow_x, arrow_y);

  // Arrow dimensions
  let arrow_width = 8.0;
  let arrow_height = 12.0;

  // Calculate arrow points
  let tip = arrow_pos + Vec2::new(arrow_height / 2.0, 0.0);
  let left = arrow_pos + Vec2::new(-arrow_height / 2.0, -arrow_width / 2.0);
  let right = arrow_pos + Vec2::new(-arrow_height / 2.0, arrow_width / 2.0);

  // Draw arrow
  painter.add(egui::Shape::convex_polygon(
    vec![tip, left, right],
    egui::Color32::WHITE,
    Stroke::new(1.0, egui::Color32::WHITE),
  ));
}

#[derive(Debug, Clone, PartialEq)]
struct VisualState {
  selected: bool,
  id: Id,
}

impl_custom_any!(VisualState);

pub struct VisualAst<'a> {
  ui: &'a mut Ui,
}

impl<'a> VisualAst<'a> {
  pub fn new(ui: &'a mut Ui) -> Self {
    VisualAst { ui }
  }

  pub fn render(&mut self, ast: &mut Sequence) -> Option<Response> {
    ast.accept_mut(self)
  }

  fn mutate_shard(&mut self, x: &mut Function) -> Option<Response> {
    let directory = directory::get_global_map();
    let shards = directory.0.get_fast_static("shards");
    let shards = shards.as_table().unwrap();
    Some(
      egui::CollapsingHeader::new(x.name.name.as_str())
        .default_open(false)
        .show(self.ui, |ui| {
          if let Some(params) = &mut x.params {
            let mut mutator = VisualAst::new(ui);
            for param in params {
              param.accept_mut(&mut mutator);
            }
          }
        })
        .header_response,
    )
  }
}

impl<'a> AstMutator<Option<Response>> for VisualAst<'a> {
  fn visit_program(&mut self, program: &mut Program) -> Option<Response> {
    program.metadata.accept_mut(self);
    program.sequence.accept_mut(self)
  }

  fn visit_sequence(&mut self, sequence: &mut Sequence) -> Option<Response> {
    let mut final_response: Option<Response> = None;
    for statement in &mut sequence.statements {
      let response = statement.accept_mut(self);
      if let Some(previous_response) = final_response.take() {
        if let Some(response) = response {
          final_response = Some(previous_response.union(response))
        } else {
          final_response = Some(previous_response)
        }
      }
    }
    final_response
  }

  fn visit_statement(&mut self, statement: &mut Statement) -> Option<Response> {
    self
      .ui
      .horizontal(|ui| {
        let mut mutator = VisualAst::new(ui);
        match statement {
          Statement::Assignment(assignment) => assignment.accept_mut(&mut mutator),
          Statement::Pipeline(pipeline) => pipeline.accept_mut(&mut mutator),
        }
      })
      .inner
  }

  fn visit_assignment(&mut self, assignment: &mut Assignment) -> Option<Response> {
    let mut combined_response = None;
    let (r_a, r_b) = match assignment {
      Assignment::AssignRef(pipeline, identifier) => {
        (pipeline.accept_mut(self), identifier.accept_mut(self))
      }
      Assignment::AssignSet(pipeline, identifier) => {
        (pipeline.accept_mut(self), identifier.accept_mut(self))
      }
      Assignment::AssignUpd(pipeline, identifier) => {
        (pipeline.accept_mut(self), identifier.accept_mut(self))
      }
      Assignment::AssignPush(pipeline, identifier) => {
        (pipeline.accept_mut(self), identifier.accept_mut(self))
      }
    };
    if let Some(a) = r_a {
      if let Some(b) = r_b {
        combined_response = Some(a.union(b));
      } else {
        combined_response = Some(a);
      }
    }
    combined_response
  }

  fn visit_pipeline(&mut self, pipeline: &mut Pipeline) -> Option<Response> {
    let mut final_response: Option<Response> = None;
    let mut i = 0;
    while i < pipeline.blocks.len() {
      let response = match self.visit_block(&mut pipeline.blocks[i]) {
        (BlockAction::Remove, r) => {
          pipeline.blocks.remove(i);
          r
        }
        (BlockAction::Keep, r) => {
          i += 1;
          r
        }
        (BlockAction::Duplicate, r) => {
          let mut block = pipeline.blocks[i].clone();
          block.get_custom_state::<VisualState>().map(|x| {
            x.selected = false;
            x.id = Id::new(nanoid!(16));
          });
          pipeline.blocks.insert(i, block);
          i += 2;
          r
        }
        (BlockAction::Swap(block), r) => {
          pipeline.blocks[i] = block;
          i += 1;
          r
        }
      };

      // if let Some(response) = &response {
      //   draw_arrow_head(
      //     self.ui,
      //     response.rect,
      //     response.rect.translate(Vec2::new(10.0, 0.0)),
      //   );
      // }

      if let Some(previous_response) = final_response.take() {
        if let Some(response) = response {
          final_response = Some(previous_response.union(response))
        } else {
          final_response = Some(previous_response)
        }
      }
    }

    final_response
  }

  fn visit_block(&mut self, block: &mut Block) -> (BlockAction, Option<Response>) {
    let (selected, id) = {
      let state = block.get_or_insert_custom_state(|| VisualState {
        selected: false,
        id: Id::new(nanoid!(16)),
      });
      (state.selected, state.id)
    };

    let mut action = BlockAction::Keep;

    let response = self
      .ui
      .push_id(id, |ui| {
        egui::Frame::window(ui.style())
          .show(ui, |ui| {
            ui.vertical(|ui| {
              if selected {
                ui.horizontal(|ui| {
                  if ui.button("S").clicked() {
                    // switch
                  }
                  if ui.button("D").clicked() {
                    action = BlockAction::Duplicate;
                  }
                  if ui.button("X").clicked() {
                    action = BlockAction::Remove;
                  }
                });
              }
              match &mut block.content {
                BlockContent::Empty => Some(ui.separator()),
                BlockContent::Shard(x) => {
                  let mut mutator = VisualAst::new(ui);
                  mutator.mutate_shard(x)
                }
                BlockContent::Expr(x) | BlockContent::EvalExpr(x) | BlockContent::Shards(x) => {
                  ui.group(|ui| {
                    let mut mutator = VisualAst::new(ui);
                    x.accept_mut(&mut mutator)
                  })
                  .inner
                }
                BlockContent::Const(x) => {
                  let mut mutator = VisualAst::new(ui);
                  x.accept_mut(&mut mutator)
                }
                BlockContent::TakeTable(_, _) => todo!(),
                BlockContent::TakeSeq(_, _) => todo!(),
                BlockContent::Func(x) => {
                  let mut mutator = VisualAst::new(ui);
                  mutator.mutate_shard(x)
                }
                BlockContent::Program(x) => {
                  ui.group(|ui| {
                    let mut mutator = VisualAst::new(ui);
                    x.accept_mut(&mut mutator)
                  })
                  .inner
                }
              }
            })
            .inner
          })
          .inner
      })
      .inner;
    (action, response)
  }

  fn visit_function(&mut self, function: &mut Function) -> Option<Response> {
    self.mutate_shard(function)
  }

  fn visit_param(&mut self, param: &mut Param) -> Option<Response> {
    if let Some(name) = &param.name {
      self.ui.label(name.as_str());
    }
    param.value.accept_mut(self)
  }

  fn visit_identifier(&mut self, identifier: &mut Identifier) -> Option<Response> {
    Some(self.ui.label(identifier.name.as_str()))
  }

  fn visit_value(&mut self, value: &mut Value) -> Option<Response> {
    match value {
      Value::None => Some(self.ui.label("None")),
      Value::Identifier(x) => Some(self.ui.label(x.name.as_str())),
      Value::Boolean(x) => Some(self.ui.checkbox(x, if *x { "true" } else { "false" })),
      Value::Enum(_, _) => todo!(),
      Value::Number(x) => match x {
        Number::Integer(x) => Some(self.ui.add(CustomDragValue::new(x))),
        Number::Float(x) => Some(self.ui.add(CustomDragValue::new(x))),
        Number::Hexadecimal(_x) => todo!(),
      },
      Value::String(x) => {
        // if long we should use a multiline text editor
        // if short we should use a single line text editor
        let x = x.to_mut();
        Some(if x.len() > 16 {
          self.ui.text_edit_multiline(x)
        } else {
          let text = x.as_str();
          let text_width = 10.0 * text.chars().count() as f32;
          let width = text_width + 20.0; // Add some padding
          TextEdit::singleline(x).desired_width(width).ui(self.ui)
        })
      }
      Value::Bytes(_) => todo!(),
      Value::Int2(_) => todo!(),
      Value::Int3(_) => todo!(),
      Value::Int4(_) => todo!(),
      Value::Int8(_) => todo!(),
      Value::Int16(_) => todo!(),
      Value::Float2(_) => todo!(),
      Value::Float3(_) => todo!(),
      Value::Float4(_) => todo!(),
      Value::Seq(x) => todo!(),
      Value::Table(_) => todo!(),
      Value::Shard(x) => {
        let mut mutator = VisualAst::new(self.ui);
        x.accept_mut(&mut mutator)
      }
      Value::Shards(x) => {
        let mut mutator = VisualAst::new(self.ui);
        x.accept_mut(&mut mutator)
      }
      Value::EvalExpr(_) => todo!(),
      Value::Expr(_) => todo!(),
      Value::TakeTable(_, _) => todo!(),
      Value::TakeSeq(_, _) => todo!(),
      Value::Func(_) => todo!(),
    }
  }

  fn visit_metadata(&mut self, _metadata: &mut Metadata) -> Option<Response> {
    None
  }
}

#[derive(shards::shard)]
#[shard_info("UI.Shards", "A Shards program editor")]
pub struct UIShardsShard {
  #[shard_required]
  required: ExposedTypes,

  #[shard_warmup]
  parents: ParamVar,

  ast: Sequence,
}

impl Default for UIShardsShard {
  fn default() -> Self {
    let code = include_str!("simple.shs");
    let successful_parse = ShardsParser::parse(Rule::Program, code).unwrap();
    let mut env = shards_lang::read::ReadEnv::new("", ".", ".");
    let seq =
      shards_lang::read::process_program(successful_parse.into_iter().next().unwrap(), &mut env)
        .unwrap();
    let seq = seq.sequence;
    Self {
      required: ExposedTypes::new(),
      parents: ParamVar::new_named(PARENTS_UI_NAME),
      ast: seq,
    }
  }
}

#[shards::shard_impl]
impl Shard for UIShardsShard {
  fn input_types(&mut self) -> &Types {
    &NONE_TYPES
  }

  fn output_types(&mut self) -> &Types {
    &NONE_TYPES
  }

  fn warmup(&mut self, ctx: &ShardsContext) -> Result<(), &str> {
    self.warmup_helper(ctx)?;

    Ok(())
  }

  fn cleanup(&mut self, ctx: Option<&ShardsContext>) -> Result<(), &str> {
    self.cleanup_helper(ctx)?;

    Ok(())
  }

  fn compose(&mut self, data: &InstanceData) -> Result<Type, &str> {
    self.compose_helper(data)?;

    require_parents(&mut self.required);

    Ok(self.output_types()[0])
  }

  fn activate(&mut self, _context: &ShardsContext, _input: &Var) -> Result<Var, &str> {
    let ui = get_current_parent_opt(self.parents.get())?.ok_or("No parent UI")?;
    let mut mutator = VisualAst::new(ui);
    self.ast.accept_mut(&mut mutator);
    Ok(Var::default())
  }
}

pub fn register_shards() {
  register_shard::<UIShardsShard>();
}
