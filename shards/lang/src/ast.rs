#![allow(non_upper_case_globals)]

use pest::Position;
use serde::{Deserialize, Serialize};
use shards::{
  types::Var, SHType_Bool, SHType_Bytes, SHType_Float, SHType_Int, SHType_None, SHType_String,
};

use crate::{RcBytesWrapper, RcStrWrapper};

#[derive(Parser)]
#[grammar = "shards.pest"]
pub struct ShardsParser;

#[derive(Serialize, Deserialize, Debug, Copy, Clone, Default, PartialEq)]
pub struct LineInfo {
  pub line: u32,
  pub column: u32,
}

#[derive(Debug)]
pub struct ShardsError {
  pub message: String,
  pub loc: LineInfo,
}

impl<'a> Into<ShardsError> for (&str, Position<'a>) {
  fn into(self) -> ShardsError {
    let (message, pos) = self;
    let line = pos.line_col().0;
    let column = pos.line_col().1;
    ShardsError {
      message: message.to_string(),
      loc: LineInfo {
        line: line as u32,
        column: column as u32,
      },
    }
  }
}

impl<'a> Into<ShardsError> for (String, Position<'a>) {
  fn into(self) -> ShardsError {
    let (message, pos) = self;
    let line = pos.line_col().0;
    let column = pos.line_col().1;
    ShardsError {
      message,
      loc: LineInfo {
        line: line as u32,
        column: column as u32,
      },
    }
  }
}

impl<'a> Into<ShardsError> for (&str, LineInfo) {
  fn into(self) -> ShardsError {
    let (message, pos) = self;
    ShardsError {
      message: message.to_string(),
      loc: pos,
    }
  }
}

impl<'a> Into<ShardsError> for (String, LineInfo) {
  fn into(self) -> ShardsError {
    let (message, pos) = self;
    ShardsError { message, loc: pos }
  }
}

impl<'a> Into<LineInfo> for Position<'a> {
  fn into(self) -> LineInfo {
    let line = self.line_col().0;
    let column = self.line_col().1;
    LineInfo {
      line: line as u32,
      column: column as u32,
    }
  }
}

impl Into<(u32, u32)> for LineInfo {
  fn into(self) -> (u32, u32) {
    (self.line, self.column)
  }
}

#[derive(Serialize, Deserialize, Debug, Clone, PartialEq)]
pub enum Number {
  Integer(i64),
  Float(f64),
  Hexadecimal(RcStrWrapper),
}

#[derive(Serialize, Deserialize, Debug, Clone, PartialEq, Hash, Eq)]
pub struct Identifier {
  pub name: RcStrWrapper,
  pub namespaces: Vec<RcStrWrapper>,
}

impl Identifier {
  pub fn resolve(&self) -> RcStrWrapper {
    if self.namespaces.is_empty() {
      return self.name.clone();
    } else {
      // go thru all namespaces and concatenate them with "/" finally add name
      let mut result = String::new();
      for namespace in &self.namespaces {
        result.push_str(&namespace);
        result.push('/');
      }
      result.push_str(&self.name);
      result.into()
    }
  }
}

#[derive(Serialize, Deserialize, Debug, Clone, PartialEq)]
pub enum Value {
  None,
  Identifier(Identifier),
  Boolean(bool),
  Enum(RcStrWrapper, RcStrWrapper),
  Number(Number),
  String(RcStrWrapper),
  Bytes(RcBytesWrapper),
  Int2([i64; 2]),
  Int3([i32; 3]),
  Int4([i32; 4]),
  Int8([i16; 8]),
  Int16([i8; 16]),
  Float2([f64; 2]),
  Float3([f32; 3]),
  Float4([f32; 4]),
  Seq(Vec<Value>),
  Table(Vec<(Value, Value)>),
  Shard(Function),
  Shards(Sequence),
  EvalExpr(Sequence),
  Expr(Sequence),
  TakeTable(Identifier, Vec<RcStrWrapper>),
  TakeSeq(Identifier, Vec<u32>),
  Func(Function),
}

impl TryFrom<Var> for Value {
  type Error = &'static str;

  fn try_from(value: Var) -> Result<Self, Self::Error> {
    match value.valueType {
      SHType_None => Ok(Value::None),
      SHType_Bool => Ok(Value::Boolean(value.as_ref().try_into().unwrap())),
      SHType_Int => Ok(Value::Number(Number::Integer(
        value.as_ref().try_into().unwrap(),
      ))),
      SHType_Float => Ok(Value::Number(Number::Float(
        value.as_ref().try_into().unwrap(),
      ))),
      SHType_String => {
        let s: &str = value.as_ref().try_into().unwrap();
        Ok(Value::String(s.into()))
      }
      SHType_Bytes => {
        let b: &[u8] = value.as_ref().try_into().unwrap();
        Ok(Value::Bytes(b.into()))
      }
      _ => Err("Unsupported type"),
    }
  }
}

