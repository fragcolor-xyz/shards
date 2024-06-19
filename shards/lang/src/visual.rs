use std::cell::{RefCell, RefMut};

use egui::*;
use shards_egui::{widgets::drag_value::CustomDragValue, MutVarTextBuffer};

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
    todo!()
  }

  fn visit_function(&mut self, function: &mut Function) {
    todo!()
  }

  fn visit_param(&mut self, param: &mut Param) {
    todo!()
  }

  fn visit_identifier(&mut self, identifier: &mut Identifier) {
    todo!()
  }

  fn visit_value(&mut self, value: &mut Value) {
    let _response = match value {
      Value::None => self.ui.label("None"),
      Value::Identifier(_) => todo!(),
      Value::Boolean(x) => self.ui.checkbox(x, ""),
      Value::Enum(_, _) => todo!(),
      Value::Number(x) => match x {
        Number::Integer(x) => self.ui.add(CustomDragValue::new(x)),
        Number::Float(x) => self.ui.add(CustomDragValue::new(x)),
        Number::Hexadecimal(_x) => todo!(),
      },
      Value::String(x) => {
        // // if long we should use a multiline text editor
        // // if short we should use a single line text editor
        // let mut buffer = MutVarTextBuffer(x);
        // if x.len() > 16 {
        //   self.ui.text_edit_multiline(x)
        // } else {
        //   self.ui.text_edit_singleline(x)
        // }
        todo!()
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
  }

  fn visit_metadata(&mut self, metadata: &mut Metadata) {}
}
