#![allow(non_upper_case_globals)]

use std::any::Any;
use std::fmt::Debug;

use pest::Position;
use serde::{Deserialize, Serialize};
use shards::{
  types::Var, SHType_Bool, SHType_Bytes, SHType_Float, SHType_Int, SHType_None, SHType_String,
};

use crate::{RcBytesWrapper, RcStrWrapper};

#[derive(Parser)]
#[grammar = "shards.pest"]
pub struct ShardsParser;

/// CustomAny trait allows for flexible, type-erased storage of custom state within AST nodes.
/// This approach was chosen to balance performance, flexibility, and simplicity.
///
/// Benefits:
/// - Direct access to custom state without additional lookups
/// - Simplifies state management by keeping it close to related data
/// - Minimizes memory overhead through use of Box<dyn CustomAny>
///
/// Considerations:
/// - Adds non-structural data to the AST
/// - May require special handling during serialization/deserialization
///
/// Alternative considered: External state storage with UUID references in AST nodes.
/// Current approach preferred due to reduced lookup overhead and simpler implementation.
pub trait CustomAny: Any + Debug + CustomClone + CustomPartialEq {
  fn as_any(&self) -> &dyn Any;
  fn as_any_mut(&mut self) -> &mut dyn Any;
}

pub trait CustomClone {
  fn clone_box(&self) -> Box<dyn CustomAny>;
}

impl Clone for Box<dyn CustomAny> {
  fn clone(&self) -> Box<dyn CustomAny> {
    self.clone_box()
  }
}

pub trait CustomPartialEq {
  fn eq_box(&self, other: &dyn CustomAny) -> bool;
}

impl PartialEq for Box<dyn CustomAny> {
  fn eq(&self, other: &Self) -> bool {
    self.eq_box(other.as_ref())
  }
}

// Macro to implement CustomAny for common types
#[macro_export]
macro_rules! impl_custom_any {
  ($($t:ty),*) => {
      $(
          impl CustomClone for $t {
              fn clone_box(&self) -> Box<dyn CustomAny> {
                  Box::new(self.clone())
              }
          }

          impl CustomPartialEq for $t {
              fn eq_box(&self, other: &dyn CustomAny) -> bool {
                  other
                      .as_any()
                      .downcast_ref::<$t>()
                      .map_or(false, |a| self == a)
              }
          }

          impl CustomAny for $t {
              fn as_any(&self) -> &dyn Any {
                  self
              }

              fn as_any_mut(&mut self) -> &mut dyn Any {
                self
              }
          }
      )*
  };
}

macro_rules! impl_custom_state {
  ($type:ident) => {
    impl $type {
      /// Sets custom state for this item.
      /// This method allows attaching arbitrary data to AST nodes,
      /// enabling flexible extensions without modifying the core AST structure.
      pub fn set_custom_state<T: 'static + CustomAny>(&mut self, state: T) {
        self.custom_state = Some(Box::new(state));
      }

      /// Gets a mutable reference to the custom state of a specific type, if it exists.
      pub fn get_custom_state<T: 'static + CustomAny>(&mut self) -> Option<&mut T> {
        self
          .custom_state
          .as_mut()
          .and_then(|state| state.as_any_mut().downcast_mut::<T>())
      }

      /// Uses a custom state of a specific type, if it exists within a closure.
      /// This method simplifies working with custom state by providing a safe, ergonomic API.
      pub fn with_custom_state<T: 'static, R, F>(&mut self, f: F) -> Option<R>
      where
        T: CustomAny,
        F: FnOnce(&mut T) -> R,
      {
        self.get_custom_state::<T>().map(|state| f(state))
      }

      /// Gets or inserts custom state of a specific type.
      /// If the state doesn't exist, it's created using the provided default function.
      pub fn get_or_insert_custom_state<T: 'static + CustomAny, F>(&mut self, default: F) -> &mut T
      where
        F: FnOnce() -> T,
      {
        if self.custom_state.is_none() {
          self.custom_state = Some(Box::new(default()));
        }

        self.get_custom_state().unwrap()
      }

      /// Clears all custom state from this item.
      /// Useful when a "clean" AST representation is needed, free of runtime-specific data.
      pub fn clear_custom_state(&mut self) {
        self.custom_state = None;
      }
    }
  };
}

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

  /// Custom state for UI or other runtime-specific data.
  /// Stored directly in the AST node for efficient access and simpler management.
  /// Uses Box<dyn CustomAny> to minimize memory overhead when unused.
  #[serde(skip)]
  pub custom_state: Option<Box<dyn CustomAny>>,
}

impl_custom_state!(Param);

#[derive(Serialize, Deserialize, Debug, Clone, PartialEq)]
pub struct Function {
  pub name: Identifier,
  pub params: Option<Vec<Param>>,

  /// Custom state for UI or other runtime-specific data.
  /// Stored directly in the AST node for efficient access and simpler management.
  /// Uses Box<dyn CustomAny> to minimize memory overhead when unused.
  #[serde(skip)]
  pub custom_state: Option<Box<dyn CustomAny>>,
}

impl_custom_state!(Function);

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

  /// Custom state for UI or other runtime-specific data.
  /// Stored directly in the AST node for efficient access and simpler management.
  /// Uses Box<dyn CustomAny> to minimize memory overhead when unused.
  #[serde(skip)]
  pub custom_state: Option<Box<dyn CustomAny>>,
}

impl_custom_state!(Block);

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

  /// Custom state for UI or other runtime-specific data.
  /// Stored directly in the AST node for efficient access and simpler management.
  /// Uses Box<dyn CustomAny> to minimize memory overhead when unused.
  #[serde(skip)]
  pub custom_state: Option<Box<dyn CustomAny>>,
}

impl_custom_state!(Sequence);

#[derive(Serialize, Deserialize, Debug, Clone, PartialEq)]
pub struct Program {
  pub sequence: Sequence,
  pub metadata: Metadata,
}

pub trait RewriteFunction {
  fn rewrite_function(&self, function: &Function) -> Option<Function>;
}