impl Value {
  pub fn get_identifier(&self) -> Option<&Identifier> {
    match self {
      Value::Identifier(identifier) => Some(identifier),
      _ => None,
    }
  }
}

#[derive(Serialize, Deserialize, Debug, Clone, PartialEq)]
pub struct Param {
  pub name: Option<RcStrWrapper>,
  pub value: Value,
}

#[derive(Serialize, Deserialize, Debug, Clone, PartialEq)]
pub struct Function {
  pub name: Identifier,
  pub params: Option<Vec<Param>>,
}

#[derive(Serialize, Deserialize, Debug, Clone, PartialEq)]
pub enum BlockContent {
  Empty,
  Shard(Function),                          // Rule: Shard
  Shards(Sequence),                         // Rule: Shards
  Const(Value),                             // Rules: ConstValue, Vector
  TakeTable(Identifier, Vec<RcStrWrapper>), // Rule: TakeTable
  TakeSeq(Identifier, Vec<u32>),            // Rule: TakeSeq
  EvalExpr(Sequence),                       // Rule: EvalExpr
  Expr(Sequence),                           // Rule: Expr
  Func(Function),                           // Rule: BuiltIn
  Program(Program), // @include files, this is a sequence that will include itself when evaluated
}

#[derive(Serialize, Deserialize, Debug, Clone, PartialEq)]
pub struct Block {
  pub content: BlockContent,
  pub line_info: Option<LineInfo>,
}

#[derive(Serialize, Deserialize, Debug, Clone, PartialEq)]
pub struct Pipeline {
  pub blocks: Vec<Block>,
}

#[derive(Serialize, Deserialize, Debug, Clone, PartialEq)]
pub enum Assignment {
  AssignRef(Pipeline, Identifier),
  AssignSet(Pipeline, Identifier),
  AssignUpd(Pipeline, Identifier),
  AssignPush(Pipeline, Identifier),
}

#[derive(Serialize, Deserialize, Debug, Clone, PartialEq)]
pub enum Statement {
  Assignment(Assignment),
  Pipeline(Pipeline),
}

#[derive(Serialize, Deserialize, Debug, Clone, PartialEq)]
pub struct Metadata {
  pub name: RcStrWrapper,
}

#[derive(Serialize, Deserialize, Debug, Clone, PartialEq)]
pub struct Sequence {
  pub statements: Vec<Statement>,
}

#[derive(Serialize, Deserialize, Debug, Clone, PartialEq)]
pub struct Program {
  pub sequence: Sequence,
  pub metadata: Metadata,
}

pub trait ReplaceExpression {
  fn replace_expression<F>(&mut self, f: &F)
  where
    F: Fn(&Value) -> Option<Value>;
}

impl ReplaceExpression for Value {
  fn replace_expression<F>(&mut self, f: &F)
  where
    F: Fn(&Value) -> Option<Value>,
  {
    if let Some(new_value) = f(self) {
      *self = new_value;
    } else {
      match self {
        Value::Seq(seq) => {
          for val in seq {
            val.replace_expression(f);
          }
        }
        Value::Table(table) => {
          for (key, val) in table {
            key.replace_expression(f);
            val.replace_expression(f);
          }
        }
        Value::Shard(func) | Value::Func(func) => {
          func.replace_expression(f);
        }
        Value::Shards(seq) | Value::EvalExpr(seq) | Value::Expr(seq) => {
          seq.replace_expression(f);
        }
        _ => {}
      }
    }
  }
}

impl ReplaceExpression for Function {
  fn replace_expression<F>(&mut self, f: &F)
  where
    F: Fn(&Value) -> Option<Value>,
  {
    if let Some(params) = &mut self.params {
      for param in params {
        param.value.replace_expression(f);
      }
    }
  }
}

impl ReplaceExpression for Block {
  fn replace_expression<F>(&mut self, f: &F)
  where
    F: Fn(&Value) -> Option<Value>,
  {
    match &mut self.content {
      BlockContent::Shard(func) | BlockContent::Func(func) => {
        func.replace_expression(f);
      }
      BlockContent::Shards(seq) | BlockContent::EvalExpr(seq) | BlockContent::Expr(seq) => {
        seq.replace_expression(f);
      }
      BlockContent::Const(value) => {
        value.replace_expression(f);
      }
      BlockContent::TakeTable(_, _) | BlockContent::TakeSeq(_, _) => {}
      BlockContent::Program(prog) => {
        prog.sequence.replace_expression(f);
      }
      BlockContent::Empty => {}
    }
  }
}

