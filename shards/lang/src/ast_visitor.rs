use crate::ast::*;

pub trait AstVisitor {
  fn visit_program(&mut self, program: &Program);
  fn visit_sequence(&mut self, sequence: &Sequence);
  fn visit_statement(&mut self, statement: &Statement);
  fn visit_assignment(&mut self, assignment: &Assignment);
  fn visit_pipeline(&mut self, pipeline: &Pipeline);
  fn visit_block(&mut self, block: &Block);
  fn visit_function(&mut self, function: &Function);
  fn visit_param(&mut self, param: &Param);
  fn visit_identifier(&mut self, identifier: &Identifier);
  fn visit_value(&mut self, value: &Value);
  fn visit_metadata(&mut self, metadata: &Metadata);
}

impl Program {
  pub fn accept<V: AstVisitor>(&self, visitor: &mut V) {
    visitor.visit_program(self);
  }
}

impl Sequence {
  pub fn accept<V: AstVisitor>(&self, visitor: &mut V) {
    visitor.visit_sequence(self);
  }
}

impl Statement {
  pub fn accept<V: AstVisitor>(&self, visitor: &mut V) {
    visitor.visit_statement(self);
  }
}

impl Assignment {
  pub fn accept<V: AstVisitor>(&self, visitor: &mut V) {
    visitor.visit_assignment(self);
  }
}

impl Pipeline {
  pub fn accept<V: AstVisitor>(&self, visitor: &mut V) {
    visitor.visit_pipeline(self);
  }
}

impl Block {
  pub fn accept<V: AstVisitor>(&self, visitor: &mut V) {
    visitor.visit_block(self);
  }
}

impl Function {
  pub fn accept<V: AstVisitor>(&self, visitor: &mut V) {
    visitor.visit_function(self);
  }
}

impl Param {
  pub fn accept<V: AstVisitor>(&self, visitor: &mut V) {
    visitor.visit_param(self);
  }
}

impl Identifier {
  pub fn accept<V: AstVisitor>(&self, visitor: &mut V) {
    visitor.visit_identifier(self);
  }
}

impl Value {
  pub fn accept<V: AstVisitor>(&self, visitor: &mut V) {
    visitor.visit_value(self);
  }
}

impl Metadata {
  pub fn accept<V: AstVisitor>(&self, visitor: &mut V) {
    visitor.visit_metadata(self);
  }
}

pub enum BlockAction {
  Keep,
  Remove,
  Duplicate,
  Swap(Block)
}

pub trait AstMutator<R> {
  fn visit_program(&mut self, program: &mut Program) -> R;
  fn visit_sequence(&mut self, sequence: &mut Sequence) -> R;
  fn visit_statement(&mut self, statement: &mut Statement) -> R;
  fn visit_assignment(&mut self, assignment: &mut Assignment) -> R;
  fn visit_pipeline(&mut self, pipeline: &mut Pipeline) -> R;
  fn visit_block(&mut self, block: &mut Block) -> (BlockAction, R);
  fn visit_function(&mut self, function: &mut Function) -> R;
  fn visit_param(&mut self, param: &mut Param) -> R;
  fn visit_identifier(&mut self, identifier: &mut Identifier) -> R;
  fn visit_value(&mut self, value: &mut Value) -> R;
  fn visit_metadata(&mut self, metadata: &mut Metadata) -> R;
}

impl Program {
  pub fn accept_mut<V: AstMutator<R>, R>(&mut self, visitor: &mut V) -> R {
    visitor.visit_program(self)
  }
}

impl Sequence {
  pub fn accept_mut<V: AstMutator<R>, R>(&mut self, visitor: &mut V) -> R {
    visitor.visit_sequence(self)
  }
}

impl Statement {
  pub fn accept_mut<V: AstMutator<R>, R>(&mut self, visitor: &mut V) -> R {
    visitor.visit_statement(self)
  }
}

impl Assignment {
  pub fn accept_mut<V: AstMutator<R>, R>(&mut self, visitor: &mut V) -> R {
    visitor.visit_assignment(self)
  }
}

impl Pipeline {
  pub fn accept_mut<V: AstMutator<R>, R>(&mut self, visitor: &mut V) -> R {
    visitor.visit_pipeline(self)
  }
}

impl Block {
  pub fn accept_mut<V: AstMutator<R>, R>(&mut self, visitor: &mut V) -> (BlockAction, R) {
    visitor.visit_block(self)
  }
}

impl Function {
  pub fn accept_mut<V: AstMutator<R>, R>(&mut self, visitor: &mut V) -> R {
    visitor.visit_function(self)
  }
}

impl Param {
  pub fn accept_mut<V: AstMutator<R>, R>(&mut self, visitor: &mut V) -> R {
    visitor.visit_param(self)
  }
}

impl Identifier {
  pub fn accept_mut<V: AstMutator<R>, R>(&mut self, visitor: &mut V) -> R {
    visitor.visit_identifier(self)
  }
}

impl Value {
  pub fn accept_mut<V: AstMutator<R>, R>(&mut self, visitor: &mut V) -> R {
    visitor.visit_value(self)
  }
}

impl Metadata {
  pub fn accept_mut<V: AstMutator<R>, R>(&mut self, visitor: &mut V) -> R {
    visitor.visit_metadata(self)
  }
}
