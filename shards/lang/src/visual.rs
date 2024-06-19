use std::cell::{RefCell, RefMut};

use egui::*;
use shards::{
  shard::Shard,
  types::{Context as ShardsContext, ExposedTypes, InstanceData, Type, Types, Var, NONE_TYPES},
};
use shards_egui::widgets::drag_value::CustomDragValue;

use crate::{ast::*, ast_visitor::AstMutator};

pub struct VisualAst<'a> {
  ui: &'a mut Ui,
  response: Option<Response>,
}

impl<'a> VisualAst<'a> {
  pub fn new(ui: &'a mut Ui) -> Self {
    VisualAst { ui, response: None }
  }

  pub fn render(&mut self, ast: &mut Sequence) -> Option<Response> {
    ast.accept_mut(self);
    self.response.clone()
  }

  fn mutate_shard(&mut self, x: &mut Function) -> Response {
    self
      .ui
      .collapsing(x.name.name.as_str(), |ui: &mut Ui| {
        if let Some(params) = &mut x.params {
          let mut mutator = VisualAst::new(ui);
          for param in params {
            param.accept_mut(&mut mutator);
          }
        }
      })
      .header_response
  }
}

impl<'a> AstMutator for VisualAst<'a> {
  fn visit_program(&mut self, program: &mut Program) {
    program.metadata.accept_mut(self);
    program.sequence.accept_mut(self);
  }

  fn visit_sequence(&mut self, sequence: &mut Sequence) {
    for statement in &mut sequence.statements {
      statement.accept_mut(self);
    }
  }

  fn visit_statement(&mut self, statement: &mut Statement) {
    match statement {
      Statement::Assignment(assignment) => assignment.accept_mut(self),
      Statement::Pipeline(pipeline) => pipeline.accept_mut(self),
    }
  }

  fn visit_assignment(&mut self, assignment: &mut Assignment) {
    match assignment {
      Assignment::AssignRef(pipeline, identifier) => {
        pipeline.accept_mut(self);
        identifier.accept_mut(self);
      }
      Assignment::AssignSet(pipeline, identifier) => {
        pipeline.accept_mut(self);
        identifier.accept_mut(self);
      }
      Assignment::AssignUpd(pipeline, identifier) => {
        pipeline.accept_mut(self);
        identifier.accept_mut(self);
      }
      Assignment::AssignPush(pipeline, identifier) => {
        pipeline.accept_mut(self);
        identifier.accept_mut(self);
      }
    }
  }

  fn visit_pipeline(&mut self, pipeline: &mut Pipeline) {
    for block in &mut pipeline.blocks {
      block.accept_mut(self);
    }
  }

  fn visit_block(&mut self, block: &mut Block) {
    let _response = match &mut block.content {
      BlockContent::Empty => self.ui.separator(),
      BlockContent::Shard(x) => self.mutate_shard(x),
      BlockContent::Expr(x) | BlockContent::EvalExpr(x) | BlockContent::Shards(x) => {
        self
          .ui
          .group(|ui| {
            let mut mutator = VisualAst::new(ui);
            x.accept_mut(&mut mutator)
          })
          .response
      }
      BlockContent::Const(x) => {
        let mut mutator = VisualAst::new(self.ui);
        x.accept_mut(&mut mutator);
        mutator.response.unwrap()
      }
      BlockContent::TakeTable(_, _) => todo!(),
      BlockContent::TakeSeq(_, _) => todo!(),
      BlockContent::Func(x) => self.mutate_shard(x),
      BlockContent::Program(x) => {
        self
          .ui
          .group(|ui| {
            let mut mutator = VisualAst::new(ui);
            x.accept_mut(&mut mutator)
          })
          .response
      }
    };
  }

  fn visit_function(&mut self, function: &mut Function) {
    self.mutate_shard(function);
  }

  fn visit_param(&mut self, param: &mut Param) {
    if let Some(name) = &param.name {
      self.ui.label(name.as_str());
    }
    param.value.accept_mut(self);
  }

  fn visit_identifier(&mut self, identifier: &mut Identifier) {
    self.ui.label(identifier.name.as_str());
  }

  fn visit_value(&mut self, value: &mut Value) {
    let response = match value {
      Value::None => self.ui.label("None"),
      Value::Identifier(_) => todo!(),
      Value::Boolean(x) => self.ui.checkbox(x, if *x { "true" } else { "false" }),
      Value::Enum(_, _) => todo!(),
      Value::Number(x) => match x {
        Number::Integer(x) => self.ui.add(CustomDragValue::new(x)),
        Number::Float(x) => self.ui.add(CustomDragValue::new(x)),
        Number::Hexadecimal(_x) => todo!(),
      },
      Value::String(x) => {
        // if long we should use a multiline text editor
        // if short we should use a single line text editor
        let x = x.to_mut();
        if x.len() > 16 {
          self.ui.text_edit_multiline(x)
        } else {
          self.ui.text_edit_singleline(x)
        }
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
      Value::Seq(_) => todo!(),
      Value::Table(_) => todo!(),
      Value::Shard(_) => todo!(),
      Value::Shards(_) => todo!(),
      Value::EvalExpr(_) => todo!(),
      Value::Expr(_) => todo!(),
      Value::TakeTable(_, _) => todo!(),
      Value::TakeSeq(_, _) => todo!(),
      Value::Func(_) => todo!(),
    };
    self.response = Some(response);
  }

  fn visit_metadata(&mut self, metadata: &mut Metadata) {}
}

#[derive(shards::shard)]
#[shard_info("UI.Shards", "A Shards program editor")]
struct UIShardsShard {
  #[shard_required]
  required: ExposedTypes,
}

impl Default for UIShardsShard {
  fn default() -> Self {
    Self {
      required: ExposedTypes::new(),
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
    Ok(self.output_types()[0])
  }

  fn activate(&mut self, _context: &ShardsContext, _input: &Var) -> Result<Var, &str> {
    Ok(Var::default())
  }
}
