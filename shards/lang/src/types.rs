use core::convert::TryInto;
use pest::{iterators::Pair, Parser, Position};
use serde::{Deserialize, Serialize};
use shards::types::{ShardRef, Var, Wire, WireRef};

#[derive(Parser)]
#[grammar = "shards.pest"]
pub struct ShardsParser;

#[derive(Serialize, Deserialize, Debug, Copy, Clone, Default)]
pub struct LineInfo {
  pub line: usize,
  pub column: usize,
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
      loc: LineInfo { line, column },
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
    ShardsError {
      message: message,
      loc: pos,
    }
  }
}

impl<'a> Into<LineInfo> for Position<'a> {
  fn into(self) -> LineInfo {
    let line = self.line_col().0;
    let column = self.line_col().1;
    LineInfo { line, column }
  }
}

#[derive(Serialize, Deserialize, Debug)]
pub enum Value {
  None,
  Identifier(String),
  Boolean(bool),
  Number(Number),
  String(String),
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
  Shards(Sequence),
  EvalExpr(Sequence),
  Expr(Sequence),
  TakeTable(String, Vec<String>),
  TakeSeq(String, Vec<u32>),
}

#[derive(Serialize, Deserialize, Debug)]
pub enum Number {
  Integer(i64),
  Float(f64),
  Hexadecimal(String),
}

#[derive(Serialize, Deserialize, Debug)]
pub struct Shard {
  pub name: String,
  pub params: Option<Vec<Param>>,
}

#[derive(Serialize, Deserialize, Debug)]
pub struct Param {
  pub name: Option<String>,
  pub value: Value,
}

#[derive(Serialize, Deserialize, Debug)]
pub enum Statement {
  Assignment(Assignment),
  Pipeline(Pipeline),
}

#[derive(Serialize, Deserialize, Debug)]
pub struct Sequence {
  pub statements: Vec<Statement>,
}

#[derive(Serialize, Deserialize, Debug)]
pub enum Assignment {
  AssignRef(Pipeline, String),
  AssignSet(Pipeline, String),
  AssignUpd(Pipeline, String),
  AssignPush(Pipeline, String),
}

#[derive(Serialize, Deserialize, Debug)]
pub enum Operator {
  Add,
  Sub,
  Mul,
  Div,
  Mod,
  Pow,
  MatMul,
}

#[derive(Serialize, Deserialize, Debug)]
pub enum BlockContent {
  Shard(Shard),
  Const(Value),
  TakeTable(String, Vec<String>),
  TakeSeq(String, Vec<u32>),
  Operator(Operator, Value),
}

#[derive(Serialize, Deserialize, Debug)]
pub enum Block {
  BlockContent(BlockContent, LineInfo),
  EvalExpr(Sequence),
  Expr(Sequence),
}

#[derive(Serialize, Deserialize, Debug)]
pub struct Pipeline {
  pub blocks: Vec<Block>,
}