impl ReplaceExpression for Pipeline {
  fn replace_expression<F>(&mut self, f: &F)
  where
    F: Fn(&Value) -> Option<Value>,
  {
    for block in &mut self.blocks {
      block.replace_expression(f);
    }
  }
}

impl ReplaceExpression for Assignment {
  fn replace_expression<F>(&mut self, f: &F)
  where
    F: Fn(&Value) -> Option<Value>,
  {
    match self {
      Assignment::AssignRef(pipeline, _)
      | Assignment::AssignSet(pipeline, _)
      | Assignment::AssignUpd(pipeline, _)
      | Assignment::AssignPush(pipeline, _) => {
        pipeline.replace_expression(f);
      }
    }
  }
}

impl ReplaceExpression for Statement {
  fn replace_expression<F>(&mut self, f: &F)
  where
    F: Fn(&Value) -> Option<Value>,
  {
    match self {
      Statement::Assignment(assignment) => {
        assignment.replace_expression(f);
      }
      Statement::Pipeline(pipeline) => {
        pipeline.replace_expression(f);
      }
    }
  }
}

impl ReplaceExpression for Sequence {
  fn replace_expression<F>(&mut self, f: &F)
  where
    F: Fn(&Value) -> Option<Value>,
  {
    for statement in &mut self.statements {
      statement.replace_expression(f);
    }
  }
}

impl ReplaceExpression for Program {
  fn replace_expression<F>(&mut self, f: &F)
  where
    F: Fn(&Value) -> Option<Value>,
  {
    self.sequence.replace_expression(f);
  }
}

pub fn replace_in_sequence<F>(sequence: &mut Sequence, replacer: F)
where
  F: Fn(&Value) -> Option<Value>,
{
  profiling::scope!("replace_in_sequence");
  sequence.replace_expression(&replacer);
}

pub fn replace_in_program<F>(program: &mut Program, replacer: F)
where
  F: Fn(&Value) -> Option<Value>,
{
  profiling::scope!("replace_in_program");
  program.replace_expression(&replacer);
}

#[test]
fn test1() {
  let identifier = Identifier {
    name: RcStrWrapper::from("test"),
    namespaces: vec![],
  };

  let function = Function {
    name: identifier.clone(),
    params: Some(vec![Param {
      name: None,
      value: Value::None,
    }]),
  };

  let block = Block {
    content: BlockContent::Shard(function.clone()),
    line_info: None,
  };

  let pipeline = Pipeline {
    blocks: vec![block.clone()],
  };

  let assignment = Assignment::AssignRef(pipeline.clone(), identifier.clone());

  let statement = Statement::Assignment(assignment);

  let sequence = Sequence {
    statements: vec![statement],
  };

  let mut program = Program {
    sequence,
    metadata: Metadata {
      name: RcStrWrapper::from("example"),
    },
  };

  replace_in_program(&mut program, |value| {
    if *value == Value::None {
      Some(Value::Boolean(true))
    } else {
      None
    }
  });

  println!("{:?}", program);
}

#[test]
fn test2() {
  // Create a deeply nested structure
  let identifier = Identifier {
      name: RcStrWrapper::from("test"),
      namespaces: vec![],
  };

  let function = Function {
      name: identifier.clone(),
      params: Some(vec![Param {
          name: None,
          value: Value::None,
      }]),
  };

  let block = Block {
      content: BlockContent::Shard(function.clone()),
      line_info: None,
  };

  let pipeline = Pipeline {
      blocks: vec![block.clone()],
  };

  let assignment = Assignment::AssignRef(pipeline.clone(), identifier.clone());

  let statement = Statement::Assignment(assignment.clone());

  let nested_sequence = Sequence {
      statements: vec![
          Statement::Pipeline(pipeline.clone()),
          Statement::Assignment(assignment.clone()),
          Statement::Pipeline(Pipeline {
              blocks: vec![
                  Block {
                      content: BlockContent::Shards(Sequence {
                          statements: vec![statement.clone()],
                      }),
                      line_info: None,
                  },
              ],
          }),
      ],
  };

  let mut sequence = Sequence {
      statements: vec![
          statement.clone(),
          Statement::Pipeline(pipeline.clone()),
          Statement::Pipeline(Pipeline {
              blocks: vec![
                  Block {
                      content: BlockContent::Shards(nested_sequence.clone()),
                      line_info: None,
                  },
              ],
          }),
      ],
  };

  println!("Original sequence: {:?}", sequence);

  replace_in_sequence(&mut sequence, |value| {
      if *value == Value::None {
          Some(Value::Boolean(true))
      } else {
          None
      }
  });

  println!("Modified sequence: {:?}", sequence);
}