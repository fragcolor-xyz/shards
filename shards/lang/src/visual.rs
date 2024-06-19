use std::rc::Rc;

use egui::*;

use crate::{ast::*, ast_visitor::AstVisitor};

struct VisualAst {
  response: Response,
}

impl VisualAst {
  fn render(&mut self, ast: &Sequence, ui: &mut Ui) -> Response {
    ast.accept(self);
    self.response.clone() // Assuming Response is Clone
  }
}

impl AstVisitor for VisualAst {
  fn visit_program(&mut self, program: &Program) {
    todo!()
  }

  fn visit_sequence(&mut self, sequence: &Sequence) {
    todo!()
  }

  fn visit_statement(&mut self, statement: &Statement) {
    todo!()
  }

  fn visit_assignment(&mut self, assignment: &Assignment) {
    todo!()
  }

  fn visit_pipeline(&mut self, pipeline: &Pipeline) {
    todo!()
  }

  fn visit_block(&mut self, block: &Block) {
    todo!()
  }

  fn visit_function(&mut self, function: &Function) {
    todo!()
  }

  fn visit_param(&mut self, param: &Param) {
    todo!()
  }

  fn visit_identifier(&mut self, identifier: &Identifier) {
    todo!()
  }

  fn visit_value(&mut self, value: &Value) {
    todo!()
  }

  fn visit_metadata(&mut self, metadata: &Metadata) {
    todo!()
  }
}
